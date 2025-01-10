#ifndef RECONFIGURATION_MANAGER_H
#define RECONFIGURATION_MANAGER_H

#include "clock.hpp"
#include "feasibility/graph.hpp"
#include "feasibility/simple_bounds.hpp"
#include "feasibility/z3.hpp"
#include "rating_graph.hpp"
#include "global/space.hpp"

namespace NP::Reconfiguration {
	struct Options {
		bool enabled;
	};

	template<class Time> static void run(Options &options, NP::Scheduling_problem<Time> &problem) {
		Processor_clock cpu_time;
		cpu_time.start();

		inner_reconfigure(options, problem);

		double spent_time = cpu_time.stop();
		std::cout << "Reconfiguration took " << spent_time << " seconds and consumed " <<
				(get_peak_memory_usage() / 1024) << "MB of memory" << std::endl;
	}

	template<class Time> static void inner_reconfigure(Options &options, NP::Scheduling_problem<Time> &problem) {
		const auto bounds = Feasibility::compute_simple_bounds(problem);
		if (bounds.definitely_infeasible) {
			if (bounds.has_precedence_cycle) {
				std::cout << "The given problem is infeasible because it has a precedence constraint cycle:"
						  << std::endl;
				for (size_t index = 1; index < bounds.problematic_chain.size(); index++) {
					std::cout << " - " << problem.jobs[bounds.problematic_chain[index]].get_id() << " depends on " <<
							  problem.jobs[bounds.problematic_chain[index - 1]].get_id() << std::endl;
				}
			} else {
				std::cout << "The given problem is infeasible because a deadline will be missed when all jobs "
							 "arrive at their latest arrival time and run for their worst-case execution time:"
						  << std::endl;
				const auto &first_job = problem.jobs[bounds.problematic_chain[0]];
				std::cout << " - " << first_job.get_id() << " arrives at " << first_job.latest_arrival() <<
						  " and finishes at " << (first_job.latest_arrival() + first_job.maximal_exec_time())
						  << std::endl;
				for (size_t index = 1; index < bounds.problematic_chain.size(); index++) {
					const auto &next_job = problem.jobs[bounds.problematic_chain[index]];
					const auto &previous_job = problem.jobs[bounds.problematic_chain[index - 1]];
					std::cout << " - Since " << next_job.get_id() << " depends on " << previous_job.get_id()
							  << ", it can only start at ";
					Time next_start_time = bounds.earliest_pessimistic_start_times[next_job.get_job_index()];
					std::cout << next_start_time;
					if (next_start_time > bounds.earliest_pessimistic_start_times[previous_job.get_job_index()] +
										  previous_job.maximal_exec_time()) {
						std::cout << " (due to suspension)";
					}
					std::cout << ", and finishes at " << (next_start_time + next_job.maximal_exec_time()) << std::endl;
				}

				const auto &last_job = problem.jobs[bounds.problematic_chain[bounds.problematic_chain.size() - 1]];
				std::cout << "However, the deadline of " << last_job.get_id() << " is " << last_job.get_deadline()
						  << std::endl;
			}
			return;
		}

		std::cout << "The problem has passed the cheap non-conclusive feasibility check; determining schedulability..." << std::endl;

		Rating_graph rating_graph;
		Agent_rating_graph<Time>::generate(problem, rating_graph);

		if (rating_graph.nodes[0].get_rating() == 1.0f) {
			std::cout << "The given problem is already schedulable using our scheduler" << std::endl;
			return;
		}

		std::cout << "The given problem is unschedulable using our scheduler" << std::endl;
		std::cout << "Checking feasibility..." << std::endl;

		if (problem.jobs.size() < 50) {
			rating_graph.generate_dot_file("nptest.dot", problem, std::vector<Rating_graph_cut>());
			generate_z3("nptest.z3", problem, bounds);
		}

		Feasibility::Feasibility_graph<Time> feasibility_graph(rating_graph);
		const auto predecessor_mapping = Feasibility::create_predecessor_mapping(problem);
		feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);
		feasibility_graph.explore_backward();

		// TODO Well... do something
	}
}
#endif
