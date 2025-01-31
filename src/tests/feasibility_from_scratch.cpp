#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "problem.hpp"
#include "feasibility/simple_bounds.hpp"
#include "feasibility/from_scratch.hpp"

using namespace NP;
using namespace NP::Feasibility;

TEST_CASE("Feasibility from scratch on very simple problem") {
	const std::vector<Job<dtime_t>> jobs{
		{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(10, 10), 15, 0, 0, 0},
		{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 1), 5, 1, 1, 1}
	};
	const Scheduling_problem<dtime_t> problem(jobs);

	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);

	Ordering_generator<dtime_t> generator(problem, bounds, predecessor_mapping, 0);
	REQUIRE(!generator.has_failed());
	REQUIRE(!generator.has_finished());
	REQUIRE(generator.choose_next_job() == 1);
	REQUIRE(!generator.has_failed());
	REQUIRE(!generator.has_finished());
	REQUIRE(generator.choose_next_job() == 0);
	REQUIRE(generator.has_finished());
	REQUIRE(!generator.has_failed());
}

TEST_CASE("Search safe job ordering on very simple problem") {
	const std::vector<Job<dtime_t>> jobs{
		{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(10, 10), 15, 0, 0, 0},
		{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 1), 5, 1, 1, 1}
	};
	const Scheduling_problem<dtime_t> problem(jobs);

	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);

	const auto safe_ordering = search_for_safe_job_ordering(problem, bounds, predecessor_mapping, 0, false);
	REQUIRE(safe_ordering.size() == 2);
	CHECK(safe_ordering[0] == 1);
	CHECK(safe_ordering[1] == 0);
}

TEST_CASE("Feasibility from scratch on modified classic counter-example where least-slack-time is not optimal") {
	const std::vector<Job<dtime_t>> jobs{
		{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 10, 0, 0, 0}, // 7 slack
		{1, Interval<dtime_t>(2, 2), Interval<dtime_t>(6, 6), 14, 1, 1, 1}, // 8 slack
		{2, Interval<dtime_t>(3, 3), Interval<dtime_t>(4, 4), 12, 2, 2, 2}, // 9 slack
	};
	const Scheduling_problem<dtime_t> problem(jobs);

	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);

	Ordering_generator<dtime_t> generator1(problem, bounds, predecessor_mapping, 0);
	REQUIRE(generator1.choose_next_job() == 0);
	REQUIRE(generator1.choose_next_job() == 1);
	REQUIRE(generator1.choose_next_job() == 2);
	REQUIRE(generator1.has_finished());
	REQUIRE(generator1.has_failed());

	bool has_succeeded = false;
	for (int counter = 0; counter < 100; counter++) {
		Ordering_generator<dtime_t> generator2(problem, bounds, predecessor_mapping, 50);
		while (!generator2.has_finished()) generator2.choose_next_job();
		if (generator2.has_failed()) continue;
		has_succeeded = true;
		break;
	}
	CHECK(has_succeeded);
}

TEST_CASE("Search safe job ordering on modified classic counter-example where least-slack-time is not optimal") {
	const std::vector<Job<dtime_t>> jobs{
		{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 10, 0, 0, 0}, // 7 slack
		{1, Interval<dtime_t>(2, 2), Interval<dtime_t>(6, 6), 14, 1, 1, 1}, // 8 slack
		{2, Interval<dtime_t>(3, 3), Interval<dtime_t>(4, 4), 12, 2, 2, 2}, // 9 slack
	};
	const Scheduling_problem<dtime_t> problem(jobs);

	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);

	const auto safe_ordering = search_for_safe_job_ordering(problem, bounds, predecessor_mapping, 50, false);
	REQUIRE(safe_ordering.size() == 3);
	CHECK(safe_ordering[0] == 0);
	CHECK(safe_ordering[1] == 2);
	CHECK(safe_ordering[2] == 1);
}

TEST_CASE("Feasibility from scratch when least-slack-time is invalid due to precedence constraints") {
	std::vector<Job<dtime_t>> jobs;
	for (size_t counter = 0; counter < 10; counter++) {
		jobs.emplace_back(counter, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 1), 15, 0, counter, counter);
	}
	std::vector<Precedence_constraint<dtime_t>> constraints;
	for (size_t counter = 0; counter < jobs.size(); counter++) {
		if (counter == 5) continue;
		constraints.emplace_back(jobs[5].get_id(), jobs[counter].get_id(), Interval<dtime_t>(), false);
	}

	// All jobs should get 15 slack, but only job 5 can be scheduled first due to dispatch ordering constraints
	const Scheduling_problem<dtime_t> problem(jobs, constraints);

	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	REQUIRE(!bounds.definitely_infeasible);

	Ordering_generator<dtime_t> generator(problem, bounds, predecessor_mapping, 0);
	REQUIRE(generator.choose_next_job() == 5);
	for (size_t counter = 0; counter < jobs.size() - 1; counter++) {
		REQUIRE(!generator.has_failed());
		REQUIRE(!generator.has_finished());
		REQUIRE(generator.choose_next_job() != 5);
	}
	REQUIRE(generator.has_finished());
	REQUIRE(!generator.has_failed());
}

#endif
