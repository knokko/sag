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

TEST_CASE("Feasibility from scratch on 1-core modified classic counter-example where least-slack-time is not optimal") {
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
	REQUIRE(!generator1.has_finished());

	// Even though job 2 has more slack than job 1, the generator should be smart enough to see that NOT picking job 2
	// will cause job 2 to miss its deadline
	REQUIRE(generator1.choose_next_job() == 2);
	REQUIRE(!generator1.has_finished());
	REQUIRE(generator1.choose_next_job() == 1);
	REQUIRE(generator1.has_finished());
	REQUIRE(!generator1.has_failed());
}

TEST_CASE("Feasibility from scratch on 2-core modified classic counter-example where least-slack-time is not optimal") {
	const std::vector<Job<dtime_t>> jobs{
		{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 10, 0, 0, 0}, // 7 slack
		{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 10, 1, 1, 1}, // 7 slack
		{2, Interval<dtime_t>(2, 2), Interval<dtime_t>(6, 6), 14, 2, 2, 2}, // 8 slack
		{3, Interval<dtime_t>(2, 2), Interval<dtime_t>(6, 6), 14, 3, 3, 3}, // 8 slack
		{4, Interval<dtime_t>(3, 3), Interval<dtime_t>(4, 4), 12, 4, 4, 4}, // 9 slack
		{5, Interval<dtime_t>(3, 3), Interval<dtime_t>(4, 4), 12, 5, 5, 5}, // 9 slack
	};
	const Scheduling_problem<dtime_t> problem(jobs, 2);

	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);

	Ordering_generator<dtime_t> generator1(problem, bounds, predecessor_mapping, 0);
	Job_index j1 = generator1.choose_next_job();
	REQUIRE(j1 <= 1);
	Job_index j2 = generator1.choose_next_job();
	REQUIRE(j2 <= 1);
	REQUIRE(j1 != j2);

	Job_index j3 = generator1.choose_next_job();
	REQUIRE(j3 > 1);
	REQUIRE(j3 <= 3);
	Job_index j4 = generator1.choose_next_job();
	REQUIRE(j4 >= 4);

	Job_index j5 = generator1.choose_next_job();
	CHECK(j5 >= 4);
	CHECK(j5 != j4);
	generator1.choose_next_job();
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

TEST_CASE("Search safe job ordering on 1-core modified classic counter-example where least-slack-time is not optimal") {
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

TEST_CASE("Search safe job ordering on 2-core modified classic counter-example where least-slack-time is not optimal") {
	const std::vector<Job<dtime_t>> jobs{
		{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 10, 0, 0, 0}, // 7 slack
		{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(3, 3), 10, 1, 1, 1}, // 7 slack
		{2, Interval<dtime_t>(2, 2), Interval<dtime_t>(6, 6), 14, 2, 2, 2}, // 8 slack
		{3, Interval<dtime_t>(2, 2), Interval<dtime_t>(6, 6), 14, 3, 3, 3}, // 8 slack
		{4, Interval<dtime_t>(3, 3), Interval<dtime_t>(4, 4), 12, 4, 4, 4}, // 9 slack
		{5, Interval<dtime_t>(3, 3), Interval<dtime_t>(4, 4), 12, 5, 5, 5}, // 9 slack
	};
	const Scheduling_problem<dtime_t> problem(jobs, 2);

	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);

	const auto safe_ordering = search_for_safe_job_ordering(problem, bounds, predecessor_mapping, 50, false);
	REQUIRE(safe_ordering.size() == 6);
	CHECK(safe_ordering[0] <= 1);
	CHECK(safe_ordering[1] <= 1);
	CHECK(safe_ordering[0] != safe_ordering[1]);
	for (size_t index = 2; index < 6; index++) CHECK(safe_ordering[index] > 1);
	CHECK(safe_ordering[2] > 3);
	CHECK(safe_ordering[3] > 3);
	CHECK(safe_ordering[2] <= 5);
	CHECK(safe_ordering[3] <= 5);
	CHECK(safe_ordering[2] != safe_ordering[3]);
	CHECK(safe_ordering[4] <= 3);
	CHECK(safe_ordering[5] <= 3);
	CHECK(safe_ordering[4] != safe_ordering[5]);
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
