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

	// At time = 10:
	// - job 1 must have finished
	// - job 2 must have started, and must have completed at least 7 of the 8 time units
	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 10);
	CHECK(load.get_minimum_executed_load() == 10);
	CHECK(load.get_maximum_executed_load() == 10);

	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 11);
	CHECK(load.get_minimum_executed_load() == 11);
	CHECK(load.get_maximum_executed_load() == 11);

	// All jobs must have completed at time = 16
	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 16);
	CHECK(load.get_minimum_executed_load() == 16);
	CHECK(load.get_maximum_executed_load() == 16);

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

	// At time 18:
	// - job 0 must have finished
	// - job 1 must have executed for at least 4 of its 5 time units
	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 18);
	CHECK(load.get_minimum_executed_load() == 10);
	CHECK(load.get_maximum_executed_load() == 11);

	// At time 19, both jobs must have finished
	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 19);
	CHECK(load.get_minimum_executed_load() == 11);
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

	// At time 18:
	// - job 0 must have finished
	// - job 1 must have executed for at least 5 of its 7 time units
	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 18);
	CHECK(load.get_minimum_executed_load() == 11);
	CHECK(load.get_maximum_executed_load() == 13);

	// Both jobs must have finished at time 20
	REQUIRE(load.next());
	REQUIRE(load.get_current_time() == 20);
	CHECK(load.get_minimum_executed_load() == 13);
	CHECK(load.get_maximum_executed_load() == 13);

	CHECK(!load.next());
	CHECK(!load.is_certainly_infeasible());
}

TEST_CASE("Tight feasible case with more jobs and 2 cores") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(2, 2), Interval<dtime_t>(5, 5), 10, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(8, 13), 30, 1, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 25, 2, 2, 2},
			Job<dtime_t>{3, Interval<dtime_t>(0, 10), Interval<dtime_t>(2, 2), 25, 3, 3, 3},
			Job<dtime_t>{4, Interval<dtime_t>(0, 0), Interval<dtime_t>(7, 7), 20, 4, 4, 4},

			Job<dtime_t>{5, Interval<dtime_t>(2, 2), Interval<dtime_t>(5, 5), 10, 5, 5, 5},
			Job<dtime_t>{6, Interval<dtime_t>(0, 0), Interval<dtime_t>(8, 8), 25, 6, 6, 6},
			Job<dtime_t>{7, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 30, 7, 7, 7},
			Job<dtime_t>{8, Interval<dtime_t>(0, 10), Interval<dtime_t>(8, 8), 30, 8, 8, 8},
			Job<dtime_t>{9, Interval<dtime_t>(0, 0), Interval<dtime_t>(6, 6), 20, 9, 9, 9},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, {}, 2);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);
	while (load.next());
	CHECK(!load.is_certainly_infeasible());
	CHECK(load.get_current_time() == 30);
	CHECK(load.get_minimum_executed_load() == 60);
	CHECK(load.get_maximum_executed_load() == 60);
}

TEST_CASE("Tight infeasible case with more jobs and 2 cores") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(2, 2), Interval<dtime_t>(5, 5), 10, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(8, 13), 30, 1, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 25, 2, 2, 2},
			Job<dtime_t>{3, Interval<dtime_t>(0, 10), Interval<dtime_t>(2, 2), 25, 3, 3, 3},
			Job<dtime_t>{4, Interval<dtime_t>(0, 0), Interval<dtime_t>(7, 7), 20, 4, 4, 4},

			Job<dtime_t>{5, Interval<dtime_t>(2, 2), Interval<dtime_t>(5, 5), 10, 5, 5, 5},
			Job<dtime_t>{6, Interval<dtime_t>(0, 0), Interval<dtime_t>(8, 9), 25, 6, 6, 6}, // takes 1 time unit longer than in previous example
			Job<dtime_t>{7, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 30, 7, 7, 7},
			Job<dtime_t>{8, Interval<dtime_t>(0, 10), Interval<dtime_t>(8, 8), 30, 8, 8, 8},
			Job<dtime_t>{9, Interval<dtime_t>(0, 0), Interval<dtime_t>(6, 6), 20, 9, 9, 9},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, {}, 2);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);
	while (load.next());
	CHECK(load.is_certainly_infeasible());
}

TEST_CASE("Almost infeasible case due to early overload") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 1, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 2, 2, 2},

			Job<dtime_t>{3, Interval<dtime_t>(1, 8), Interval<dtime_t>(5, 5), 20, 3, 3, 3},

			Job<dtime_t>{3, Interval<dtime_t>(1, 30), Interval<dtime_t>(5, 5), 40, 4, 4, 4},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);
	while (load.next());
	CHECK(!load.is_certainly_infeasible());
}

TEST_CASE("Infeasible case due to early overload") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 4), 10, 1, 1, 1}, // Takes 1 time unit longer than in the previous test case
			Job<dtime_t>{2, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 2, 2, 2},

			Job<dtime_t>{3, Interval<dtime_t>(0, 10), Interval<dtime_t>(5, 5), 20, 3, 3, 3},

			Job<dtime_t>{3, Interval<dtime_t>(0, 30), Interval<dtime_t>(5, 5), 40, 4, 4, 4},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);
	while (load.next());
	CHECK(load.is_certainly_infeasible());
}

TEST_CASE("Almost infeasible case due to middle overload") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 12), Interval<dtime_t>(3, 3), 20, 2, 2, 2},
			Job<dtime_t>{3, Interval<dtime_t>(1, 12), Interval<dtime_t>(5, 5), 20, 3, 3, 3},

			Job<dtime_t>{3, Interval<dtime_t>(1, 30), Interval<dtime_t>(5, 5), 40, 4, 4, 4},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);
	while (load.next());
	CHECK(!load.is_certainly_infeasible());
}

TEST_CASE("Infeasible case due to middle overload") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 12), Interval<dtime_t>(3, 4), 20, 2, 2, 2}, // 1 time unit longer than in the previous example
			Job<dtime_t>{3, Interval<dtime_t>(0, 12), Interval<dtime_t>(5, 5), 20, 3, 3, 3},

			Job<dtime_t>{3, Interval<dtime_t>(0, 30), Interval<dtime_t>(5, 5), 40, 4, 4, 4},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);
	while (load.next());
	CHECK(load.is_certainly_infeasible());
}

TEST_CASE("Almost infeasible case due to late overload") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 12), Interval<dtime_t>(6, 6), 20, 2, 2, 2},

			Job<dtime_t>{3, Interval<dtime_t>(0, 30), Interval<dtime_t>(5, 5), 40, 3, 3, 3},
			Job<dtime_t>{3, Interval<dtime_t>(0, 30), Interval<dtime_t>(5, 5), 40, 4, 4, 4},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);
	while (load.next());
	CHECK(!load.is_certainly_infeasible());
}

TEST_CASE("Infeasible case due to late overload") {
	std::vector<Job<dtime_t>> jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 1), Interval<dtime_t>(3, 3), 10, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 12), Interval<dtime_t>(6, 6), 20, 2, 2, 2},

			Job<dtime_t>{3, Interval<dtime_t>(0, 30), Interval<dtime_t>(5, 6), 40, 3, 3, 3}, // takes 1 time unit longer than in previous test case
			Job<dtime_t>{3, Interval<dtime_t>(0, 30), Interval<dtime_t>(5, 5), 40, 4, 4, 4},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = compute_simple_bounds(problem);
	Load_test<dtime_t> load(problem, bounds);
	while (load.next());
	CHECK(load.is_certainly_infeasible());
}

#endif
