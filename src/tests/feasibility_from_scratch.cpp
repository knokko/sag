#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "problem.hpp"
#include "feasibility/simple_bounds.hpp"
#include "feasibility/from_scratch.hpp"

using namespace NP;
using namespace NP::Feasibility;

TEST_CASE("Best paths history: is_prefix") {
	BestPathsHistory history{ .max_size=0 };
	CHECK(history.is_prefix({}, {}));
	CHECK(history.is_prefix({}, { 1 }));
	CHECK(!history.is_prefix({ 1 }, {}));
	CHECK(history.is_prefix({ 1 }, { 1 }));
	CHECK(!history.is_prefix({ 1 }, { 2 }));
	CHECK(history.is_prefix({ 1 }, { 1, 2 }));
	CHECK(history.is_prefix({ 1, 2 }, { 1, 2 }));
	CHECK(!history.is_prefix({ 2, 1 }, { 1, 2 }));
	CHECK(!history.is_prefix({ 2, 1 }, { 1 }));
}

TEST_CASE("Best paths history: max_size = 0") {
	BestPathsHistory history;
	CHECK(history.insert({}) == 0);
	CHECK(history.insert({ 1 }) == 0);
	CHECK(history.insert({ 5 }) == 0);
	CHECK(history.insert({ 1, 5 }) == 0);
	CHECK(history.paths.empty());

	std::vector<Job_index> result;
	history.suggest_prefix(result);
	CHECK(result.empty());
}

TEST_CASE("Best paths history: max_size = 1") {
	std::vector<Job_index> result;
	BestPathsHistory history{ .max_size=1 };
	CHECK(history.insert({}) == 0);
	history.suggest_prefix(result);
	CHECK(result.empty());

	CHECK(history.insert({ 1 }) == 2);
	CHECK(history.insert({ 5 }) == 0);
	REQUIRE(history.paths.size() == 1);
	REQUIRE(history.paths[0].size() == 1);
	CHECK(history.paths[0][0] == 1);
	history.suggest_prefix(result);
	CHECK(result.empty());

	CHECK(history.insert({ 5, 1 }) == 2);
	CHECK(history.insert({ 5 }) == 0);
	CHECK(history.insert({ 1 }) == 0);
	REQUIRE(history.paths.size() == 1);
	REQUIRE(history.paths[0].size() == 2);
	CHECK(history.paths[0][0] == 5);
	CHECK(history.paths[0][1] == 1);

	{
		bool found_empty = false;
		bool found_full = false;
		for (int counter = 0; counter < 1000; counter++) {
			history.suggest_prefix(result);
			if (result.empty()) {
				found_empty = true;
			} else {
				assert(result.size() == 1 && result[0] == 5);
				found_full = true;
			}
		}
		CHECK(found_empty);
		CHECK(found_full);
	}

	CHECK(history.insert({ 5, 1, 2 }) == 2);
	CHECK(history.insert({}) == 0);
	CHECK(history.insert({ 5, 1 }) == 0);
	CHECK(history.insert({ 1, 5 }) == 0);
	REQUIRE(history.paths.size() == 1);
	REQUIRE(history.paths[0].size() == 3);
	CHECK(history.paths[0][0] == 5);
	CHECK(history.paths[0][1] == 1);
	CHECK(history.paths[0][2] == 2);

	{
		std::vector<bool> found{ false, false, false };
		REQUIRE(found.size() == 3);

		for (int counter = 0; counter < 10000; counter++) {
			history.suggest_prefix(result);
			assert(result.size() <= 2);
			if (result.size() >= 1) assert(result[0] == 5);
			if (result.size() >= 2) assert(result[1] == 1);
			found[result.size()] = true;
		}
		for (int size = 0; size <= 2; size++) CHECK(found[size]);
	}
}

