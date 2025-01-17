#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "time.hpp"
#include "problem.hpp"
#include "feasibility/simple_bounds.hpp"
#include "feasibility/load.hpp"

using namespace NP;
using namespace NP::Feasibility;

TEST_CASE("Simple tight feasible case all arrive at time 0") {
    std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(5, 5), 16, 0, 0, 0}, // stack = 11
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 10, 1, 1, 1}, // slack = 7
            Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(8, 8), 11, 2, 2, 2}, // slack = 3
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
    Load_test<dtime_t> load(problem, bounds);

    REQUIRE(load.next());
    REQUIRE(load.get_current_time() == 3); // Latest safe start time of job 2 is 3
    REQUIRE(load.get_minimum_executed_load() == 0); // It is possible to schedule job 2 at time 3, before running any other jobs
    REQUIRE(load.get_maximum_executed_load() == 3); // Plenty of jobs arrive at time 0, so it could have done something those 3 time units

    REQUIRE(load.next());
    REQUIRE(load.get_current_time() == 7); // Latest safe start time of job 1 is 7

    // Since there is only 1 core and both jobs 1 and 2 must have started at or before time 7, at least 1 of them must have finished. There are 2 possibilities:
    // - job 2 has finished and job 1 starts at time min(7, 7) -> requiring a finished load of 8 time units
    // - job 1 has finished and job 2 starts at time min(3, 7) -> requiring a finished load of 3 time units.
    //   also, job 2 must have spent 4 time units running already -> 3 + 4 = 7
    // The minimum of these cases is 7, so 7 time units is the minimum
    REQUIRE(load.get_minimum_executed_load() == 7);
    REQUIRE(load.get_maximum_executed_load() == 7);

    load.next();
    REQUIRE(load.get_current_time() == 11);
    REQUIRE(load.get_minimum_executed_load() == 11);
    REQUIRE(load.get_maximum_executed_load() == 11);

    CHECK(!load.next());
}

TEST_CASE("Hm...") {
    // Job 1 has slack 12 and exec time 6
    // Job 2 has slack 14 and exec time 5

    // at time 14, either:
    // job 1 has finished and job 2 starts: cost = 6 + 0 = 6
    // job 2 has finished and job 1 starts: cost = 5 + 2 = 7

    // job 1 has a finished cost of 6 and a starting cost of 2 -> relative cost is 4. also, maximum remaining time is 4
    // job 2 has a finished cost of 5 and a starting cost of 0 -> relative cost is 5. also, maximum remaining time is 5
}

#endif
