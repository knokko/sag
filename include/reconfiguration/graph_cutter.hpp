#ifndef TREE_CUTTER_H
#define TREE_CUTTER_H

#include <vector>

#include "rating_graph.hpp"
#include "feasibility/graph.hpp"

namespace NP::Reconfiguration {

	template<class Time> static std::vector<Rating_graph_cut> cut_rating_graph(
		Rating_graph &graph, const Feasibility::Feasibility_graph<Time> &feasibility, double cut_threshold
	) {
		// Rating graph should already be sorted at this point
		for (size_t edge_index = 1; edge_index < graph.edges.size(); edge_index++) {
			assert(graph.edges[edge_index - 1].get_parent_node_index() <= graph.edges[edge_index].get_parent_node_index());
		}

		Index_set has_visited;
		std::vector<Rating_graph_cut> cuts;

		struct Node {
			const size_t index;
			size_t next_edge_index;
			double largest_child_rating;
			double largest_feasible_child_rating;
		};

		std::vector<Node> branch;
		if (graph.nodes[0].get_rating() > 0.0) {
			branch.push_back(Node { });
		} else {
			std::vector<Job_index> possible_jobs;
			size_t edge_index = 0;
			while (edge_index < graph.edges.size() && graph.edges[edge_index].get_parent_node_index() == 0) {
				possible_jobs.push_back(graph.edges[edge_index].get_taken_job_index());
				edge_index += 1;
			}
			cuts.push_back(Rating_graph_cut { .node_index=0, .forbidden_jobs=possible_jobs });
		}

		while (!branch.empty()) {
			size_t branch_index = branch.size() - 1;
			size_t node_index = branch[branch_index].index;
			size_t edge_index = branch[branch_index].next_edge_index;

			auto node = graph.nodes[node_index];

			assert(node.get_rating() > 0.0);
			if (node.get_rating() == 1.0 || has_visited.contains(node_index) || edge_index >= graph.edges.size() || graph.edges[edge_index].get_parent_node_index() != node_index) {
				has_visited.add(node_index);
				branch.pop_back();
				continue;
			}

			if (edge_index == 0 || graph.edges[edge_index - 1].get_parent_node_index() != node_index) {
				size_t test_edge_index = edge_index;
				while (test_edge_index < graph.edges.size() && graph.edges[test_edge_index].get_parent_node_index() == node_index) {
					double edge_rating = graph.nodes[graph.edges[test_edge_index].get_child_node_index()].get_rating();
					branch[branch_index].largest_child_rating = std::max(branch[branch_index].largest_child_rating, edge_rating);
					if (feasibility.is_edge_feasible(test_edge_index)) {
						branch[branch_index].largest_feasible_child_rating = std::max(branch[branch_index].largest_feasible_child_rating, edge_rating);
					}
					test_edge_index++;
				}
			}

			branch[branch_index].next_edge_index += 1;
			const auto current_edge = graph.edges[edge_index];
			const auto destination = graph.nodes[current_edge.get_child_node_index()];
			if (destination.get_rating() == 1.0) continue; // Check if edge is already cut by a similar node in previous iterations

			if (
				(destination.get_rating() < cut_threshold * branch[branch_index].largest_feasible_child_rating) ||
				(destination.get_rating() < 1.0 && branch[branch_index].largest_feasible_child_rating == 1.0) ||
				(!feasibility.is_edge_feasible(edge_index) && destination.get_rating() < 1.0)
			) {
				bool add_new = true;
				for (auto &cut : cuts) {
					if (cut.node_index == node_index) {
						add_new = false;
						cut.forbidden_jobs.push_back(current_edge.get_taken_job_index());
					}
				}

				if (add_new) {
					cuts.push_back(Rating_graph_cut {
							.node_index = node_index,
							.forbidden_jobs=std::vector<size_t>{current_edge.get_taken_job_index()}
					});
				}
				continue;
			}

			Rating_edge dummy_search_edge(current_edge.get_child_node_index(), current_edge.get_child_node_index(), 0);
			auto child_iter = std::lower_bound(graph.edges.begin(), graph.edges.end(), dummy_search_edge, [](const Rating_edge &a, const Rating_edge &b) {
				return a.get_parent_node_index() < b.get_parent_node_index();
			});
			size_t child_edge_index = child_iter - graph.edges.begin();
			branch.push_back(Node { .index=current_edge.get_child_node_index(), .next_edge_index=child_edge_index });
		}

		for (auto &cut : cuts) {
			const Rating_edge dummy_search_edge(cut.node_index, cut.node_index, 0);
			const auto edge_iter = std::lower_bound(graph.edges.begin(), graph.edges.end(), dummy_search_edge, [](const Rating_edge &a, const Rating_edge &b) {
				return a.get_parent_node_index() < b.get_parent_node_index();
			});
			size_t edge_index = edge_iter - graph.edges.begin();

			while (edge_index < graph.edges.size() && graph.edges[edge_index].get_parent_node_index() == cut.node_index) {
				bool is_forbidden = false;
				auto taken_job = graph.edges[edge_index].get_taken_job_index();
				for (const auto forbidden_job : cut.forbidden_jobs) {
					if (taken_job == forbidden_job) {
						is_forbidden = true;
						break;
					}
				}

				if (!is_forbidden) {
					if (feasibility.is_edge_feasible(edge_index)) cut.safe_jobs.push_back(taken_job);
					else cut.allowed_jobs.push_back(taken_job);
				}
				edge_index += 1;
			}
		}

		std::sort(cuts.begin(), cuts.end(), [](const Rating_graph_cut &a, const Rating_graph_cut &b) {
			return a.node_index < b.node_index;
		});

		return cuts;
	}
}

#endif
