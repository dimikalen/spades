/*
 * erroneous_connection_remover.hpp
 *
 *  Created on: May 31, 2011
 *      Author: sergey
 */

#ifndef ERRONEOUS_CONNECTION_REMOVER_HPP_
#define ERRONEOUS_CONNECTION_REMOVER_HPP_

namespace omnigraph {

#include "omni_tools.hpp"
#include "omni_utils.hpp"
#include "xmath.h"

template<class Graph>
class LowCoverageEdgeRemover {
	Graph& g_;
	size_t max_length_;
	double max_coverage_;
public:
	LowCoverageEdgeRemover(Graph& g, size_t max_length, double max_coverage) :
			g_(g), max_length_(max_length), max_coverage_(max_coverage) {

	}

	bool RemoveEdges() {
		bool change = false;
		for (auto it = g_.SmartEdgeBegin(); !it.IsEnd(); ++it) {
			typename Graph::EdgeId e = *it;
			if (g_.length(e) < max_length_ && g_.coverage(e) < max_coverage_) {
				g_.DeleteEdge(e);
				change = true;
			}
		}
		omnigraph::Compressor<Graph> compressor(g_);
		compressor.CompressAllVertices();
		omnigraph::Cleaner<Graph> cleaner(g_);
		cleaner.Clean();
		return change;
	}
};

template<class Graph>
class ErroneousEdgeRemover {
	typedef typename Graph::EdgeId EdgeId;
	typedef typename Graph::VertexId VertexId;
	Graph& g_;
	EdgeRemover<Graph>& edge_remover_;
	bool graph_changed_;
public:
	ErroneousEdgeRemover(Graph& g, EdgeRemover<Graph>& edge_remover) :
			g_(g), edge_remover_(edge_remover), graph_changed_(false) {

	}

	virtual ~ErroneousEdgeRemover() {

	}

	bool RemoveEdges() {
		InnerRemoveEdges();
		omnigraph::Cleaner<Graph> cleaner(g_);
		cleaner.Clean();
		return graph_changed_;
	}

protected:
	void DeleteEdge(EdgeId edge, bool delete_between_related = true) {
		graph_changed_ = edge_remover_.DeleteEdge(edge, delete_between_related)
				|| graph_changed_;
	}

	virtual void InnerRemoveEdges() = 0;

	const Graph& graph() const {
		return g_;
	}
};

template<class Graph>
class ChimericEdgesRemover: public ErroneousEdgeRemover<Graph> {
private:
	typedef typename Graph::EdgeId EdgeId;
	typedef typename Graph::VertexId VertexId;
	typedef ErroneousEdgeRemover<Graph> base;

	size_t max_overlap_;

public:
	ChimericEdgesRemover(Graph &graph, size_t max_overlap,
			EdgeRemover<Graph>& edge_remover) :
			base(graph, edge_remover), max_overlap_(max_overlap) {
	}

	bool CheckEnd(VertexId v) {
		return this->graph().OutgoingEdgeCount(v) == 1
		/*&& graph_.IncomingEdgeCount(v) >= 2*/;
//		return graph_.OutgoingEdgeCount(v) == 1
//				&& graph_.IncomingEdgeCount(v) >= 2;
	}

	bool CheckStart(VertexId v) {
		return /*graph_.OutgoingEdgeCount(v) >= 2
		 &&*/this->graph().IncomingEdgeCount(v) == 1;
//		return graph_.OutgoingEdgeCount(v) >= 2
//				&& graph_.IncomingEdgeCount(v) == 1;
	}

	void InnerRemoveEdges() {
		for (auto it = this->graph().SmartEdgeBegin(); !it.IsEnd(); ++it) {
			EdgeId edge = *it;
			if (this->graph().length(edge) <= this->graph().k()
					&& this->graph().length(edge)
							>= this->graph().k() - max_overlap_
					&& CheckEnd(this->graph().EdgeEnd(edge))
					&& CheckStart(this->graph().EdgeStart(edge))) {
				this->DeleteEdge(edge);
//				graph_.DeleteEdge(edge);
			}
		}
	}
};

template<class Graph>
class IterativeLowCoverageEdgeRemover: public ErroneousEdgeRemover<Graph> {
	typedef typename Graph::EdgeId EdgeId;
	typedef typename Graph::VertexId VertexId;
	typedef ErroneousEdgeRemover<Graph> base;
	size_t max_length_;
	double max_coverage_;

public:
	IterativeLowCoverageEdgeRemover(Graph& g, size_t max_length,
			double max_coverage, EdgeRemover<Graph>& edge_remover) :
			base(g, edge_remover), max_length_(max_length), max_coverage_(
					max_coverage) {

	}

