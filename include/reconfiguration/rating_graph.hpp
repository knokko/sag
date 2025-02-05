#ifndef RATING_GRAPH_H
#define RATING_GRAPH_H
#include <ranges>
#include <vector>
#include <array>

#include "agent.hpp"
#include "attachment.hpp"
#include "global/space.hpp"
#include "index_set.hpp"

namespace NP::Reconfiguration {

	static uint8_t _extract(size_t value, int shift) {
		return static_cast<uint8_t>(value >> shift);
	}

	static size_t _recover(uint8_t packed, int shift) {
		return static_cast<size_t>(packed) << shift;
	}

	struct Rating_graph_cut {
		size_t node_index;
		std::vector<Job_index> forbidden_jobs;
		std::vector<Job_index> allowed_jobs;
		std::vector<Job_index> safe_jobs;
	};

	struct Rating_edge {
		std::array<uint8_t, 5> raw_parent_node_index;
		std::array<uint8_t, 4> raw_child_node_index_offset;
		std::array<uint8_t, 3> raw_taken_job_index;

		Rating_edge(
				size_t parent_node_index, size_t child_node_index, size_t taken_job_index
		) : raw_parent_node_index({
				_extract(parent_node_index, 0), _extract(parent_node_index, 8), _extract(parent_node_index, 16),
				_extract(parent_node_index, 24), _extract(parent_node_index, 32)
	   	}), raw_child_node_index_offset({
			   _extract(child_node_index - parent_node_index, 0), _extract(child_node_index - parent_node_index, 8),
			   _extract(child_node_index - parent_node_index, 16), _extract(child_node_index - parent_node_index, 24)
	   	}), raw_taken_job_index({
			   _extract(taken_job_index, 0), _extract(taken_job_index, 8), _extract(taken_job_index, 16)
	   	}) {
			assert(parent_node_index >= 0 && child_node_index >= parent_node_index && taken_job_index >= 0);
			assert(parent_node_index < (static_cast<uint64_t>(1) << 40));
			assert((child_node_index - parent_node_index) < (static_cast<uint64_t>(1) << 32));
			assert(taken_job_index < (1 << 24));
		}

		size_t get_parent_node_index() const {
			return _recover(raw_parent_node_index[0], 0) | _recover(raw_parent_node_index[1], 8) | _recover(raw_parent_node_index[2], 16) |
				   _recover(raw_parent_node_index[3], 24) | _recover(raw_parent_node_index[4], 32);
		}

		size_t get_child_node_index() const {
			size_t offset = _recover(raw_child_node_index_offset[0], 0) | _recover(raw_child_node_index_offset[1], 8) |
							_recover(raw_child_node_index_offset[2], 16) | _recover(raw_child_node_index_offset[3], 24);
			return get_parent_node_index() + offset;
		}

		size_t get_taken_job_index() const {
			return _recover(raw_taken_job_index[0], 0) | _recover(raw_taken_job_index[1], 8) | _recover(raw_taken_job_index[2], 16);
		}
	};

	struct Rating_node {
		uint8_t raw_rating = 0;

		double get_rating() const {
			if (raw_rating == 255) return -1.0;
			return static_cast<double>(raw_rating) / 250.0;
		}

		void set_rating(double rating) {
			if (rating == -1.0) {
				raw_rating = 255;
				return;
			}
			assert(rating >= 0.0 && rating <= 1.001);
			raw_rating = static_cast<uint8_t>(250.0 * rating + 0.5);
			if (rating > 0.0 && raw_rating == 0) raw_rating = 1;
			if (rating < 1.0 && raw_rating == 250) raw_rating = 249;
		}
	};

	class Rating_graph {
		bool dry_run = true;
		size_t edge_counter = 0;
	public:
		std::vector<Rating_node> nodes;
		std::vector<Rating_edge> edges;

		Rating_graph() {
			nodes.push_back(Rating_node {});
		}

		void end_dry_run() {
			assert(dry_run);
			dry_run = false;
			edges.reserve(edge_counter);
			nodes.resize(1);
		}

