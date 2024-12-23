#ifndef RECONFIGURATION_MANAGER_H
#define RECONFIGURATION_MANAGER_H

#include "global/space.hpp"
#include "pessimistic.hpp"
#include "agent/failure_job_set.hpp"
#include "graph_strategy.hpp"

namespace NP::Reconfiguration {
	struct Options {
        bool enabled{};
        bool skip_pessimism{};
        bool skip_graph{};
    };

	template<class Time>
	class Manager {
	public:
		static int run_with_automatic_reconfiguration(const Options options, Scheduling_problem<Time> problem) {
			Index_collection interesting_jobs_for_pessimism;
			bool was_schedulable = Agent_failure_job_set_search<Time>::find_all_jobs_on_paths_to_deadline_misses(
					problem, &interesting_jobs_for_pessimism
			);

			if (was_schedulable) {
				std::cout << "The given problem is already schedulable.\n";
				return 0;
			}

			if (!options.skip_pessimism) {
				Pessimistic_reconfigurator<Time> pessimistic_reconfigurator(problem, &interesting_jobs_for_pessimism);
				auto pessimistic_solution = pessimistic_reconfigurator.find_local_minimal_solution();
				if (pessimistic_solution.size() > 0) {
					std::cout << "The given problem is not schedulable, but you can make it schedulable by following these steps:\n";
					for (auto solution : pessimistic_solution) solution->print();
					return 0;
				}
			}

			if (!options.skip_graph) {
				auto graph_solution = apply_graph_strategy(problem);
				if (graph_solution.size() > 0) {
					std::cout << "The given problem is not schedulable, but you can make it schedulable by following these steps:\n";
					for (auto solution : graph_solution) solution->print();
					return 0;
				}
			}

			std::cout << "The given problem is not schedulable, and I couldn't find a solution to fix it.\n";
			return 1;
		}
	};
}

#endif