	void InnerRemoveEdges() {
		TRACE("Removing edges")
		CoverageComparator<Graph> comparator(this->graph());
		for (auto it = this->graph().SmartEdgeBegin(comparator); !it.IsEnd();
				++it) {
			typename Graph::EdgeId e = *it;
			TRACE("Considering edge " << e);
			if (math::gr(this->graph().coverage(e), max_coverage_)) {
				TRACE("Max coverage " << max_coverage_ << " achieved");
				return;
			}TRACE("Checking length");
			if (this->graph().length(e) < max_length_) {
				TRACE("Condition ok");
				this->DeleteEdge(e);
//				VertexId start = g_.EdgeStart(e);
//				VertexId end = g_.EdgeEnd(e);
//				TRACE("Start " << start);
//				TRACE("End " << end);
//				TRACE("Deleting edge");
//				g_.DeleteEdge(e);
//				TRACE("Compressing locality");
//				if (!g_.RelatedVertices(start, end)) {
//					TRACE("Vertices not related");
//					TRACE("Compressing end");
//					g_.CompressVertex(end);
//				}TRACE("Compressing start");
//				g_.CompressVertex(start);
			} else {
				TRACE("Condition failed");
			}TRACE("Edge " << e << " processed");
		}
//		TRACE("Cleaning graph");
//		omnigraph::Cleaner<Graph> cleaner(this->graph());
//		cleaner.Clean();
//		TRACE("Graph cleaned");
	}
private:
	DECL_LOGGER("IterativeLowCoverageEdgeRemover")
	;
};

template<class T>
void Append(vector<T>& current, const vector<T>& to_append) {
	current.insert(current.end(), to_append.begin(), to_append.end());
}

template<class Graph>
class TopologyBasedChimericEdgeRemover: public ErroneousEdgeRemover<Graph> {
	typedef typename Graph::EdgeId EdgeId;
	typedef typename Graph::VertexId VertexId;
	typedef ErroneousEdgeRemover<Graph> base;

	size_t max_length_;
	double coverage_gap_;
	size_t neighbour_length_threshold_;

public:
	TopologyBasedChimericEdgeRemover(Graph& g, size_t max_length,
			double coverage_gap, size_t neighbour_length_threshold,
			EdgeRemover<Graph>& edge_remover) :
			base(g, edge_remover), max_length_(max_length), coverage_gap_(
					coverage_gap), neighbour_length_threshold_(
					neighbour_length_threshold) {

	}

	bool StrongNeighbourCondition(EdgeId neighbour_edge, EdgeId possible_ec) {
		return neighbour_edge == possible_ec
				|| math::gr(this->graph().coverage(neighbour_edge),
						this->graph().coverage(possible_ec) * coverage_gap_)
				|| this->graph().length(neighbour_edge)
						>= neighbour_length_threshold_;
	}

	bool CheckAdjacent(const vector<EdgeId>& edges, EdgeId possible_ec) {
		for (auto it = edges.begin(); it != edges.end(); ++it) {
			if (!StrongNeighbourCondition(*it, possible_ec))
				return false;
		}
		return true;
	}

	void InnerRemoveEdges() {
		LengthComparator<Graph> comparator(this->graph());
		for (auto it = this->graph().SmartEdgeBegin(comparator); !it.IsEnd();
				++it) {
			typename Graph::EdgeId e = *it;
			if (this->graph().length(e) > max_length_) {
				return;
			}
			vector<EdgeId> adjacent_edges;
			Append(adjacent_edges,
					this->graph().OutgoingEdges(this->graph().EdgeStart(e)));
			Append(adjacent_edges,
					this->graph().IncomingEdges(this->graph().EdgeStart(e)));
			Append(adjacent_edges,
					this->graph().OutgoingEdges(this->graph().EdgeEnd(e)));
			Append(adjacent_edges,
					this->graph().IncomingEdges(this->graph().EdgeEnd(e)));

			if (CheckAdjacent(adjacent_edges, e)) {
				this->DeleteEdge(e, false);
//				VertexId start = g_.EdgeStart(e);
//				VertexId end = g_.EdgeEnd(e);
//				if (!g_.RelatedVertices(start, end)) {
//					g_.DeleteEdge(e);
//					g_.CompressVertex(start);
//					g_.CompressVertex(end);
//				}
			}
		}
//		omnigraph::Cleaner<Graph> cleaner(this->graph());
//		cleaner.Clean();
	}
};

template<class Graph>
class NewTopologyBasedChimericEdgeRemover: public ErroneousEdgeRemover<Graph> {
	typedef typename Graph::EdgeId EdgeId;
	typedef typename Graph::VertexId VertexId;
	typedef ErroneousEdgeRemover<Graph> base;

	size_t max_length_;
	size_t uniqueness_length_;
	size_t plausibility_length_;

	bool Unique(const vector<EdgeId>& edges, bool forward) const {
		return edges.size() == 1 && CheckUniqueness(*edges.begin(), forward);
	}

