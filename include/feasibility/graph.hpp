#ifndef FEASIBILITY_GRAPH_H
#define FEASIBILITY_GRAPH_H

#include <vector>

#include "index_set.hpp"
#include "problem.hpp"
#include "reconfiguration/rating_graph.hpp"
#include "node.hpp"

using namespace NP::Reconfiguration;

namespace NP::Feasibility {

	template<class Time> class Feasibility_graph {
		Reconfiguration::Rating_graph &ratings;
		Index_set feasible_nodes;
		Index_set feasible_edges;
	public:
		Feasibility_graph(Reconfiguration::Rating_graph &ratings) : ratings(ratings) {}

		bool is_node_feasible(size_t node_index) const {
			return feasible_nodes.contains(node_index);
		}

		bool is_edge_feasible(size_t edge_index) const {
			return feasible_edges.contains(edge_index);
		}

		void explore_forward(
			const NP::Scheduling_problem<Time> &problem,
			const Simple_bounds<Time> &bounds,
			const std::vector<std::vector<Precedence_constraint<Time>>> &predecessor_mapping
		) {
			assert(ratings.is_sorted_by_parents());

			size_t current_node_offset = 0;
			size_t edge_offset = 0;

			std::vector<std::unique_ptr<Active_node<Time>>> current_front;
			current_front.push_back(std::make_unique<Active_node<Time>>(problem.num_processors));
			std::vector<std::unique_ptr<Active_node<Time>>> next_front;

			Index_set current_front_has_feasible_edge;

			while (!current_front.empty()) {
				size_t next_node_offset = ratings.nodes.size();
				size_t next_node_end_index = current_node_offset;
				size_t last_edge_index = 0;
				for (
					size_t edge_index = edge_offset;
					edge_index < ratings.edges.size() && ratings.edges[edge_index].get_parent_node_index() < current_node_offset + current_front.size();
					edge_index++
				) {
					size_t child_index = ratings.edges[edge_index].get_child_node_index();
					next_node_offset = std::min(next_node_offset, child_index);
					next_node_end_index = std::max(next_node_end_index, child_index);
					last_edge_index = edge_index;
				}

				if (next_node_offset == ratings.nodes.size()) {
					for (size_t relative_node_index = 0; relative_node_index < current_front.size(); relative_node_index++) {
						size_t absolute_node_index = current_node_offset + relative_node_index;
						if (current_front[relative_node_index] && ratings.nodes[absolute_node_index].get_rating() > 0.0) {
							assert(current_front[relative_node_index]->get_num_dispatched_jobs() == problem.jobs.size());
							feasible_nodes.add(absolute_node_index);
						}
					}
					break;
				}
				next_front.resize(1 + next_node_end_index - next_node_offset);

				for (size_t edge_index = edge_offset; edge_index <= last_edge_index; edge_index++) {
					const auto &edge = ratings.edges[edge_index];
					if (ratings.nodes[edge.get_parent_node_index()].get_rating() > 0.0f && ratings.nodes[edge.get_child_node_index()].get_rating() > 0.0f) {
						feasible_edges.add(edge_index);
					} else continue;
					const size_t parent_index = edge.get_parent_node_index() - current_node_offset;
					const size_t child_index = edge.get_child_node_index() - next_node_offset;

					if (!current_front[parent_index]) {
						feasible_edges.remove(edge_index);
						continue;
					}

					std::unique_ptr<Active_node<Time>> explored_node = current_front[parent_index]->copy();
					assert(!explored_node->has_missed_deadline());
					explored_node->schedule(problem.jobs[edge.get_taken_job_index()], bounds, predecessor_mapping);
					if (explored_node->has_missed_deadline()) {
						feasible_edges.remove(edge_index);
						continue;
					}

					if (next_front[child_index]) {
						explored_node->merge(*next_front[child_index]);
						if (explored_node->has_missed_deadline()) {
							feasible_edges.remove(edge_index);
							continue;
						}
						next_front[child_index]->merge(*explored_node);
					} else next_front[child_index] = std::move(explored_node);

					current_front_has_feasible_edge.add(edge.get_parent_node_index() - current_node_offset);
				}

				for (size_t relative_node_index = 0; relative_node_index < current_front.size(); relative_node_index++) {
					if (current_front_has_feasible_edge.contains(relative_node_index)) {
						size_t absolute_node_index = current_node_offset + relative_node_index;
						if (ratings.nodes[absolute_node_index].get_rating() > 0.0) feasible_nodes.add(absolute_node_index);
					}
				}

				current_front.clear();
				current_front_has_feasible_edge.clear();
				std::swap(current_front, next_front);

				current_node_offset = next_node_offset;
				edge_offset = last_edge_index + 1;
			}
		}

		void explore_backward() {
			size_t node_index = ratings.nodes.size() - 1;
			size_t edge_index = ratings.edges.size() - 1;
			size_t node_edge_index = ratings.edges.size() - 1;
			while (node_index < ratings.nodes.size()) {
				bool explore_edge = false;
				if (edge_index < ratings.edges.size()) explore_edge = ratings.edges[edge_index].get_child_node_index() > node_index;

				if (explore_edge) {
					const auto &edge = ratings.edges[edge_index];
					if (!is_node_feasible(edge.get_child_node_index())) {
						feasible_edges.remove(edge_index);
					}

					edge_index -= 1;
				} else {
					const auto &node = ratings.nodes[node_index];
					bool has_any_edge = false;
					bool has_feasible_edge = false;
					while (node_edge_index < ratings.edges.size()) {
						const auto &node_edge = ratings.edges[node_edge_index];
						assert(node_edge.get_parent_node_index() <= node_index);
						if (node_edge.get_parent_node_index() != node_index) break;
						assert(node_edge_index > edge_index || edge_index > ratings.edges.size());

						has_any_edge = true;
						if (feasible_edges.contains(node_edge_index)) has_feasible_edge = true;
						node_edge_index -= 1;
					}

					if (has_any_edge && !has_feasible_edge) feasible_nodes.remove(node_index);
					node_index -= 1;
				}
			}
		}
	};
}

#endif
