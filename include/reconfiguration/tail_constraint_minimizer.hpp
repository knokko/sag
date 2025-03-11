#ifndef RECONFIGURATION_TAIL_CONSTRAINT_MINIMIZER_H
#define RECONFIGURATION_TAIL_CONSTRAINT_MINIMIZER_H

#include <future>

#include "problem.hpp"
#include "global/space.hpp"
#include "remove_constraints.hpp"
#include "intermediate_trial.hpp"
#include "timeout.hpp"

namespace NP::Reconfiguration {
	template<class Time> class Tail_constraint_minimizer {
		Scheduling_problem<Time> &problem;
		const int num_original_constraints;
		int num_required_constraints = 0;

		int get_remaining_constraints(Scheduling_problem<Time> &target_problem) const {
			return target_problem.prec.size() - num_original_constraints - num_required_constraints;
		}

		void remove_from(Scheduling_problem<Time> &target_problem, int amount) const {
			assert(amount <= get_remaining_constraints(target_problem));
			std::vector<size_t> indices_to_remove;
			indices_to_remove.reserve(amount);
			{
				size_t next_index = target_problem.prec.size() - num_required_constraints - amount;
				while (indices_to_remove.size() < amount) {
					indices_to_remove.push_back(next_index);
					next_index += 1;
				}
			}
			remove_constraints(target_problem, indices_to_remove);
		}
	public:
		Tail_constraint_minimizer(
			Scheduling_problem<Time> &problem, int num_original_constraints
		) : problem(problem), num_original_constraints(num_original_constraints) {}

		bool can_remove(int amount, bool print_info) const {
			auto copied_problem = problem;
			remove_from(copied_problem, amount);
			return is_schedulable(copied_problem, print_info);
		}

		int try_to_remove(int num_threads, int &num_constraints_per_trial, bool print_info) {
			if (num_threads == 1) {
				if (num_constraints_per_trial <= 0) throw std::invalid_argument("#constraints per trial must be positive");
				if (num_constraints_per_trial > get_remaining_constraints(problem)) throw std::invalid_argument("too many constraints per trial");
				if (can_remove(num_constraints_per_trial, print_info)) {
					remove_from(problem, num_constraints_per_trial);
					int result = num_constraints_per_trial;
					num_constraints_per_trial = std::min(2 * num_constraints_per_trial, get_remaining_constraints(problem));
					return result;
				} else if (num_constraints_per_trial > 1 && can_remove(1, print_info)) {
					remove_from(problem, 1);
					num_constraints_per_trial = std::min(num_constraints_per_trial / 2, get_remaining_constraints(problem));
					return 1;
				} else {
					num_required_constraints += 1;
					num_constraints_per_trial = std::min(num_constraints_per_trial, get_remaining_constraints(problem));
					return 0;
				}
			}

			std::vector<std::future<bool>> trial_threads;
			trial_threads.reserve(num_threads);
			int amount = 1;
			for (int power = 0; power < num_threads; power++) {
				trial_threads.push_back(std::async(&Tail_constraint_minimizer<Time>::can_remove, this, amount, print_info));
				int old_amount = amount;
				amount = std::min(static_cast<size_t>(2 * amount), problem.prec.size() - num_required_constraints - num_original_constraints);
				if (amount == old_amount) break;
			}

			amount = 1;
			int largest_amount = 0;
			for (auto &result : trial_threads) {
				if (result.get()) largest_amount = amount;
				amount = std::min(static_cast<size_t>(2 * amount), problem.prec.size() - num_required_constraints - num_original_constraints);
			}

			if (largest_amount > 0) remove_from(problem, largest_amount);
			else num_required_constraints += 1;

			return largest_amount;
		}

		void remove_constraints_until_finished(int num_threads, double timeout, bool print_progress) {
			const auto start_time = std::chrono::high_resolution_clock::now();
			while (true) {
				const size_t before = problem.prec.size();
				num_required_constraints = 0;
				int num_constraints_per_trial = std::min(10, get_remaining_constraints(problem));
				while (get_remaining_constraints(problem) > 0) {
					const size_t inner_before = problem.prec.size();
					try_to_remove(num_threads, num_constraints_per_trial, print_progress);
					if (print_progress && problem.prec.size() != inner_before) {
						std::cout << " reduced #extra constraints to " << (problem.prec.size() - num_original_constraints) << std::endl;
					}
					if (did_exceed_timeout(timeout, start_time)) {
						std::cout << "Tail constraint minimization timed out" << std::endl;
						return;
					}
				}
				if (before == problem.prec.size()) break;
			}
		}
	};
}

#endif
