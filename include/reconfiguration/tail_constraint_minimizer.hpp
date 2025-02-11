#ifndef RECONFIGURATION_TAIL_CONSTRAINT_MINIMIZER_H
#define RECONFIGURATION_TAIL_CONSTRAINT_MINIMIZER_H

#include <future>

#include "problem.hpp"
#include "global/space.hpp"
#include "remove_constraints.hpp"

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

		bool can_remove(int amount) const {
			auto copied_problem = problem;
			remove_from(copied_problem, amount);
			const auto space = Global::State_space<Time>::explore(copied_problem, {}, nullptr);
			bool result = space->is_schedulable();
			delete space;
			return result;
		}

		int try_to_remove(int num_threads) {
			std::vector<std::future<bool>> trial_threads;
			trial_threads.reserve(num_threads);
			int amount = 1;
			for (int power = 0; power < num_threads; power++) {
				trial_threads.push_back(std::async(&Tail_constraint_minimizer<Time>::can_remove, this, amount));
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

		void remove_constraints_until_finished(int num_threads, bool print_progress) {
			while (get_remaining_constraints(problem) > 0) {
				try_to_remove(num_threads);
				if (print_progress) std::cout << get_remaining_constraints(problem) << " potentially redundant constraints remain" << std::endl;
			}
		}
	};
}

#endif