		size_t add_node(size_t parent_index, Job_index taken_job) {
			assert(taken_job >= 0);
			if (nodes[parent_index].get_rating() == -1.0 && false) return parent_index;

			size_t child_index = nodes.size();
			nodes.push_back(Rating_node { });
			if (nodes[parent_index].get_rating() == -1.0) nodes[child_index].set_rating(-1.0); // TODO Do this cleaner
			if (dry_run) edge_counter += 1;
			else edges.push_back(Rating_edge(parent_index, child_index, taken_job));
			return child_index;
		}

		void insert_edge(size_t parent_index, size_t child_index, Job_index taken_job) {
			assert(parent_index >= 0 && parent_index < nodes.size());
			assert(child_index >= 0 && child_index < nodes.size());
			assert(taken_job >= 0);
			if (nodes[parent_index].get_rating() == -1.0 && false) {
				size_t dummy_node_index = add_node(parent_index, taken_job);
				if (dry_run) edge_counter += 1;
				else edges.push_back(Rating_edge(parent_index, dummy_node_index, taken_job));
			} else {
				assert(parent_index < child_index);
				if (dry_run) edge_counter += 1;
				else edges.push_back(Rating_edge(parent_index, child_index, taken_job));
			}
		}

		void set_missed_deadline(size_t node_index) {
			assert(node_index >= 0);
			assert(node_index < nodes.size());
			nodes[node_index].set_rating(-1.0);
		}

		void mark_as_leaf_node(size_t node_index) {
			assert(node_index >= 0 && node_index < nodes.size());
			if (nodes[node_index].get_rating() == -1.0) return;
			nodes[node_index].set_rating(1.0);
		}

		void compute_ratings() {
			assert(!dry_run);

			std::sort(edges.begin(), edges.end(), [](const Rating_edge &a, const Rating_edge &b) {
				return a.get_parent_node_index() < b.get_parent_node_index();
			});

			size_t edge_index = edges.size() - 1;
			for (size_t node_index = nodes.size() - 1; node_index < nodes.size(); node_index--) {
				auto &node = nodes[node_index];
				if (node.get_rating() == -1.0) node.set_rating(0.0);

				while (edge_index < edges.size() && edges[edge_index].get_parent_node_index() > node_index) edge_index -= 1;

				size_t num_children = 0;
				double rating = 0.0;
				while (edge_index < edges.size() && edges[edge_index].get_parent_node_index() == node_index) {
					rating += nodes[edges[edge_index].get_child_node_index()].get_rating();
					num_children += 1;
					edge_index -= 1;
				}
				if (num_children > 0) node.set_rating(rating / num_children);
			}

			assert(edge_index == -1);
		}

		std::shared_ptr<Index_set> get_previously_dispatched_jobs(size_t node_index) {
			// TODO It's probably not nice that sorting is required, but this method is currently unused, so it doesn't matter now...
			std::sort(edges.begin(), edges.end(), [](const Rating_edge &a, const Rating_edge &b) {
				return a.get_child_node_index() < b.get_child_node_index();
			});

			auto previous_jobs = std::make_shared<Index_set>();
			std::vector<size_t> nodes_to_visit{node_index};
			Index_set has_inserted_into_nodes_to_visit;

			while (!nodes_to_visit.empty()) {
				const size_t current_node = nodes_to_visit[nodes_to_visit.size() - 1];
				nodes_to_visit.pop_back();

				const Rating_edge dummy_search_edge(current_node, current_node, 0);
				auto edge_iter = std::lower_bound(edges.begin(), edges.end(), dummy_search_edge, [](const Rating_edge &a, const Rating_edge &b) {
					return a.get_child_node_index() < b.get_child_node_index();
				});
				size_t edge_index = edge_iter - edges.begin();

				while (edge_index < edges.size() && edges[edge_index].get_child_node_index() == current_node) {
					const auto &edge = edges[edge_index];
					previous_jobs->add(edge.get_taken_job_index());

					if (!has_inserted_into_nodes_to_visit.contains(edge.get_parent_node_index())) {
						nodes_to_visit.push_back(edge.get_parent_node_index());
						has_inserted_into_nodes_to_visit.add(edge.get_parent_node_index());
					}

					edge_index += 1;
				}
			}

			return previous_jobs;
		}