	bool ExistPlausible(const vector<EdgeId>& edges) const {
		for (auto it = edges.begin(); it != edges.end(); ++it) {
			if (CheckPlausibility(*it)) {
				return true;
			}
		}
		return false;
	}

	bool CheckStart(EdgeId e) const {
		return Unique(this->graph().IncomingEdges(this->graph().EdgeStart(e)),
				false)
				&& ExistPlausible(
						this->graph().OutgoingEdges(this->graph().EdgeStart(e)));
	}

	bool CheckEnd(EdgeId e) const {
		return Unique(this->graph().OutgoingEdges(this->graph().EdgeEnd(e)),
				true)
				&& ExistPlausible(
						this->graph().IncomingEdges(this->graph().EdgeEnd(e)));
	}

public:
	NewTopologyBasedChimericEdgeRemover(Graph& g, size_t max_length,
			size_t uniqueness_length, size_t plausibility_length,
			EdgeRemover<Graph>& edge_remover) :
			base(g, edge_remover), max_length_(max_length), uniqueness_length_(
					uniqueness_length), plausibility_length_(
					plausibility_length) {
		VERIFY(max_length < plausibility_length);
		VERIFY(uniqueness_length > plausibility_length);
	}

protected:

	size_t uniqueness_length() const {
		return uniqueness_length_;
	}

	size_t plausibility_length() const {
		return plausibility_length_;
	}

	virtual bool CheckUniqueness(EdgeId e, bool forward) const {
		return this->graph().length(e) >= uniqueness_length_;
	}

	virtual bool CheckPlausibility(EdgeId e) const {
		return this->graph().length(e) >= plausibility_length_;
	}

	void InnerRemoveEdges() {
		LengthComparator<Graph> comparator(this->graph());
		for (auto it = this->graph().SmartEdgeBegin(comparator); !it.IsEnd();
				++it) {
			typename Graph::EdgeId e = *it;
			if (this->graph().length(e) > max_length_) {
				return;
			}
			if (CheckStart(e) || CheckEnd(e)) {
				this->DeleteEdge(e, false);
			}
		}
	}
};

template<class Graph>
class AdvancedTopologyChimericEdgeRemover: public NewTopologyBasedChimericEdgeRemover<
		Graph> {
	typedef typename Graph::EdgeId EdgeId;
	typedef typename Graph::VertexId VertexId;
	typedef NewTopologyBasedChimericEdgeRemover<Graph> base;

	UniquePathFinder<Graph> unique_path_finder_;

	PlausiblePathFinder<Graph> plausible_path_finder_;

public:
	AdvancedTopologyChimericEdgeRemover(Graph& g, size_t max_length,
			size_t uniqueness_length, size_t plausibility_length,
			EdgeRemover<Graph>& edge_remover) :
			base(g, max_length, uniqueness_length, plausibility_length,
					edge_remover), unique_path_finder_(g), plausible_path_finder_(
					g) {
	}

protected:

	bool CheckUniqueness(EdgeId e, bool forward) const {
		return CummulativeLength(
				this->graph(),
				forward ?
						unique_path_finder_.UniquePathForward(e) :
						unique_path_finder_.UniquePathBackward(e))
				>= this->uniqueness_length();
	}

	bool CheckPlausibility(EdgeId e, bool forward) const {
		return CummulativeLength(
				this->graph(),
				forward ?
						plausible_path_finder_.PlausiblePath(e,
								ForwardDirection<Graph>(this->graph())) :
						plausible_path_finder_.PlausiblePath(e,
								BackwardDirection<Graph>(this->graph())))
				>= this->plausibility_length();
	}
};

template<class Graph>
class PairInfoAwareErroneousEdgeRemover: public ErroneousEdgeRemover<Graph> {
	typedef typename Graph::EdgeId EdgeId;
	typedef typename Graph::VertexId VertexId;
	typedef ErroneousEdgeRemover<Graph> base;

	const PairedInfoIndex<Graph>& paired_index_;
	size_t max_length_;
	size_t min_neighbour_length_;
	size_t insert_size_;
	size_t read_length_;
	size_t gap_;

public:
	PairInfoAwareErroneousEdgeRemover(Graph& g,
			const PairedInfoIndex<Graph>& paired_index, size_t max_length,
			size_t min_neighbour_length, size_t insert_size, size_t read_length,
			EdgeRemover<Graph>& edge_remover) :
			base(g, edge_remover), paired_index_(paired_index), max_length_(
					max_length), min_neighbour_length_(min_neighbour_length), insert_size_(
					insert_size), read_length_(read_length), gap_(
					insert_size_ - 2 * read_length_) {
		VERIFY(insert_size_ >= 2 * read_length_);
	}

