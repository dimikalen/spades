#ifndef ABSTRACT_CONJUGATE_GRAPH_HPP_
#define ABSTRACT_CONJUGATE_GRAPH_HPP_

#include <vector>
#include <set>
#include <cstring>
#include "sequence/seq.hpp"
#include "sequence/sequence.hpp"
#include "logging.hpp"
#include "sequence/nucl.hpp"
//#include "strobe_read.hpp"
#include "io/paired_read.hpp"
#include "omni_utils.hpp"
#include "abstract_graph.hpp"
#include "coverage.hpp"

namespace omnigraph {

template<class DataMaster>
class AbstractConjugateGraph;

template<class DataMaster>
class PairedEdge;

template<class DataMaster>
class PairedVertex {
private:
	typedef PairedVertex<DataMaster>* VertexId;
	typedef PairedEdge<DataMaster>* EdgeId;
	typedef typename DataMaster::VertexData VertexData;

	friend class AbstractConjugateGraph<DataMaster> ;

	vector<EdgeId> outgoing_edges_;

	VertexId conjugate_;

	VertexData data_;

	void set_conjugate(VertexId conjugate) {
		conjugate_ = conjugate;
	}

	size_t OutgoingEdgeCount() const {
		return outgoing_edges_.size();
	}

	const vector<EdgeId> OutgoingEdges() const {
		return outgoing_edges_;
	}

	const vector<EdgeId> OutgoingEdgesTo(VertexId v) const {
		vector<EdgeId> result;
		for (auto it = outgoing_edges_.begin(); it != outgoing_edges_.end(); ++it) {
			if ((*it)->end() == v) {
				result.push_back(*it);
			}
		}
		return result;
	}

	size_t IncomingEdgeCount() const {
		return conjugate_->OutgoingEdgeCount();
	}

	const vector<EdgeId> IncomingEdges() const {
		vector<EdgeId> result = conjugate_->OutgoingEdges();
		for (size_t i = 0; i < result.size(); i++) {
			result[i] = result[i]->conjugate();
		}
		return result;
	}

	PairedVertex(VertexData data) :
		data_(data) {
	}

	VertexData &data() {
		return data_;
	}

	void set_data(VertexData data) {
		data_ = data;
	}

//	bool IsDeadend() {
//		return outgoing_edges_.size() == 0;
//	}

	void AddOutgoingEdge(EdgeId e) {
		outgoing_edges_.push_back(e);
	}

	bool RemoveOutgoingEdge(const EdgeId e) {
		auto it = outgoing_edges_.begin();
		while (it != outgoing_edges_.end() && *it != e) {
			++it;
		}
		if (it == outgoing_edges_.end()) {
			return false;
		}
		outgoing_edges_.erase(it);
		return true;
	}

	VertexId conjugate() const {
		return conjugate_;
	}

	~PairedVertex() {
		TRACE("PairedVertex destructor");
		assert(outgoing_edges_.size() == 0);
		TRACE("PairedVertex destructor ok");
	}
};

template<class DataMaster>
class PairedEdge {
private:
	typedef PairedVertex<DataMaster>* VertexId;
	typedef PairedEdge<DataMaster>* EdgeId;
	typedef typename DataMaster::EdgeData EdgeData;
	friend class AbstractConjugateGraph<DataMaster> ;
	//todo unfriend
	friend class PairedVertex<DataMaster> ;
	VertexId end_;

	EdgeData data_;

	EdgeId conjugate_;

	PairedEdge(VertexId end, const EdgeData &data) :
		end_(end), data_(data) {
	}

	EdgeData &data() {
		return data_;
	}

	void set_data(const EdgeData &data) {
		data_ = data;
	}

	VertexId end() const {
		return end_;
	}

	void set_conjugate(EdgeId conjugate) {
		conjugate_ = conjugate;
	}

