#ifndef RECONFIGURATION_TRIAL_CONSTRAINT_MINIMIZER_H
#define RECONFIGURATION_TRIAL_CONSTRAINT_MINIMIZER_H

#include <cstdlib>
#include <unordered_set>
#include <mutex>

#include "problem.hpp"
#include "global/space.hpp"
#include "remove_constraints.hpp"

namespace NP::Reconfiguration {

	template<class Time> class Trial_constraint_minimizer {
		std::shared_ptr<Scheduling_problem<Time>> shared_problem;
		size_t num_original_constraints;
		std::shared_ptr<std::mutex> lock;
		int num_threads;
		size_t expected_size = 0;
	public:
		Trial_constraint_minimizer(
			std::shared_ptr<Scheduling_problem<Time>> problem, size_t num_original_constraints,
			std::shared_ptr<std::mutex> lock = nullptr, int num_threads = 1
		) : shared_problem(problem), num_original_constraints(num_original_constraints), lock(std::move(lock)), num_threads(num_threads) {}

		bool can_remove(std::vector<size_t> constraint_indices_to_remove) {
			if (lock) lock->lock();
			auto current_problem = *shared_problem;
			if (lock) lock->unlock();

			expected_size = current_problem.prec.size();
			for (const size_t constraint_index : constraint_indices_to_remove) {
				if (constraint_index >= expected_size) return false;
			}

			remove_constraints(current_problem, constraint_indices_to_remove);
			const auto space = Global::State_space<Time>::explore(current_problem, {}, nullptr);
			bool result = space->is_schedulable();
			delete space;
			return result;
		}

		int try_to_remove(std::vector<size_t> constraint_indices_to_remove) {
			if (can_remove(constraint_indices_to_remove)) {
				int result;
				if (lock) lock->lock();
				// Don't update shared_problem if another thread modified it in the meantime
				if (shared_problem->prec.size() == expected_size) {
					remove_constraints(*shared_problem, constraint_indices_to_remove);
					result = 1;
					std::cout << "Reduced #extra constraints from " << (expected_size - num_original_constraints) <<
							" to " << (shared_problem->prec.size() - num_original_constraints) << std::endl;
				} else result = 2;
				if (lock) lock->unlock();
				return result;
			} else return 0;
		}

		void repeatedly_try_to_remove_random_constraints() {
			std::unordered_set<size_t> candidate_set;
			std::vector<size_t> candidate_vector;

			const int initial_dead_countdown = 50;
			int dead_countdown = initial_dead_countdown;
			size_t num_constraints_per_trial = 10;
			while (dead_countdown > 0) {
				size_t num_precedence_constraints;
				if (lock) lock->lock();
				num_precedence_constraints = shared_problem->prec.size();
				if (lock) lock->unlock();
				if (num_precedence_constraints <= 1 + num_original_constraints) break;

				const size_t num_candidates = num_precedence_constraints - num_original_constraints;
				num_constraints_per_trial = std::max(static_cast<size_t>(1), std::min(num_constraints_per_trial, num_candidates / 2));

				candidate_set.clear();
				while (candidate_set.size() < num_constraints_per_trial) {
					candidate_set.insert(num_original_constraints + (rand() % num_candidates));
				}
				candidate_vector.clear();
				for (const size_t index : candidate_set) candidate_vector.push_back(index);

				int remove_result = 2;
				while (remove_result == 2) remove_result = try_to_remove(candidate_vector);
				if (remove_result == 1) {
					dead_countdown = initial_dead_countdown;
					num_constraints_per_trial *= 4;
				} else if (remove_result == 0) {
					if (num_constraints_per_trial == 1) dead_countdown -= 1;
					num_constraints_per_trial /= 2;
				}
			}
		}
	};
}

#endif
