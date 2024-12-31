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

	const auto problem = Scheduling_problem<dtime_t>(jobs, std::vector<Precedence_constraint<dtime_t>>());
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

	const auto problem = Scheduling_problem<dtime_t>(jobs, std::vector<Precedence_constraint<dtime_t>>());
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