	~PairedEdge() {
	}

public:
	EdgeId conjugate() {
		return conjugate_;
	}
};

template<class DataMaster>
class AbstractConjugateGraph:
	public AbstractGraph<PairedVertex<DataMaster>*, PairedEdge<DataMaster>*
	, DataMaster, typename set<PairedVertex<DataMaster>*>::const_iterator> {
	typedef AbstractGraph<PairedVertex<DataMaster>*, PairedEdge<DataMaster>*
			, DataMaster, typename set<PairedVertex<DataMaster>*>::const_iterator> base;

public:
	//todo remove unused typedefs
//	typedef typename base::SmartVertexIt SmartVertexIt;
//	typedef typename base::SmartEdgeIt SmartEdgeIt;

	typedef typename base::VertexId VertexId;
	typedef typename base::EdgeId EdgeId;
	typedef typename base::VertexData VertexData;
	typedef typename base::EdgeData EdgeData;
	typedef typename base::VertexIterator VertexIterator;

private:
	typedef set<VertexId> Vertices;

	Vertices vertices_;

	VertexId HiddenAddVertex(const VertexData &data1, const VertexData &data2) {
		VertexId v1 =
				new PairedVertex<DataMaster> (data1);
		VertexId v2 =
				new PairedVertex<DataMaster> (data2);
		v1->set_conjugate(v2);
		v2->set_conjugate(v1);
		vertices_.insert(v1);
		vertices_.insert(v2);
//		cout << "add v " << v1 << " " << v2 << endl;
		return v1;
	}

	virtual VertexId HiddenAddVertex(const VertexData &data) {
		return HiddenAddVertex(data, this->master().conjugate(data));
	}


	virtual void HiddenDeleteVertex(VertexId v) {
		TRACE("ab_conj DeleteVertex "<<v);
		VertexId conjugate = v->conjugate();
		TRACE("ab_conj DeleteVertex "<<v<<" and conj "<<conjugate);
		vertices_.erase(v);
		TRACE("ab_conj delete "<<v);
		delete v;
		TRACE("ab_conj erase "<<conjugate);
		vertices_.erase(conjugate);
		TRACE("ab_conj delete "<<conjugate);
		delete conjugate;
		TRACE("ab_conj delete FINISHED");
//		cout << "del v " << v << " " << conjugate << endl;
	}

	virtual EdgeId HiddenAddEdge(VertexId v1, VertexId v2, const EdgeData &data) {
		assert(vertices_.find(v1) != vertices_.end() && vertices_.find(v2) != vertices_.end());
		EdgeId result = AddSingleEdge(v1, v2, data);
		if (this->master().isSelfConjugate(data)) {
			result->set_conjugate(result);
			return result;
		}
		EdgeId rcEdge = AddSingleEdge(v2->conjugate(), v1->conjugate(),
				this->master().conjugate(data));
		result->set_conjugate(rcEdge);
		rcEdge->set_conjugate(result);
//		cout << "add e" << result << " " << rcEdge << endl;
		return result;
	}

	virtual void HiddenDeleteEdge(EdgeId edge) {
		EdgeId rcEdge = conjugate(edge);
		VertexId rcStart = conjugate(edge->end());
		VertexId start = conjugate(rcEdge->end());
		start->RemoveOutgoingEdge(edge);
		rcStart->RemoveOutgoingEdge(rcEdge);
		if (edge != rcEdge) {
			delete rcEdge;
		}
		delete edge;
//		cout << "del e" << edge << " " << rcEdge << endl;
	}

	virtual vector<EdgeId> CorrectMergePath(const vector<EdgeId>& path) {
		for (size_t i = 0; i < path.size(); i++) {
			if (path[i] == conjugate(path[i])) {
				vector<EdgeId> result;
				if (i < path.size() - 1 - i) {
					for (size_t j = 0; j < path.size(); j++)
						result.push_back(conjugate(path[path.size() - 1 - j]));
					i = path.size() - 1 - i;
				} else {
					result = path;
				}
				size_t size = 2 * i + 1;
				for (size_t j = result.size(); j < size; j++) {
					result.push_back(conjugate(result[size - 1 - j]));
				}
				return result;
			}
		}
		return path;
	}

	virtual vector<EdgeId> EdgesToDelete(const vector<EdgeId> &path) {
		set<EdgeId> edgesToDelete;
		edgesToDelete.insert(path[0]);
		for (size_t i = 0; i + 1 < path.size(); i++) {
			EdgeId e = path[i + 1];
			if (edgesToDelete.find(conjugate(e)) == edgesToDelete.end())
				edgesToDelete.insert(e);
		}
		return vector<EdgeId>(edgesToDelete.begin(), edgesToDelete.end());
	}

	virtual vector<VertexId> VerticesToDelete(const vector<EdgeId> &path) {
		set<VertexId> verticesToDelete;
		for (size_t i = 0; i + 1 < path.size(); i++) {
			EdgeId e = path[i + 1];
			VertexId v = EdgeStart(e);
			if (verticesToDelete.find(conjugate(v)) == verticesToDelete.end())
				verticesToDelete.insert(v);
		}
		return vector<VertexId>(verticesToDelete.begin(), verticesToDelete.end());
	}

	EdgeId AddSingleEdge(VertexId v1, VertexId v2, const EdgeData &data) {
		EdgeId newEdge = new PairedEdge<DataMaster> (v2,
				data);
		v1->AddOutgoingEdge(newEdge);
		return newEdge;
	}

public:

	AbstractConjugateGraph(const DataMaster& master) :
		base(new PairedHandlerApplier<AbstractConjugateGraph>(*this), master)
	{}

	virtual ~AbstractConjugateGraph() {
		TRACE("~AbstractConjugateGraph")
		for (auto it = this->SmartVertexBegin(); !it.IsEnd(); ++it) {
			ForceDeleteVertex(*it);
		}
		TRACE("~AbstractConjugateGraph ok")
	}

	virtual VertexIterator begin() const {
		return vertices_.begin();
	}

	virtual VertexIterator end() const {
		return vertices_.end();
	}

	size_t size() const {
		return vertices_.size();
	}


	virtual const vector<EdgeId> OutgoingEdges(VertexId v) const {
		return v->OutgoingEdges();
	}

	virtual const vector<EdgeId> IncomingEdges(VertexId v) const {
		return v->IncomingEdges();
	}

	virtual size_t OutgoingEdgeCount(VertexId v) const {
		return v->OutgoingEdgeCount();
	}

	virtual size_t IncomingEdgeCount(VertexId v) const {
		return v->conjugate()->OutgoingEdgeCount();
	}

	virtual vector<EdgeId> GetEdgesBetween(VertexId v, VertexId u) {
		return v->OutgoingEdgesTo(u);
	}

	virtual const EdgeData& data(EdgeId edge) const {
		return edge->data();
	}

	virtual const VertexData& data(VertexId v) const {
		return v->data();
	}

	virtual VertexId EdgeStart(EdgeId edge) const {
		return edge->conjugate()->end()->conjugate();
	}

	virtual VertexId EdgeEnd(EdgeId edge) const {
		return edge->end();
	}

	VertexId conjugate(VertexId v) const {
		return v->conjugate();
	}

	EdgeId conjugate(EdgeId edge) const {
		return edge->conjugate();
	}


	pair<VertexId, vector<pair<EdgeId, EdgeId>>> SplitVertex(VertexId vertex, vector<EdgeId> splittingEdges) {
		vector<double> split_coefficients(splittingEdges.size(),1);
		return SplitVertex(vertex, splittingEdges, split_coefficients);
	}

	pair<VertexId, vector<pair<EdgeId, EdgeId>>> SplitVertex(VertexId vertex, vector<EdgeId> &splittingEdges, vector<double> &split_coefficients) {
//TODO:: check whether we handle loops correctly!
		VertexId newVertex = HiddenAddVertex(vertex->data());
		vector<pair<EdgeId, EdgeId>> edge_clones;
		vector<pair<EdgeId, EdgeId>> rc_edge_clones;

		for (size_t i = 0; i < splittingEdges.size(); i++) {
			VertexId start_v = this->EdgeStart(splittingEdges[i]);
			VertexId start_e = this->EdgeEnd(splittingEdges[i]);
			if (start_v == vertex)
				start_v = newVertex;
			if (start_e == vertex)
				start_e = newVertex;
			EdgeId newEdge = HiddenAddEdge(start_v, start_e, splittingEdges[i]->data());
			edge_clones.push_back(make_pair(splittingEdges[i], newEdge));
			rc_edge_clones.push_back(make_pair((splittingEdges[i])->conjugate(), newEdge->conjugate()));
		}
//FIRE
		FireVertexSplit(newVertex, edge_clones, split_coefficients, vertex);
		FireAddVertex(newVertex);
		for(size_t i = 0; i < splittingEdges.size(); i ++)
			FireAddEdge(edge_clones[i].second);

		FireVertexSplit(newVertex->conjugate(), rc_edge_clones, split_coefficients, vertex->conjugate());
		FireAddVertex(newVertex->conjugate());
		for(size_t i = 0; i < splittingEdges.size(); i ++)
			FireAddEdge(rc_edge_clones[i].second);


		return make_pair(newVertex, edge_clones);
	}

private:
	DECL_LOGGER("AbstractConjugateGraph")
};

}
#endif /* ABSTRUCT_CONJUGATE_GRAPH_HPP_ */
