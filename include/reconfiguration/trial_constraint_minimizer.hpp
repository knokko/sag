#ifndef RECONFIGURATION_TRIAL_CONSTRAINT_MINIMIZER_H
#define RECONFIGURATION_TRIAL_CONSTRAINT_MINIMIZER_H

#include <cstdlib>
#include <future>
#include <unordered_set>

#include "problem.hpp"
#include "global/space.hpp"
#include "remove_constraints.hpp"
#include "intermediate_trial.hpp"
#include "timeout.hpp"

namespace NP::Reconfiguration {

	template<class Time> static bool can_remove(
		Scheduling_problem<Time> problem, std::vector<size_t> constraint_indices_to_remove, bool print_info
	) {
		remove_constraints(problem, constraint_indices_to_remove);
		return is_schedulable(problem, print_info);
	}

	template<class Time> class Trial_constraint_minimizer {
		Scheduling_problem<Time> &problem;
		const size_t num_original_constraints;
		const int num_threads;
		const bool print_progress;
		size_t barrier_index;
		double timeout;

		void choose_candidate_set(std::unordered_set<size_t> scratch_set, std::vector<size_t> &destination, size_t num_candidates, size_t size) {
			scratch_set.clear();
			while (scratch_set.size() < size) {
				scratch_set.insert(barrier_index + (rand() % num_candidates));
			}

			destination.clear();
			destination.reserve(size);
			for (const size_t index : scratch_set) destination.push_back(index);
		}
	public:
		Trial_constraint_minimizer(
			Scheduling_problem<Time> &problem, size_t num_original_constraints,
			int num_threads, double timeout, bool print_progress
		) : problem(problem), num_original_constraints(num_original_constraints), num_threads(num_threads),
			print_progress(print_progress), barrier_index(num_original_constraints), timeout(timeout) {}

		void repeatedly_try_to_remove_random_constraints() {
			const auto start_time = std::chrono::high_resolution_clock::now();

			std::unordered_set<size_t> candidate_set;
			std::vector<std::vector<size_t>> candidate_vector(num_threads);
			assert(candidate_vector.size() == num_threads);

			bool made_progress = false;
			size_t num_constraints_per_trial = 10;
			while (problem.prec.size() > num_original_constraints + 1) {
				if (problem.prec.size() <= barrier_index) {
					if (!made_progress) break;
					barrier_index = num_original_constraints;
					made_progress = false;
				}

				if (did_exceed_timeout(timeout, start_time)) {
					std::cout << "Random constraint minimization timed out" << std::endl;
					break;
				}

				const size_t num_candidates = problem.prec.size() - barrier_index;
				num_constraints_per_trial = std::max(static_cast<size_t>(1), std::min(num_constraints_per_trial, num_candidates / 2));

				for (size_t thread_index = 0; thread_index < num_threads; thread_index++) {
					choose_candidate_set(candidate_set, candidate_vector[thread_index], num_candidates, num_constraints_per_trial);
				}

				std::vector<std::future<bool>> trial_threads;
				trial_threads.reserve(num_threads);
				for (size_t thread_index = 0; thread_index < num_threads; thread_index++) {
					trial_threads.push_back(std::async(&can_remove<Time>, problem, candidate_vector[thread_index], print_progress));
				}

				size_t success_index = -1;
				int success_counter = 0;
				for (size_t thread_index = 0; thread_index < num_threads; thread_index++) {
					if (trial_threads[thread_index].get()) success_index = thread_index;
				}

				if (success_index == -1) {
					if (num_constraints_per_trial == 1) {
						// When there is only 1 candidate per trial, we should mark all of them as 'definitely required',
						// by putting them before the barrier index
						for (const auto &small_vec : candidate_vector) {
							assert(small_vec.size() == 1);
							const size_t required_prec_index = small_vec[0];
							if (required_prec_index > barrier_index) std::swap(problem.prec[barrier_index], problem.prec[required_prec_index]);
							if (required_prec_index >= barrier_index) barrier_index += 1;
						}
					}
					num_constraints_per_trial /= 2;
				} else {
					remove_constraints(problem, candidate_vector[success_index]);
					made_progress = true;
					num_constraints_per_trial *= 2;
					if (print_progress) {
						std::cout << " reduced #extra constraints to " << (problem.prec.size() - num_original_constraints) << std::endl;
					}
				}
			}
		}
	};
}

#endif
