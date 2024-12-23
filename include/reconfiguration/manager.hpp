#ifndef RECONFIGURATION_MANAGER_H
#define RECONFIGURATION_MANAGER_H

#include "clock.hpp"
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
		Rating_graph rating_graph;
		Agent_rating_graph<Time>::generate(problem, rating_graph);

		if (rating_graph.nodes[0].get_rating() == 1.0f) {
			std::cout << "The given problem is already schedulable using our scheduler" << std::endl;
			return;
		}

		// TODO Well... do something
	}
}
#endif
