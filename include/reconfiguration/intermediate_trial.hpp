#ifndef INTERMEDIATE_TRIAL_HPP
#define INTERMEDIATE_TRIAL_HPP

#include "problem.hpp"
#include "memory_used.hpp"
#include "global/space.hpp"

namespace NP::Reconfiguration {
    template<class Time> static bool is_schedulable(const Scheduling_problem<Time> &problem, bool print_info) {
        const auto space = Global::State_space<Time>::explore(problem, {}, nullptr);
        const bool result = space->is_schedulable(); // TODO Print memory
        if (print_info) {
            long megabytes = get_peak_memory_usage() / 1024;
            std::cout << "    intermediate exploration: schedulable? " << result << " time: " << space->get_cpu_time() << " peak memory up until now: " << megabytes;
            std::cout << "MB #nodes: " << space->number_of_nodes() << " #edges: " << space->number_of_edges() << std::endl;
        }
        delete space;
        return result;
    }
}

#endif
