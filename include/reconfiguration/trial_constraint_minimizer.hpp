#ifndef RECONFIGURATION_TRIAL_CONSTRAINT_MINIMIZER_H
#define RECONFIGURATION_TRIAL_CONSTRAINT_MINIMIZER_H

#include <cstdlib>
#include <unordered_set>

#include "problem.hpp"
#include "global/space.hpp"
#include "remove_constraints.hpp"

namespace NP::Reconfiguration {

	template<class Time> class Trial_constraint_minimizer {
		Scheduling_problem<Time> &problem;
		size_t num_original_constraints;
	public:
		Trial_constraint_minimizer(
			Scheduling_problem<Time> &problem, size_t num_original_constraints
		) : problem(problem), num_original_constraints(num_original_constraints) {}

		bool can_remove(std::vector<size_t> constraint_indices_to_remove) {
			auto reduced_problem = problem;
			remove_constraints(reduced_problem, constraint_indices_to_remove);
			const auto space = Global::State_space<Time>::explore(reduced_problem, {}, nullptr);
			bool result = space->is_schedulable();
			delete space;
			return result;
		}

		bool try_to_remove(std::vector<size_t> constraint_indices_to_remove) {
			if (can_remove(constraint_indices_to_remove)) {
				remove_constraints(problem, constraint_indices_to_remove);
				return true;
			} else return false;
		}

		void repeatedly_try_to_remove_random_constraints() {
			std::unordered_set<size_t> candidate_set;
			std::vector<size_t> candidate_vector;

			const int initial_dead_countdown = 20;
			int dead_countdown = initial_dead_countdown;
			size_t num_constraints_per_trial = 10;
			while (dead_countdown > 0 && problem.prec.size() > 1 + num_original_constraints) {
				candidate_set.clear();
				const size_t num_candidates = problem.prec.size() - num_original_constraints;
				num_constraints_per_trial = std::max(static_cast<size_t>(1), std::min(num_constraints_per_trial, num_candidates / 2));
				std::cout << "#candidates is " << num_candidates << " and #constraints/trial is " << num_constraints_per_trial << std::endl;

				while (candidate_set.size() < num_constraints_per_trial) {
					candidate_set.insert(num_original_constraints + (rand() % num_candidates));
				}
				candidate_vector.clear();
				for (const size_t index : candidate_set) candidate_vector.push_back(index);

				if (try_to_remove(candidate_vector)) {
					dead_countdown = initial_dead_countdown;
					num_constraints_per_trial *= 2;
				} else {
					if (num_constraints_per_trial == 1) dead_countdown -= 1;
					num_constraints_per_trial /= 4;
				}
			}
		}
	};
}

#endif
