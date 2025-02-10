#ifndef TREE_CUTTER_H
#define TREE_CUTTER_H

#include <vector>

#include "rating_graph.hpp"
#include "feasibility/graph.hpp"

namespace NP::Reconfiguration {

	static std::vector<Rating_graph_cut> cut_rating_graph(
		const Rating_graph &graph, const std::vector<Job_index> &safe_path
	) {
		assert(graph.is_sorted_by_parents());
		std::vector<Rating_graph_cut> cuts;

		size_t node_index = 0;
		size_t depth = 0;
		for (const Job_index safe_job_index : safe_path) {
			Rating_graph_cut cut{ .node_index=node_index, .depth=depth, .safe_job=safe_job_index };
			size_t next_node_index = -1;
			for (size_t edge_index = graph.first_edge_index_with_parent_node(node_index); edge_index < graph.edges.size(); edge_index++) {
				const auto &edge = graph.edges[edge_index];
				if (edge.get_parent_node_index() != node_index) break;
				if (edge.get_taken_job_index() == safe_job_index) {
					next_node_index = edge.get_child_node_index();
					continue;
				}
				if (graph.nodes[edge.get_child_node_index()].get_rating() == 1.0) {
					cut.allowed_jobs.push_back(edge.get_taken_job_index());
				} else {
					cut.forbidden_jobs.push_back(edge.get_taken_job_index());
				}
			}
			if (!cut.forbidden_jobs.empty()) cuts.push_back(cut);
			if (next_node_index == -1) break;
			node_index = next_node_index;
			depth += 1;
		}
		return cuts;
	}
}

#endif
