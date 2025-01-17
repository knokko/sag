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
	CHECK(load.get_current_time() == 3); // Latest safe start time of job 2 is 3
	CHECK(load.get_minimum_executed_load() == 0); // It is possible to schedule job 2 at time 3, before running any other jobs
	CHECK(load.get_maximum_executed_load() == 3); // Plenty of jobs arrive at time 0, so it could have done something those 3 time units

	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 7); // Latest safe start time of job 1 is 7

	// Since there is only 1 core and both jobs 1 and 2 must have started at or before time 7, at least 1 of them must have finished. There are 2 possibilities:
	// - job 2 has finished and job 1 starts at time min(7, 7) -> requiring a finished load of 8 time units
	// - job 1 has finished and job 2 starts at time min(3, 7) -> requiring a finished load of 3 time units.
	//   also, job 2 must have spent 4 time units running already -> 3 + 4 = 7
	// The minimum of these cases is 7, so 7 time units is the minimum
	CHECK(load.get_minimum_executed_load() == 7);
	CHECK(load.get_maximum_executed_load() == 7);

	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 11);
	CHECK(load.get_minimum_executed_load() == 11);
	CHECK(load.get_maximum_executed_load() == 11);

	CHECK(!load.next());
	CHECK(!load.is_certainly_infeasible());
}

TEST_CASE("Simple tight infeasible case all arrive at time 0") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(5, 5), 16, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 10, 1, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(8, 8), 10, 2, 2, 2}, // note that the deadline is 1 unit earlier than in the first test case
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);

	REQUIRE(load.next());
	CHECK(load.get_current_time() == 2);
	CHECK(load.get_minimum_executed_load() == 0);
	CHECK(load.get_maximum_executed_load() == 2);

	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 7);

	// Since there is only 1 core and both jobs 1 and 2 must have started at or before time 7, at least 1 of them must have finished. There are 2 possibilities:
	// - job 2 has finished and job 1 starts at time min(7, 7) -> requiring a finished load of 8 time units
	// - job 1 has finished and job 2 starts at time min(2, 7) -> requiring a finished load of 3 time units.
	//   also, job 2 must have spent 5 time units running already -> 3 + 5 = 8
	// The minimum of these cases is 7, so 7 time units is the minimum
	CHECK(load.get_minimum_executed_load() == 8);
	CHECK(load.get_maximum_executed_load() == 7);
	CHECK(load.is_certainly_infeasible());
	CHECK(!load.next());
}

TEST_CASE("Feasible case where scheduling the longest task first is smart") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 3), Interval<dtime_t>(1, 6), 18, 0, 0, 0}, // stack = 12
			Job<dtime_t>{1, Interval<dtime_t>(0, 4), Interval<dtime_t>(1, 5), 19, 1, 1, 1}, // slack = 14
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);

	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 12);
	CHECK(load.get_minimum_executed_load() == 0);
	CHECK(load.get_maximum_executed_load() == 9); // Job 0 can't arrive later than time 3, so it could have used 12 - 3 = 9 time units

	// At time 14, either:
	// - job 0 has finished and job 1 starts: required execution time = 6 + 0 = 6
	// - job 1 has finished and job 0 starts: required execution time = 5 + 2 = 7
	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 14);
	CHECK(load.get_minimum_executed_load() == 6);
	CHECK(load.get_maximum_executed_load() == 11);

	CHECK(!load.next());
	CHECK(!load.is_certainly_infeasible());
}

TEST_CASE("Feasible case where scheduling the shortest task first is smart") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 3), Interval<dtime_t>(1, 6), 18, 0, 0, 0}, // stack = 12
			Job<dtime_t>{1, Interval<dtime_t>(0, 4), Interval<dtime_t>(1, 7), 20, 1, 1, 1}, // slack = 13
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);

	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 12);
	CHECK(load.get_minimum_executed_load() == 0);
	CHECK(load.get_maximum_executed_load() == 9); // Job 0 can't arrive later than time 3, so it could have used 12 - 3 = 9 time units

	// At time 13, either:
	// - job 0 has finished and job 1 starts: required execution time = 6 + 0 = 6
	// - job 1 has finished and job 0 starts: required execution time = 7 + 1 = 8
	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 13);
	CHECK(load.get_minimum_executed_load() == 6);
	CHECK(load.get_maximum_executed_load() == 10);

	CHECK(!load.next());
	CHECK(!load.is_certainly_infeasible());
}

#endif
