/*
 * sequence_mapper_notifier.hpp
 *
 *  Created on: Sep 3, 2013
 *      Author: ira
 */

#ifndef SEQUENCE_MAPPER_NOTIFIER_HPP_
#define SEQUENCE_MAPPER_NOTIFIER_HPP_
#include <cstdlib>
#include <vector>
#include "sequence_mapper.hpp"
#include "io/paired_read.hpp"
#include "graph_pack.hpp"

namespace debruijn_graph {
//todo think if we still need all this
class SequenceMapperListener {
public:
    virtual void StartProcessLibrary(size_t threads_count) = 0;
    virtual void StopProcessLibrary() = 0;
    virtual void ProcessPairedRead(size_t thread_index, const MappingPath<EdgeId>& read1, const MappingPath<EdgeId>& read2, size_t dist) = 0;
    virtual void ProcessSingleRead(size_t thread_index, const MappingPath<EdgeId>& read) = 0;
    virtual void MergeBuffer(size_t thread_index) = 0;
    virtual ~SequenceMapperListener(){
    }
};

class SequenceMapperNotifier {
public:
    typedef MapperFactory<conj_graph_pack>::SequenceMapperT SequenceMapperT;
    typedef std::shared_ptr<
            const NewExtendedSequenceMapper<conj_graph_pack::graph_t, conj_graph_pack::index_t> > Mapper;

    SequenceMapperNotifier(const conj_graph_pack& gp)
            : gp_(gp) {
    }

    void Subscribe(size_t lib_index, SequenceMapperListener* listener) {
        while ((int)lib_index >= (int)listeners_.size() - 1) {
            std::vector<SequenceMapperListener*> vect;
            listeners_.push_back(vect);
        }
        listeners_[lib_index].push_back(listener);
    }

    template<class ReadType>
    void ProcessLibrary(io::ReadStreamList<ReadType>& streams,
            size_t lib_index, size_t read_length, size_t threads_count) {
        streams.reset();
        NotifyStartProcessLibrary(lib_index, threads_count);
        MapperFactory<conj_graph_pack> mapper_factory(gp_);
        std::shared_ptr<SequenceMapperT> mapper = mapper_factory.GetSequenceMapper(read_length);

        size_t counter = 0;
        #pragma omp parallel num_threads(threads_count)
        {
            #pragma omp for reduction(+: counter)
            for (size_t ithread = 0; ithread < threads_count; ++ithread) {
                size_t size = 0;
                size_t limit = 1000000;
                ReadType r;
                auto& stream = streams[ithread];
                stream.reset();
                bool end_of_stream = false;
                while (!end_of_stream) {
                    end_of_stream = stream.eof();
                    while (!end_of_stream && size < limit) {
                        stream >> r;
                        ++size;
                        counter++;
                        NotifyProcessRead(r, *mapper, lib_index, ithread);
                        end_of_stream = stream.eof();
                        if (counter % 1000000 == 0) {
                            DEBUG("process " << counter << " reads");
                        }
                    }
                    #pragma omp critical
                    {
                        size = 0;
                        NotifyMergeBuffer(lib_index, ithread);
                    }
                }
            }
        }
        NotifyStopProcessLibrary(lib_index);
    }

private:
    template<class ReadType>
    void NotifyProcessRead(const ReadType& r, const SequenceMapperT& mapper, size_t ilib, size_t ithread) const;

    void NotifyStartProcessLibrary(size_t ilib, size_t thread_count) const {
        for (size_t ilistener = 0; ilistener < listeners_[ilib].size();
                ++ilistener) {
            listeners_[ilib][ilistener]->StartProcessLibrary(thread_count);
        }
    }

    void NotifyStopProcessLibrary(size_t ilib) const {
        for (size_t ilistener = 0; ilistener < listeners_[ilib].size();
                ++ilistener) {
            listeners_[ilib][ilistener]->StopProcessLibrary();
        }
    }

    void NotifyMergeBuffer(size_t ilib, size_t ithread) const {
        for (size_t ilistener = 0; ilistener < listeners_[ilib].size();
                ++ilistener) {
            listeners_[ilib][ilistener]->MergeBuffer(ithread);
        }
    }
    const conj_graph_pack& gp_;
    std::vector<std::vector<SequenceMapperListener*> > listeners_;  //first vector's size = count libs
};

template<>
inline void SequenceMapperNotifier::NotifyProcessRead(const io::PairedReadSeq& r,
                                               const SequenceMapperT& mapper,
                                               size_t ilib,
                                               size_t ithread) const {

    const Sequence read1 = r.first().sequence();
    const Sequence read2 = r.second().sequence();
    MappingPath<EdgeId> path1 = mapper.MapSequence(read1);
    MappingPath<EdgeId> path2 = mapper.MapSequence(read2);
    for (size_t ilistener = 0; ilistener < listeners_[ilib].size();
            ++ilistener) {
        listeners_[ilib][ilistener]->ProcessPairedRead(ithread, path1, path2, r.distance());
        listeners_[ilib][ilistener]->ProcessSingleRead(ithread, path1);
        listeners_[ilib][ilistener]->ProcessSingleRead(ithread, path2);
    }
}
//TODO:delete it
template<>
inline void SequenceMapperNotifier::NotifyProcessRead(const io::PairedRead& r,
                                               const SequenceMapperT& mapper,
                                               size_t ilib,
                                               size_t ithread) const {
    const Sequence read1 = r.first().sequence();
    const Sequence read2 = r.second().sequence();
    MappingPath<EdgeId> path1 = mapper.MapSequence(read1);
    MappingPath<EdgeId> path2 = mapper.MapSequence(read2);
    for (size_t ilistener = 0; ilistener < listeners_[ilib].size();
            ++ilistener) {
        listeners_[ilib][ilistener]->ProcessPairedRead(ithread, path1, path2, r.distance());
        listeners_[ilib][ilistener]->ProcessSingleRead(ithread, path1);
        listeners_[ilib][ilistener]->ProcessSingleRead(ithread, path2);
    }
}

template<>
inline void SequenceMapperNotifier::NotifyProcessRead(const io::SingleReadSeq& r,
                                               const SequenceMapperT& mapper,
                                               size_t ilib,
                                               size_t ithread) const {
    Sequence read = r.sequence();
    MappingPath<EdgeId> path = mapper.MapSequence(read);
    for (size_t ilistener = 0; ilistener < listeners_[ilib].size();
            ++ilistener) {
        listeners_[ilib][ilistener]->ProcessSingleRead(ithread, path);
    }
}

//TODO:delete it
template<>
inline void SequenceMapperNotifier::NotifyProcessRead(const io::SingleRead& r,
                                               const SequenceMapperT& mapper,
                                               size_t ilib,
                                               size_t ithread) const {
    Sequence read = r.sequence();
    MappingPath<EdgeId> path = mapper.MapSequence(read);
    for (size_t ilistener = 0; ilistener < listeners_[ilib].size();
            ++ilistener) {
        listeners_[ilib][ilistener]->ProcessSingleRead(ithread, path);
    }
}

} /*debruijn_graph*/


#endif /* SEQUENCE_MAPPER_NOTIFIER_HPP_ */