#ifndef RATING_GRAPH_H
#define RATING_GRAPH_H
#include <ranges>
#include <vector>
#include <array>

#include "agent.hpp"
#include "attachment.hpp"
#include "sub_graph.hpp"
#include "global/space.hpp"

namespace NP::Reconfiguration {

	static uint8_t _extract(size_t value, int shift) {
		return static_cast<uint8_t>(value >> shift);
	}

	static size_t _recover(uint8_t packed, int shift) {
		return static_cast<size_t>(packed) << shift;
	}

	struct Rating_graph_cut {
		std::shared_ptr<Sub_graph> previous_jobs;
		std::vector<Job_index> forbidden_jobs;
		std::vector<Job_index> allowed_jobs;
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

		void print() const {
			std::cout << "parent node = " << get_parent_node_index() << " and child node is " << get_child_node_index() << " with job " << get_taken_job_index() << std::endl;
		}
	};

	struct Rating_node {
		uint8_t raw_rating;

		float get_rating() const {
			if (raw_rating == 255) return -1.0f;
			return static_cast<float>(raw_rating) / 250.0f;
		}

		void set_rating(float rating) {
			if (rating == -1.0f) {
				raw_rating = 255;
				return;
			}
			assert(rating >= 0.0 && rating <= 1.0);
			raw_rating = static_cast<uint8_t>(250.0f * rating + 0.5f);
		}

		void print() const {
			std::cout << "rating = " << get_rating() << std::endl;
		}
	};

	class Rating_graph {
	public:
		std::vector<Rating_node> nodes;
		std::vector<Rating_edge> edges;

		Rating_graph() {
			nodes.push_back(Rating_node {});
		}

		size_t add_node(size_t parent_index, Job_index taken_job) {
			assert(taken_job >= 0);
			if (nodes[parent_index].get_rating() == -1.0f) return parent_index;

			size_t child_index = nodes.size();
			nodes.push_back(Rating_node { });
			edges.push_back(Rating_edge(parent_index, child_index, taken_job));
			return child_index;
		}

		void insert_edge(size_t parent_index, size_t child_index, Job_index taken_job) {
			assert(parent_index >= 0 && parent_index < nodes.size());
			assert(child_index >= 0 && child_index < nodes.size());
			assert(taken_job >= 0);
			if (nodes[parent_index].get_rating() == -1.0) {
				size_t dummy_node_index = add_node(parent_index, taken_job);
				edges.push_back(Rating_edge(parent_index, dummy_node_index, taken_job));
			} else {
				assert(parent_index < child_index);
				edges.push_back(Rating_edge(parent_index, child_index, taken_job));
			}
		}

		void set_missed_deadline(size_t node_index) {
			assert(node_index >= 0);
			assert(node_index < nodes.size());
			nodes[node_index].set_rating(-1.0f);
		}

		void mark_as_leaf_node(size_t node_index) {
			assert(node_index >= 0 && node_index < nodes.size());
			if (nodes[node_index].get_rating() == -1.0f) return;
			nodes[node_index].set_rating(1.0f);
		}

		void compute_ratings() {
			std::sort(edges.begin(), edges.end(), [](const Rating_edge &a, const Rating_edge &b) {
				return a.get_parent_node_index() < b.get_parent_node_index();
			});
			std::cout << "there are " << edges.size() << " edges and " << nodes.size() << " nodes\n";

			size_t edge_index = edges.size() - 1;
			for (size_t node_index = nodes.size() - 1; node_index < nodes.size(); node_index--) {
				auto &node = nodes[node_index];
				if (node.get_rating() == -1.0f) {
					node.set_rating(0.0f);
					continue;
				}

				while (edge_index < edges.size() && edges[edge_index].get_parent_node_index() > node_index) edge_index -= 1;

				size_t num_children = 0;
				float rating = node.get_rating();
				while (edge_index < edges.size() && edges[edge_index].get_parent_node_index() == node_index) {
					rating += nodes[edges[edge_index].get_child_node_index()].get_rating();
					num_children += 1;
					edge_index -= 1;
				}
				if (num_children > 1) rating /= num_children;
				node.set_rating(rating);
			}

			assert(edge_index == -1);
		}

		template<class Time> void generate_dot_file(
				const char *file_path,
				const Scheduling_problem<Time> &problem,
				const std::vector<Rating_graph_cut> &cuts
		) {
			FILE *file = fopen(file_path, "w");
			if (!file) {
				std::cout << "Failed to write to file " << file_path << std::endl;
				return;
			}

			std::vector<std::vector<size_t>> subgraph_node_mapping;
			subgraph_node_mapping.reserve(cuts.size());
			for (size_t cut_index = 0; cut_index < cuts.size(); cut_index++) {
				subgraph_node_mapping.emplace_back();
				auto &last = subgraph_node_mapping[subgraph_node_mapping.size() - 1];
				last.reserve(nodes.size());
				last.push_back(0);
				for (size_t node_index = 1; node_index < nodes.size(); node_index++) last.push_back(-2);
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
				if (nodes[node_index].get_rating() == 0.0f) fprintf(file, ", color=red");
				if (nodes[node_index].get_rating() == 1.0f) fprintf(file, ", color=green");
				fprintf(file, "];\n");
				if (should_visit_nodes[node_index] == 1) continue;

				while (edge_index < edges.size() && edges[edge_index].get_parent_node_index() == node_index) {
					size_t destination_node_index = edges[edge_index].get_child_node_index();
					should_visit_nodes[destination_node_index] = nodes[destination_node_index].get_rating() == 1.0f ? 1 : 2;
					const auto job = problem.jobs[edges[edge_index].get_taken_job_index()].get_id();
					fprintf(
							file, "\tnode%lu -> node%lu [label=\"T%luJ%lu (%zu)\"",
							node_index, destination_node_index, job.task, job.job, edges[edge_index].get_taken_job_index()
					);

					for (size_t cut_index = 0; cut_index < cuts.size(); cut_index++) {
						size_t mapped_source_node = subgraph_node_mapping[cut_index][node_index];
						if (mapped_source_node >= nodes.size()) continue;
						size_t mapped_destination_node = cuts[cut_index].previous_jobs->can_take_job(
								mapped_source_node, edges[edge_index].get_taken_job_index()
						);
						subgraph_node_mapping[cut_index][destination_node_index] = mapped_destination_node;
						if (!cuts[cut_index].previous_jobs->is_leaf(mapped_source_node)) continue;

						bool is_forbidden = false;
						for (const auto forbidden_job : cuts[cut_index].forbidden_jobs) {
							if (edges[edge_index].get_taken_job_index() == forbidden_job) {
								is_forbidden = true;
								break;
							}
						}

						if (is_forbidden) {
							fprintf(file, ", color=red");
							break;
						}
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
			return (rating_graph->nodes[destination_attachment->index].get_rating() != -1.0f) ==
					(rating_graph->nodes[parent_attachment->index].get_rating() != -1.0f);
		}
	};
}

#endif
