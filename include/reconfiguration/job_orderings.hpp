#ifndef RECONFIGURATION_JOB_ORDERINGS_H
#define RECONFIGURATION_JOB_ORDERINGS_H

#include "problem.hpp"
#include "rating_graph.hpp"

namespace NP::Reconfiguration {
	class Job_orderings {
		std::vector<size_t> earliest_dispatches;
		std::vector<size_t> latest_dispatches;
		size_t reverse_occurrence_index = 0;
		bool finalized = false;

	public:
		Job_orderings(size_t num_jobs) {
			assert(num_jobs > 0);
			earliest_dispatches.reserve(num_jobs);
			latest_dispatches.reserve(num_jobs);
			for (Job_index counter = 0; counter < num_jobs; counter++) {
				earliest_dispatches.push_back(num_jobs);
				latest_dispatches.push_back(0);
			}
		}

		void advance() {
			reverse_occurrence_index += 1;
		}

		void mark_dispatchable(Job_index job_index) {
			assert(!finalized);
			earliest_dispatches[job_index] = std::min(earliest_dispatches[job_index], reverse_occurrence_index);
			latest_dispatches[job_index] = std::max(latest_dispatches[job_index], reverse_occurrence_index);
		}

		void finalize() {
			assert(!finalized);
			const size_t num_jobs = earliest_dispatches.size();
			for (Job_index job_index = 0; job_index < num_jobs; job_index++) {
				size_t earliest = earliest_dispatches[job_index];
				if (earliest == num_jobs) {
					earliest_dispatches[job_index] = reverse_occurrence_index + 1;
					latest_dispatches[job_index] = num_jobs - 1;
				} else {
					earliest_dispatches[job_index] = reverse_occurrence_index - latest_dispatches[job_index];
					latest_dispatches[job_index] = reverse_occurrence_index - earliest;
				}
			}
			finalized = true;
		}

		size_t get_earliest_occurrence_index(Job_index job_index) const {
			assert(finalized);
			return earliest_dispatches[job_index];
		}

		size_t get_latest_occurrence_index(Job_index job_index) const {
			assert(finalized);
			return latest_dispatches[job_index];
		}
	};

	static std::vector<Job_orderings> determine_orderings_for_cuts(size_t num_jobs, Rating_graph &graph, const std::vector<Rating_graph_cut> &cuts) {
		std::vector<Job_orderings> cut_orderings(cuts.size(), num_jobs);

		std::sort(graph.edges.begin(), graph.edges.end(), [](const Rating_edge &a, const Rating_edge &b) {
			return a.get_child_node_index() < b.get_child_node_index();
		});

		std::vector<size_t> cut_mapping(graph.nodes.size(), cuts.size());

		for (size_t cut_index = 0; cut_index < cuts.size(); cut_index++) {
			auto &job_orderings = cut_orderings[cut_index];

			std::vector<size_t> current_node_indices{cuts[cut_index].node_index};
			std::vector<size_t> next_node_indices;

			while (!current_node_indices.empty()) {
				for (const size_t node_index : current_node_indices) {
					bool has_dispatched = false;
					size_t edge_index;
					{
						const Rating_edge dummy_search_edge(node_index, node_index, 0);
						const auto edge_iter = std::lower_bound(graph.edges.begin(), graph.edges.end(), dummy_search_edge, [](const Rating_edge &a, const Rating_edge &b) {
							return a.get_child_node_index() < b.get_child_node_index();
						});
						edge_index = edge_iter - graph.edges.begin();
					}

					while (edge_index < graph.edges.size() && graph.edges[edge_index].get_child_node_index() == node_index) {
						const auto &edge = graph.edges[edge_index];
						next_node_indices.push_back(edge.get_parent_node_index());
						if (!has_dispatched && node_index != cuts[cut_index].node_index) job_orderings.advance();
						has_dispatched = true;
						job_orderings.mark_dispatchable(edge.get_taken_job_index());
						edge_index += 1;
					}
				}
				std::swap(current_node_indices, next_node_indices);
				next_node_indices.clear();
			}

			job_orderings.finalize();
		}

		std::sort(graph.edges.begin(), graph.edges.end(), [](const Rating_edge &a, const Rating_edge &b) {
			return a.get_parent_node_index() < b.get_parent_node_index();
		});

		return cut_orderings;
	}
}

#endif