TEST_CASE("Best paths history: max_size = 2") {
	BestPathsHistory history{ .max_size=2 };
	CHECK(history.insert({}) == 0);
	CHECK(history.insert({ 1 }) == 2);
	CHECK(history.insert({}) == 0);
	CHECK(history.insert({ 5 }) == 1);
	CHECK(history.insert({ 8 }) == 0);
	REQUIRE(history.paths.size() == 2);
	REQUIRE(history.paths[0].size() == 1);
	CHECK(history.paths[0][0] == 1);
	REQUIRE(history.paths[1].size() == 1);
	CHECK(history.paths[1][0] == 5);

	CHECK(history.insert({ 1, 8 }) == 2);
	CHECK(history.insert({ 5 }) == 0);
	CHECK(history.insert({ 1 }) == 0);
	REQUIRE(history.paths.size() == 2);
	REQUIRE(history.paths[0].size() == 2);
	CHECK(history.paths[0][0] == 1);
	CHECK(history.paths[0][1] == 8);
	REQUIRE(history.paths[1].size() == 1);
	CHECK(history.paths[1][0] == 5);

	CHECK(history.insert({ 5, 1, 2 }) == 2);
	CHECK(history.insert({}) == 0);
	CHECK(history.insert({ 5, 1, 2 }) == 0);
	CHECK(history.insert({ 5, 1 }) == 0);
	CHECK(history.insert({ 1, 5 }) == 0);
	REQUIRE(history.paths.size() == 2);
	REQUIRE(history.paths[0].size() == 3);
	CHECK(history.paths[0][0] == 5);
	CHECK(history.paths[0][1] == 1);
	CHECK(history.paths[0][2] == 2);
	REQUIRE(history.paths[1].size() == 2);
	CHECK(history.paths[1][0] == 1);
	CHECK(history.paths[1][1] == 8);

	CHECK(history.insert({ 2, 1, 3 }) == 1);
	REQUIRE(history.paths.size() == 2);
	REQUIRE(history.paths[0].size() == 3);
	CHECK(history.paths[0][0] == 5);
	CHECK(history.paths[0][1] == 1);
	CHECK(history.paths[0][2] == 2);
	REQUIRE(history.paths[1].size() == 3);
	CHECK(history.paths[1][0] == 2);
	CHECK(history.paths[1][1] == 1);
	CHECK(history.paths[1][2] == 3);

	std::vector<Job_index> result;
	std::vector<bool> found1{ false, false, false };
	std::vector<bool> found2{ false, false, false };

	for (int counter = 0; counter < 10000; counter++) {
		history.suggest_prefix(result);
		assert(result.size() <= 2);
		if (result.size() == 0) {
			found1[0] = true;
			found2[0] = true;
			continue;
		}
		if (result[0] == 5) {
			if (result.size() >= 1) assert(result[0] == 5);
			if (result.size() >= 2) assert(result[1] == 1);
			found1[result.size()] = true;
		} else if (result[0] == 2) {
			if (result.size() >= 1) assert(result[0] == 2);
			if (result.size() >= 2) assert(result[1] == 1);
			found2[result.size()] = true;
		} else assert(false);
	}
	for (int size = 0; size <= 2; size++) {
		CHECK(found1[size]);
		CHECK(found2[size]);
	}
}

TEST_CASE("Feasibility from scratch on very simple problem") {
	const std::vector<Job<dtime_t>> jobs{
		{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(10, 10), 15, 0, 0, 0},
		{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 1), 5, 1, 1, 1}
	};
	const Scheduling_problem<dtime_t> problem(jobs);

	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);

	Ordering_generator<dtime_t> generator(problem, bounds, predecessor_mapping, { .job_skip_chance=0 });
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

	const auto safe_ordering = search_for_safe_job_ordering(problem, bounds, predecessor_mapping, { .job_skip_chance=0 }, 1, false);
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

	Ordering_generator<dtime_t> generator1(problem, bounds, predecessor_mapping, { .job_skip_chance=0 });
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

	Ordering_generator<dtime_t> generator1(problem, bounds, predecessor_mapping, { .job_skip_chance=0 });
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
	for (int counter = 0; counter < 10000; counter++) {
		Ordering_generator<dtime_t> generator2(problem, bounds, predecessor_mapping, { .job_skip_chance=50 });
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

	const auto safe_ordering = search_for_safe_job_ordering(problem, bounds, predecessor_mapping, { .job_skip_chance=50 }, 5, false);
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

	const auto safe_ordering = search_for_safe_job_ordering(problem, bounds, predecessor_mapping, { .job_skip_chance=50 }, 20, false);
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

	Ordering_generator<dtime_t> generator(problem, bounds, predecessor_mapping, { .job_skip_chance=0 });
	REQUIRE(generator.choose_next_job() == 5);
	for (size_t counter = 0; counter < jobs.size() - 1; counter++) {
		REQUIRE(!generator.has_failed());
		REQUIRE(!generator.has_finished());
		REQUIRE(generator.choose_next_job() != 5);
	}
	REQUIRE(generator.has_finished());
	REQUIRE(!generator.has_failed());
}

TEST_CASE("Feasibility from scratch with a gap that can be filled with jobs that have more slack") {
	const std::vector<Job<dtime_t>> jobs{
		{0, Interval<dtime_t>(0, 90), Interval<dtime_t>(10, 10), 110, 0, 0, 0}, // 10 slack
		{1, Interval<dtime_t>(0, 20), Interval<dtime_t>(10, 10), 115, 1, 1, 1}, // 85 slack
		{2, Interval<dtime_t>(0, 10), Interval<dtime_t>(4, 10), 116, 2, 2, 2}, // 96 slack
		{3, Interval<dtime_t>(0, 50), Interval<dtime_t>(1, 10), 117, 3, 3, 3}, // 57 slack
	};
	const Scheduling_problem<dtime_t> problem(jobs);

	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);

	Ordering_generator<dtime_t> generator(problem, bounds, predecessor_mapping, { .job_skip_chance=0 });
	CHECK(generator.choose_next_job() == 2);
	CHECK(generator.choose_next_job() == 1);
	CHECK(generator.choose_next_job() == 3);
	CHECK(generator.choose_next_job() == 0);
	CHECK(generator.has_finished());
	CHECK(!generator.has_failed());
}

#endif
