#ifndef RECONFIGURATION_TRANSITIVITY_CONSTRAINT_MINIMIZER_H
#define RECONFIGURATION_TRANSITIVITY_CONSTRAINT_MINIMIZER_H

#include <unordered_set>
#include "problem.hpp"
#include "index_set.hpp"
#include "remove_constraints.hpp"

namespace NP::Reconfiguration {

	struct pair_hash {
		inline std::size_t operator()(const std::pair<int,int> & v) const {
			return v.first * 31 + v.second;
		}
	};

	template<class Time> class Transitivity_constraint_minimizer {
		Scheduling_problem<Time> &problem;
		size_t num_original_constraints;

		std::unordered_map<std::pair<Job_index, Job_index>, size_t, pair_hash> required_successor_constraints;
		Index_set jobs_with_extra_successors;
	public:
		Transitivity_constraint_minimizer(Scheduling_problem<Time> &problem, size_t num_original_constraints) : problem(problem), num_original_constraints(num_original_constraints) {
			for (size_t index = num_original_constraints; index < problem.prec.size(); index++) {
				const auto &constraint = problem.prec[index];
				required_successor_constraints[std::pair(constraint.get_fromIndex(), constraint.get_toIndex())] = index;
				jobs_with_extra_successors.add(constraint.get_fromIndex());
			}
		}

		void remove_redundant_constraints() {
			std::vector<size_t> all_redundant_constraints;
			for (Job_index job_index = 0; job_index < problem.jobs.size(); job_index++) {
				if (!jobs_with_extra_successors.contains(job_index)) continue;
				const auto redundant_constraints = find_redundant_constraint_indices(job_index);
				for (const auto &constraint_index : redundant_constraints) all_redundant_constraints.push_back(constraint_index);
			}
			remove_constraints(problem, all_redundant_constraints);
		}

		std::vector<size_t> find_redundant_constraint_indices(Job_index from_index) {
			std::vector<std::vector<size_t>> all_direct_successor_constraints(problem.jobs.size());
			for (size_t constraint_index = 0; constraint_index < problem.prec.size(); constraint_index++) {
				all_direct_successor_constraints[problem.prec[constraint_index].get_fromIndex()].push_back(constraint_index);
			}

			Index_set indirect_successors;
			std::unordered_set<size_t> redundant_constraint_index_set;

			std::vector<Job_index> next_successors{from_index};
			while (!next_successors.empty()) {
				const Job_index next_job = next_successors[next_successors.size() - 1];
				next_successors.pop_back();

				for (const size_t child_constraint : all_direct_successor_constraints[next_job]) {
					const auto &constraint = problem.prec[child_constraint];
					const Job_index child_job = constraint.get_toIndex();
					if (indirect_successors.contains(child_job)) {
						const std::pair key(from_index, child_job);
						if (required_successor_constraints.contains(key)) {
							redundant_constraint_index_set.insert(required_successor_constraints[key]);
						}
					} else {
						indirect_successors.add(child_job);
						next_successors.push_back(child_job);
					}
				}
			}

			std::vector<size_t> redundant_constraint_indices;
			redundant_constraint_indices.reserve(redundant_constraint_index_set.size());
			for (const size_t constraint_index : redundant_constraint_index_set) redundant_constraint_indices.push_back(constraint_index);
			return redundant_constraint_indices;
		}
	};
}

#endif
