#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "problem.hpp"
#include "reconfiguration/transitivity_constraint_minimizer.hpp"

using namespace NP;
using namespace NP::Reconfiguration;

static Scheduling_problem<dtime_t> dummy_problem(std::vector<std::pair<Job_index, Job_index>> constraints) {
	Job_index largest = 0;
	for (const auto &constraint : constraints) {
		largest = std::max(largest, std::max(constraint.first, constraint.second));
	}

	std::vector<Job<dtime_t>> jobs;
	jobs.reserve(largest + 1);
	for (size_t index = 0; index <= largest; index++) {
		jobs.push_back(Job<dtime_t>(index, Interval<dtime_t>(), Interval<dtime_t>(1, 1), 5 * largest, index, index));
	}

	std::vector<Precedence_constraint<dtime_t>> prec;
	prec.reserve(constraints.size());
	for (const auto &constraint : constraints) {
		prec.push_back(Precedence_constraint<dtime_t>(jobs[constraint.first].get_id(), jobs[constraint.second].get_id(), Interval<dtime_t>()));
	}
	return Scheduling_problem<dtime_t>(jobs, prec);
}

TEST_CASE("Constraint minimizer simple case with only 1 redundant constraint") {
	auto problem = dummy_problem({{1, 3}, {2, 3}, {2, 1}});
	auto minimizer = Transitivity_constraint_minimizer<dtime_t>(problem, 1);

	// The constraing from job 2 -> 3 is redundant
	const auto redundant = minimizer.find_redundant_constraint_indices(2);
	REQUIRE(redundant.size() == 1);
	CHECK(redundant[0] == 1);

	minimizer.remove_redundant_constraints();
	REQUIRE(problem.prec.size() == 2);
	CHECK(problem.prec[0].get_fromIndex() == 1);
	CHECK(problem.prec[0].get_toIndex() == 3);
	CHECK(problem.prec[1].get_fromIndex() == 2);
	CHECK(problem.prec[1].get_toIndex() == 1);
}

TEST_CASE("Constraint minimizer chain") {
	auto problem = dummy_problem({{0, 1}, {1, 2}, {0, 2}, {2, 10}, {2, 13}, {10, 11}, {11, 12}, {12, 13}, {11, 13}});
	auto minimizer = Transitivity_constraint_minimizer<dtime_t>(problem, 3);

	// There is a redundant constraint from job 0: {0 -> 2}, but it is an original constraint
	CHECK(minimizer.find_redundant_constraint_indices(0).size() == 0);

	// There simply aren't any redundant constraints from job 10
	CHECK(minimizer.find_redundant_constraint_indices(10).size() == 0);

	// The constraint from job 2 -> 13 is redundant
	REQUIRE(minimizer.find_redundant_constraint_indices(2).size() == 1);
	CHECK(minimizer.find_redundant_constraint_indices(2)[0] == 4);

	// The constraint from job 11 -> 13 is redundant
	REQUIRE(minimizer.find_redundant_constraint_indices(11).size() == 1);
	CHECK(minimizer.find_redundant_constraint_indices(11)[0] == 8);

	REQUIRE(problem.prec.size() == 9);
	minimizer.remove_redundant_constraints();
	REQUIRE(problem.prec.size() == 7);

	const auto expected = dummy_problem({{0, 1}, {1, 2}, {0, 2}, {2, 10}, {12, 13}, {10, 11}, {11, 12}});
	for (size_t index = 0; index < 7; index++) {
		CHECK(problem.prec[index].get_fromIndex() == expected.prec[index].get_fromIndex());
		CHECK(problem.prec[index].get_toIndex() == expected.prec[index].get_toIndex());
	}
}

#endif