	bool ShouldContainInfo(EdgeId e1, EdgeId e2, size_t gap_length) {
		//todo discuss addition of negative delta
		//todo second condition may be included into the constructor warn/assert
		TRACE(
				"Checking whether should be pair info between e1 " << PrintEdge(e1) << " and e2 " << PrintEdge(e2) << " with gap " << gap_length);
		bool should_contain = gap_length
				>= PairInfoPathLengthLowerBound(this->graph().k(),
						this->graph().length(e1), this->graph().length(e2),
						gap_, 0.)
				&& gap_length
						<= PairInfoPathLengthUpperBound(this->graph().k(),
								insert_size_, 0.);
		TRACE("Result: " << should_contain);
		return should_contain;
	}

	bool ContainsInfo(EdgeId e1, EdgeId e2, size_t ec_length) {
		TRACE(
				"Looking for pair info between e1 " << PrintEdge(e1) << " and e2 " << PrintEdge(e2));
		vector<PairInfo<EdgeId>> infos = paired_index_.GetEdgePairInfo(e1, e2);
		for (auto it = infos.begin(); it != infos.end(); ++it) {
			PairInfo<EdgeId> info = *it;
			size_t distance = this->graph().length(e1) + ec_length;
			if (math::ge(0. + distance + info.variance, info.d)
					&& math::le(0. + distance, info.d + info.variance)) {
				TRACE("Pair info found");
				return true;
			}
		}TRACE("Pair info not found");
		return false;
	}

	bool CheckAnyPairInfoAbsense(EdgeId possible_ec) {
		TRACE("Checking pair info absense");
		vector<EdgeId> incoming = this->graph().IncomingEdges(
				this->graph().EdgeStart(possible_ec));
		vector<EdgeId> outgoing = this->graph().OutgoingEdges(
				this->graph().EdgeEnd(possible_ec));
		for (auto it1 = incoming.begin(); it1 != incoming.end(); ++it1)
			for (auto it2 = outgoing.begin(); it2 != outgoing.end(); ++it2)
				if (!ShouldContainInfo(*it1, *it2,
						this->graph().length(possible_ec))
						|| ContainsInfo(*it1, *it2,
								this->graph().length(possible_ec))) {
					TRACE("Check absense: fail");
					return false;
				}TRACE("Check absense: ok");
		return true;
	}

	bool CheckAdjacentLengths(const vector<EdgeId>& edges, EdgeId possible_ec) {
		TRACE("Checking adjacent lengths");
		TRACE("min_neighbour_length = " << min_neighbour_length_);
		for (auto it = edges.begin(); it != edges.end(); ++it)
			if (min_neighbour_length_ > this->graph().length(*it)) {
				TRACE(
						"Check fail: edge " << PrintEdge(*it) << " was too short");
				return false;
			}TRACE("Check ok");
		return true;
	}

	string PrintEdge(EdgeId e) {
		stringstream ss;
		ss << this->graph().int_ids().ReturnIntId(e) << "(" << e << ") "
				<< this->graph().length(e) << "(" << this->graph().coverage(e)
				<< ")";
		return ss.str();
	}

	void InnerRemoveEdges() {
		TRACE("Removing erroneous edges based on pair info");
		LengthComparator<Graph> comparator(this->graph());
		for (auto it = this->graph().SmartEdgeBegin(comparator); !it.IsEnd();
				++it) {
			typename Graph::EdgeId e = *it;
			TRACE("Considering edge " << PrintEdge(e));
			if (this->graph().length(e) > max_length_) {
				TRACE("Max length bound = " << max_length_ << " was exceeded");
				return;
			}
			vector<EdgeId> adjacent_edges;
			Append(adjacent_edges,
					this->graph().IncomingEdges(this->graph().EdgeStart(e)));
			Append(adjacent_edges,
					this->graph().OutgoingEdges(this->graph().EdgeEnd(e)));

			if (CheckAdjacentLengths(adjacent_edges, e)
					&& CheckAnyPairInfoAbsense(e)) {
//				VertexId start = g_.EdgeStart(e);
//				VertexId end = g_.EdgeEnd(e);
//				TRACE("Try deleting edge " << PrintEdge(e));
//				if (!g_.RelatedVertices(start, end)) {
//					TRACE("Vertices not related");
//					TRACE("Deleting edge " << PrintEdge(e));
//					g_.DeleteEdge(e);
//					TRACE("Compressing start");
//					g_.CompressVertex(start);
//					TRACE("Compressing end");
//					g_.CompressVertex(end);
//				} else {
//					TRACE("Vertices are related");
//				}
				this->DeleteEdge(e, false);
			}
		}
	}
private:
	DECL_LOGGER("PairInfoAwareErroneousEdgeRemover")
	;
};

}

#endif /* ERRONEOUS_CONNECTION_REMOVER_HPP_ */
