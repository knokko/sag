#ifndef TREE_CUTTER_H
#define TREE_CUTTER_H

#include <vector>

#include "rating_graph.hpp"
#include "sub_graph.hpp"

namespace NP::Reconfiguration {

	static std::vector<Rating_graph_cut> cut_rating_graph(Rating_graph &graph) {
		// Rating graph should already be sorted at this point
		for (size_t edge_index = 1; edge_index < graph.edges.size(); edge_index++) {
			assert(graph.edges[edge_index - 1].get_parent_node_index() <= graph.edges[edge_index].get_parent_node_index());
		}

		struct Cut_builder {
			size_t node_index;
			std::vector<Job_index> forbidden_jobs;

			void print() const {
				std::cout << "Cut_builder(node = " << node_index << ", forbidden jobs:";
				for (const auto job_index : forbidden_jobs) std::cout << " " << job_index;
				std::cout << ")\n";
			}
		};
		std::vector<Cut_builder> cut_builders;

		std::vector<bool> has_visited;
		has_visited.reserve(graph.nodes.size());
		for (size_t counter = 0; counter < graph.nodes.size(); counter++) has_visited.push_back(false);

		// TODO Also make this more compact?
		struct Node {
			const size_t index;
			size_t next_edge_index;
			float largest_child_rating;
		};

		std::vector<Node> branch;
		if (graph.nodes[0].get_rating() > 0.0f) {
			branch.push_back(Node { });
		} else {
			std::vector<Job_index> possible_jobs;
			size_t edge_index = 0;
			while (edge_index < graph.edges.size() && graph.edges[edge_index].get_parent_node_index() == 0) {
				possible_jobs.push_back(graph.edges[edge_index].get_taken_job_index());
				edge_index += 1;
			}
			cut_builders.push_back(Cut_builder { .forbidden_jobs=possible_jobs });
		}

		while (!branch.empty()) {
			size_t branch_index = branch.size() - 1;
			size_t node_index = branch[branch_index].index;
			size_t edge_index = branch[branch_index].next_edge_index;

			auto node = graph.nodes[node_index];

			assert(node.get_rating() > 0.0f);
			if (node.get_rating() == 1.0 || has_visited[node_index] || edge_index >= graph.edges.size() || graph.edges[edge_index].get_parent_node_index() != node_index) {
				has_visited[node_index] = true;
				branch.pop_back();
				continue;
			}

			if (edge_index == 0 || graph.edges[edge_index - 1].get_parent_node_index() != node_index) {
				size_t test_edge_index = edge_index;
				while (test_edge_index < graph.edges.size() && graph.edges[test_edge_index].get_parent_node_index() == node_index) {
					branch[branch_index].largest_child_rating = std::max(
							branch[branch_index].largest_child_rating, graph.nodes[graph.edges[test_edge_index].get_child_node_index()].get_rating()
					);
					test_edge_index++;
				}
			}

			branch[branch_index].next_edge_index += 1;
			auto current_edge = graph.edges[edge_index];
			auto destination = graph.nodes[current_edge.get_child_node_index()];
			if (destination.get_rating() == 1.0f) continue; // Check if edge is already cut by a similar node in previous iterations

			if (destination.get_rating() < branch[branch_index].largest_child_rating) {
				bool add_new = true;
				for (auto &cut : cut_builders) {
					if (cut.node_index == node_index) {
						add_new = false;
						cut.forbidden_jobs.push_back(current_edge.get_taken_job_index());
					}
				}

				if (add_new) {
					cut_builders.push_back(Cut_builder {
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

		std::sort(graph.edges.begin(), graph.edges.end(), [](const Rating_edge &a, const Rating_edge &b) {
			return a.get_child_node_index() < b.get_child_node_index();
		});

		std::vector<Rating_graph_cut> cuts;
		for (const auto &builder : cut_builders) {
			Sub_graph previous_jobs;
			std::vector<size_t> nodes_to_visit;
			nodes_to_visit.push_back(builder.node_index);

			/**
			 * Maps node indices from the original graph to node indices in the previous_jobs graph
			 */
			std::vector<size_t> sub_graph_mapping;
			sub_graph_mapping.reserve(graph.nodes.size());
			for (size_t counter = 0; counter < graph.nodes.size(); counter++) sub_graph_mapping.push_back(-1);
			sub_graph_mapping[builder.node_index] = 0;

			while (!nodes_to_visit.empty()) {
				size_t current_node = nodes_to_visit[nodes_to_visit.size() - 1];
				nodes_to_visit.pop_back();

				size_t mapped_current_node = sub_graph_mapping[current_node];
				assert(mapped_current_node < graph.nodes.size());

				Rating_edge dummy_search_edge(current_node, current_node, 0);
				auto edge_iter = std::lower_bound(graph.edges.begin(), graph.edges.end(), dummy_search_edge, [](const Rating_edge &a, const Rating_edge &b) {
					return a.get_child_node_index() < b.get_child_node_index();
				});
				size_t edge_index = edge_iter - graph.edges.begin();

				while (edge_index < graph.edges.size() && graph.edges[edge_index].get_child_node_index() == current_node) {
					size_t original_parent_node = graph.edges[edge_index].get_parent_node_index();
					size_t destination_node = sub_graph_mapping[original_parent_node];
					if (destination_node != -1) {
						previous_jobs.add_edge_between_existing_nodes(
								mapped_current_node, destination_node, graph.edges[edge_index].get_taken_job_index()
						);
					} else {
						destination_node = previous_jobs.add_edge_to_new_node(mapped_current_node, graph.edges[edge_index].get_taken_job_index());
						assert(destination_node > 0 && destination_node < graph.nodes.size());
						sub_graph_mapping[original_parent_node] = destination_node;
						nodes_to_visit.push_back(original_parent_node);
					}
					edge_index += 1;
				}
			}

			cuts.push_back(Rating_graph_cut {
					.previous_jobs=std::make_unique<Sub_graph>(previous_jobs.reversed()),
					.forbidden_jobs=builder.forbidden_jobs
			});
		}

		std::sort(graph.edges.begin(), graph.edges.end(), [](const Rating_edge &a, const Rating_edge &b) {
			return a.get_parent_node_index() < b.get_parent_node_index();
		});

		for (size_t index = 0; index < cut_builders.size(); index++) {
			auto &builder = cut_builders[index];
			Rating_edge dummy_search_edge(builder.node_index, builder.node_index, 0);
			auto edge_iter = std::lower_bound(graph.edges.begin(), graph.edges.end(), dummy_search_edge, [](const Rating_edge &a, const Rating_edge &b) {
				return a.get_parent_node_index() < b.get_parent_node_index();
			});
			size_t edge_index = edge_iter - graph.edges.begin();

			while (edge_index < graph.edges.size() && graph.edges[edge_index].get_parent_node_index() == builder.node_index) {
				bool is_forbidden = false;
				auto taken_job = graph.edges[edge_index].get_taken_job_index();
				for (const auto forbidden_job : builder.forbidden_jobs) {
					if (taken_job == forbidden_job) {
						is_forbidden = true;
						break;
					}
				}

				if (!is_forbidden) cuts[index].allowed_jobs.push_back(taken_job);
				edge_index += 1;
			}
		}

		return cuts;
	}
}

#endif
