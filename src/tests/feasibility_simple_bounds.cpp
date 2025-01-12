#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "global/space.hpp"
#include "feasibility/simple_bounds.hpp"

using namespace NP;

TEST_CASE("Simple feasible bounds without precedence constraints") {
	Global::State_space<dtime_t>::Workload jobs{
			Job<dtime_t>{0, Interval<dtime_t>(0,  4), Interval<dtime_t>(1, 2), 10, 10, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(5, 10), Interval<dtime_t>(1, 3), 13, 20, 1, 1},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = Feasibility::compute_simple_bounds(problem);
	CHECK(!bounds.has_precedence_cycle);
	CHECK(!bounds.definitely_infeasible);
	REQUIRE(bounds.earliest_pessimistic_start_times.size() == 2);
	CHECK(bounds.earliest_pessimistic_start_times[0] == 4);
	CHECK(bounds.earliest_pessimistic_start_times[1] == 10);
	REQUIRE(bounds.latest_safe_start_times.size() == 2);
	CHECK(bounds.latest_safe_start_times[0] == 8);
	CHECK(bounds.latest_safe_start_times[1] == 10);
	REQUIRE(bounds.maximum_suspensions.size() == 2);
	CHECK(bounds.maximum_suspensions[0] == 0);
	CHECK(bounds.maximum_suspensions[1] == 0);
	CHECK(bounds.problematic_chain.empty());
}

TEST_CASE("Simple infeasible bounds without precedence constraints") {
	Global::State_space<dtime_t>::Workload jobs{
			Job<dtime_t>{0, Interval<dtime_t>(0,  4), Interval<dtime_t>(1, 2), 10, 10, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(5, 10), Interval<dtime_t>(1, 4), 13, 20, 1, 1},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto bounds = Feasibility::compute_simple_bounds(problem);
	CHECK(!bounds.has_precedence_cycle);
	CHECK(bounds.definitely_infeasible);
	REQUIRE(bounds.earliest_pessimistic_start_times.size() == 2);
	CHECK(bounds.earliest_pessimistic_start_times[0] == 4);
	CHECK(bounds.earliest_pessimistic_start_times[1] == 10);
	REQUIRE(bounds.latest_safe_start_times.size() == 2);
	CHECK(bounds.latest_safe_start_times[0] == 8);
	CHECK(bounds.latest_safe_start_times[1] == 9);
	REQUIRE(bounds.maximum_suspensions.size() == 2);
	CHECK(bounds.maximum_suspensions[0] == 0);
	CHECK(bounds.maximum_suspensions[1] == 0);
	REQUIRE(bounds.problematic_chain.size() == 1);
	CHECK(bounds.problematic_chain[0] == 1);
}

TEST_CASE("Simple feasible bounds with precedence chain") {
	Global::State_space<dtime_t>::Workload jobs{
			Job<dtime_t>{0, Interval<dtime_t>(0,  4), Interval<dtime_t>(1, 2), 10, 10, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(5, 10), Interval<dtime_t>(1, 3), 30, 20, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(5, 10), Interval<dtime_t>(8, 9), 35, 30, 2, 2},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints{
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(0, 5)),
			Precedence_constraint<dtime_t>(jobs[2].get_id(), jobs[1].get_id(), Interval<dtime_t>(1, 2))
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto bounds = Feasibility::compute_simple_bounds(problem);
	CHECK(!bounds.has_precedence_cycle);
	CHECK(!bounds.definitely_infeasible);
	REQUIRE(bounds.earliest_pessimistic_start_times.size() == 3);
	CHECK(bounds.earliest_pessimistic_start_times[0] == 4);
	CHECK(bounds.earliest_pessimistic_start_times[2] == 11); // 4 + 2 + 5
	CHECK(bounds.earliest_pessimistic_start_times[1] == 22); // 11 + 9 + 2
	REQUIRE(bounds.latest_safe_start_times.size() == 3);
	CHECK(bounds.latest_safe_start_times[0] == 8);
	CHECK(bounds.latest_safe_start_times[2] == 16);
	CHECK(bounds.latest_safe_start_times[1] == 27);
	REQUIRE(bounds.maximum_suspensions.size() == 3);
	CHECK(bounds.maximum_suspensions[0] == 5);
	CHECK(bounds.maximum_suspensions[1] == 0);
	CHECK(bounds.maximum_suspensions[2] == 2);
	CHECK(bounds.problematic_chain.empty());
}

TEST_CASE("Simple infeasible bounds with precedence chain") {
	Global::State_space<dtime_t>::Workload jobs{
			Job<dtime_t>{0, Interval<dtime_t>(0,  4), Interval<dtime_t>(1, 2), 10, 10, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(5, 10), Interval<dtime_t>(1, 3), 21, 20, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(5, 10), Interval<dtime_t>(8, 9), 35, 30, 2, 2},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints{
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(0, 5)),
			Precedence_constraint<dtime_t>(jobs[2].get_id(), jobs[1].get_id(), Interval<dtime_t>(1, 2))
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto bounds = Feasibility::compute_simple_bounds(problem);
	CHECK(!bounds.has_precedence_cycle);
	CHECK(bounds.definitely_infeasible);
	REQUIRE(bounds.earliest_pessimistic_start_times.size() == 3);
	CHECK(bounds.earliest_pessimistic_start_times[0] == 4);
	CHECK(bounds.earliest_pessimistic_start_times[2] == 11); // 4 + 2 + 5
	CHECK(bounds.earliest_pessimistic_start_times[1] == 22); // 11 + 9 + 2
	REQUIRE(bounds.latest_safe_start_times.size() == 3);
	CHECK(bounds.latest_safe_start_times[0] == 0);
	CHECK(bounds.latest_safe_start_times[2] == 7);
	CHECK(bounds.latest_safe_start_times[1] == 18);
	REQUIRE(bounds.maximum_suspensions.size() == 3);
	CHECK(bounds.maximum_suspensions[0] == 5);
	CHECK(bounds.maximum_suspensions[1] == 0);
	CHECK(bounds.maximum_suspensions[2] == 2);
	REQUIRE(bounds.problematic_chain.size() == 3);
	CHECK(bounds.problematic_chain[0] == 0);
	CHECK(bounds.problematic_chain[1] == 2);
	CHECK(bounds.problematic_chain[2] == 1);
}

TEST_CASE("Simple bounds with mixed signaling precedence chain") {
	Global::State_space<dtime_t>::Workload jobs{
			Job<dtime_t>{0, Interval<dtime_t>(0, 5), Interval<dtime_t>(1, 2), 8, 10, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(2, 7), Interval<dtime_t>(1, 3), 13, 20, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(5, 8), Interval<dtime_t>(8, 9), 19, 30, 2, 2},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints{
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>(0, 5), false),
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(1, 2))
	};

	// Test that #cores doesn't affect simple feasibility bounds
	for (int num_cores = 1; num_cores <= 2; num_cores++) {
		const auto problem1 = Scheduling_problem<dtime_t>(jobs, precedence_constraints, num_cores);
		const auto bounds1 = Feasibility::compute_simple_bounds(problem1);
		CHECK(!bounds1.has_precedence_cycle);
		CHECK(!bounds1.definitely_infeasible);
		REQUIRE(bounds1.earliest_pessimistic_start_times.size() == 3);
		CHECK(bounds1.earliest_pessimistic_start_times[0] == 5);
		CHECK(bounds1.earliest_pessimistic_start_times[1] == 10);
		CHECK(bounds1.earliest_pessimistic_start_times[2] == 9);
		REQUIRE(bounds1.latest_safe_start_times.size() == 3);
		CHECK(bounds1.latest_safe_start_times[0] == 5);
		CHECK(bounds1.latest_safe_start_times[1] == 10);
		CHECK(bounds1.latest_safe_start_times[2] == 10);
		REQUIRE(bounds1.maximum_suspensions.size() == 3);
		CHECK(bounds1.maximum_suspensions[0] == 5);
		CHECK(bounds1.maximum_suspensions[1] == 0);
		CHECK(bounds1.maximum_suspensions[2] == 0);
		CHECK(bounds1.problematic_chain.size() == 0);
	}

	precedence_constraints[0] = Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>(0, 5), true);
	const auto problem3 = Scheduling_problem<dtime_t>(jobs, precedence_constraints, 2);
	const auto bounds3 = Feasibility::compute_simple_bounds(problem3);
	CHECK(!bounds3.has_precedence_cycle);
	CHECK(bounds3.definitely_infeasible);
	REQUIRE(bounds3.earliest_pessimistic_start_times.size() == 3);
	CHECK(bounds3.earliest_pessimistic_start_times[0] == 5);
	CHECK(bounds3.earliest_pessimistic_start_times[1] == 12);
	REQUIRE(bounds3.latest_safe_start_times.size() == 3);
	CHECK(bounds3.latest_safe_start_times[0] == 3);
	CHECK(bounds3.latest_safe_start_times[1] == 10);
}

TEST_CASE("Simple infeasible self-cycle") {
	Global::State_space<dtime_t>::Workload jobs{
			Job<dtime_t>{0, Interval<dtime_t>(0,  4), Interval<dtime_t>(1, 2), 10, 10, 0, 0},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints{
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[0].get_id(), Interval<dtime_t>(0, 0)),
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto bounds = Feasibility::compute_simple_bounds(problem);
	CHECK(bounds.has_precedence_cycle);
	CHECK(bounds.definitely_infeasible);
	REQUIRE(bounds.earliest_pessimistic_start_times.size() == 0);
	REQUIRE(bounds.latest_safe_start_times.size() == 0);
	REQUIRE(bounds.maximum_suspensions.size() == 0);
	REQUIRE(bounds.problematic_chain.size() == 2);
	CHECK(bounds.problematic_chain[0] == 0);
	CHECK(bounds.problematic_chain[1] == 0);
}

TEST_CASE("Simple infeasible cycle") {
	Global::State_space<dtime_t>::Workload jobs{
			Job<dtime_t>{0, Interval<dtime_t>(0,  4), Interval<dtime_t>(1, 2), 10, 10, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(5, 10), Interval<dtime_t>(1, 3), 21, 20, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(5, 10), Interval<dtime_t>(8, 9), 35, 30, 2, 2},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints{
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(0, 5)),
			Precedence_constraint<dtime_t>(jobs[2].get_id(), jobs[1].get_id(), Interval<dtime_t>(1, 2)),
			Precedence_constraint<dtime_t>(jobs[1].get_id(), jobs[0].get_id(), Interval<dtime_t>(1, 2)),
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto bounds = Feasibility::compute_simple_bounds(problem);
	CHECK(bounds.has_precedence_cycle);
	CHECK(bounds.definitely_infeasible);
	REQUIRE(bounds.earliest_pessimistic_start_times.size() == 0);
	REQUIRE(bounds.latest_safe_start_times.size() == 0);
	REQUIRE(bounds.maximum_suspensions.size() == 0);
	REQUIRE(bounds.problematic_chain.size() == 4);
	CHECK(bounds.problematic_chain[0] == 0);
	CHECK(bounds.problematic_chain[1] == 2);
	CHECK(bounds.problematic_chain[2] == 1);
	CHECK(bounds.problematic_chain[3] == 0);
}

#endif