		std::vector<int> create_depth_mapping() const {
			for (size_t edge_index = 1; edge_index < edges.size(); edge_index++) {
				assert(edges[edge_index - 1].get_parent_node_index() <= edges[edge_index].get_parent_node_index());
			}
			std::vector<int> depth_mapping(nodes.size(), -1);
			depth_mapping[0] = 0;
			for (const auto &edge : edges) {
				const size_t child = edge.get_child_node_index();
				const size_t parent = edge.get_parent_node_index();
				assert(depth_mapping[parent] != -1);
				const int old_depth = depth_mapping[child];
				const int new_depth = 1 + depth_mapping[parent];
				assert(old_depth == -1 || old_depth == new_depth);
				depth_mapping[child] = new_depth;
			}
			for (int depth : depth_mapping) assert(depth >= 0);
			for (size_t node_index = 1; node_index < nodes.size(); node_index++) {
				assert(depth_mapping[node_index - 1] <= depth_mapping[node_index]);
			}
			return depth_mapping;
		}

		template<class Time> void generate_dot_file(
				const char *file_path,
				const Scheduling_problem<Time> &problem,
				const std::vector<Rating_graph_cut> &cuts,
				bool simplify = true
		) {
			assert(!dry_run);
			FILE *file = fopen(file_path, "w");
			if (!file) {
				std::cout << "Failed to write to file " << file_path << std::endl;
				return;
			}

			std::sort(edges.begin(), edges.end(), [](const Rating_edge &a, const Rating_edge &b) {
				return a.get_parent_node_index() < b.get_parent_node_index();
			});

			std::vector<size_t> should_visit_nodes;
			should_visit_nodes.push_back(2);
			for (size_t counter = 1; counter < nodes.size(); counter++) should_visit_nodes.push_back(0);

			fprintf(file, "strict digraph Rating {\n");

			size_t edge_index = 0;
			for (size_t node_index = 0; node_index < nodes.size(); node_index++) {
				if (should_visit_nodes[node_index] == 0) continue;
				fprintf(file, "\tnode%lu [label=%.2f", node_index, nodes[node_index].get_rating());
				if (nodes[node_index].get_rating() == 0.0) fprintf(file, ", color=red");
				if (nodes[node_index].get_rating() == 1.0) fprintf(file, ", color=green");
				fprintf(file, "];\n");
				if (should_visit_nodes[node_index] == 1) continue;

				while (edge_index < edges.size() && edges[edge_index].get_parent_node_index() <= node_index) {
					const auto &edge = edges[edge_index];
					if (edge.get_parent_node_index() != node_index) {
						edge_index += 1;
						continue;
					}
					double child_rating = nodes[edge.get_child_node_index()].get_rating();
					should_visit_nodes[edge.get_child_node_index()] = (simplify && (child_rating == 1.0 || child_rating == 0.0)) ? 1 : 2;
					const auto job = problem.jobs[edge.get_taken_job_index()].get_id();
					fprintf(
							file, "\tnode%lu -> node%lu [label=\"T%luJ%lu (%zu)\"",
							node_index, edge.get_child_node_index(), job.task, job.job, edge.get_taken_job_index()
					);

					for (size_t cut_index = 0; cut_index < cuts.size(); cut_index++) {
						if (cuts[cut_index].node_index != node_index) continue;

						bool is_forbidden = false;
						bool is_safe = false;
						for (const auto forbidden_job : cuts[cut_index].forbidden_jobs) {
							if (edge.get_taken_job_index() == forbidden_job) {
								is_forbidden = true;
								break;
							}
						}

						for (const auto safe_job : cuts[cut_index].safe_jobs) {
							if (edge.get_taken_job_index() == safe_job) {
								is_safe = true;
								break;
							}
						}

						if (is_forbidden) fprintf(file, ", color=red");
						if (is_safe) fprintf(file, ", color=blue");
					}

					fprintf(file, "];\n");
					edge_index += 1;
				}
			}

			fprintf(file, "}\n");

			fclose(file);
		}
	};

	struct Attachment_rating_node final: Attachment {
		size_t index = 0;
	};

	template<class Time> class Agent_rating_graph : public Agent<Time> {
		Rating_graph *rating_graph;

	public:
		static void generate(const Scheduling_problem<Time> &problem, Rating_graph &rating_graph) {
			Agent_rating_graph agent;
			agent.rating_graph = &rating_graph;

			Analysis_options test_options;
			test_options.early_exit = false;

			delete Global::State_space<Time>::explore(problem, test_options, &agent);
			rating_graph.end_dry_run();

			auto space = Global::State_space<Time>::explore(problem, test_options, &agent);
			rating_graph.compute_ratings();
			delete space;
		}

		Attachment* create_initial_node_attachment() override {
			return new Attachment_rating_node();
		}

		Attachment* create_next_node_attachment(
				const Global::Schedule_node<Time> &parent_node, Job<Time> next_job
		) override {
			const auto parent_attachment = dynamic_cast<Attachment_rating_node*>(parent_node.attachment);
			assert(parent_attachment);
			auto new_attachment = new Attachment_rating_node();
			new_attachment->index = rating_graph->add_node(parent_attachment->index, next_job.get_job_index());
			return new_attachment;
		}

		void merge_node_attachments(
				Global::Schedule_node<Time>* destination_node,
				const Global::Schedule_node<Time> &parent_node,
				Job<Time> next_job
		) override {
			const auto parent_attachment = dynamic_cast<Attachment_rating_node*>(parent_node.attachment);
			assert(parent_attachment);
			const auto child_attachment = dynamic_cast<Attachment_rating_node*>(destination_node->attachment);
			assert(child_attachment);
			rating_graph->insert_edge(parent_attachment->index, child_attachment->index, next_job.get_job_index());
		}

		void missed_deadline(const Global::Schedule_node<Time> &failed_node, const Job<Time> &late_job) override {
			const auto attachment = dynamic_cast<Attachment_rating_node*>(failed_node.attachment);
			assert(attachment);
			rating_graph->set_missed_deadline(attachment->index);
		}

		void encountered_dead_end(const Global::Schedule_node<Time> &dead_node) override {
			const auto attachment = dynamic_cast<Attachment_rating_node*>(dead_node.attachment);
			assert(attachment);
			rating_graph->set_missed_deadline(attachment->index);
		}

		void mark_as_leaf_node(const Global::Schedule_node<Time> &node) override {
			const auto attachment = dynamic_cast<Attachment_rating_node*>(node.attachment);
			assert(attachment);
			rating_graph->mark_as_leaf_node(attachment->index);
		}

		bool allow_merge(
				const Global::Schedule_node<Time> &parent_node,
				const Job<Time> &taken_job,
				const Global::Schedule_node<Time> &destination_node
		) override {
			const auto parent_attachment = dynamic_cast<Attachment_rating_node*>(parent_node.attachment);
			assert(parent_attachment);
			const auto destination_attachment = dynamic_cast<Attachment_rating_node*>(destination_node.attachment);
			assert(destination_attachment);

			// Forbid merges between nodes where exactly 1 has missed a deadline
			return (rating_graph->nodes[destination_attachment->index].get_rating() != -1.0) ==
					(rating_graph->nodes[parent_attachment->index].get_rating() != -1.0);
		}

		bool should_explore(const Global::Schedule_node<Time> &node) override {
			const auto attachment = dynamic_cast<Attachment_rating_node*>(node.attachment);
			assert(attachment);
			return rating_graph->nodes[attachment->index].get_rating() != -1.0;
		}
	};
}

#endif
