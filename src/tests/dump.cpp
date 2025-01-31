#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include <iostream>
#include <sstream>
#include <fstream>

#include "io.hpp"
#include "dump.hpp"
#include "problem.hpp"

using namespace NP;

TEST_CASE("Test dump problem") {
	std::vector<Job<dtime_t>> jobs{
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(1, 20), 100, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 30), 55, 2, 1, 1},
	};
	std::vector<Precedence_constraint<dtime_t>> precedence_constraints {
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>(0, 4), false)
	};
	dump_problem("test_dump_jobs.csv", "test_dump_jobs.prec.csv", Scheduling_problem<dtime_t>(jobs, precedence_constraints));

	auto jobs_file_input = std::ifstream("test_dump_jobs.csv", std::ios::in);
	auto prec_file_input = std::ifstream("test_dump_jobs.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input));

	REQUIRE(problem.jobs.size() == 2);
	REQUIRE(problem.prec.size() == 1);
	CHECK(problem.prec[0].get_fromID() == jobs[0].get_id());
	CHECK(problem.prec[0].get_toID() == jobs[1].get_id());
	CHECK(problem.prec[0].get_suspension() == precedence_constraints[0].get_suspension());
	CHECK(!problem.prec[0].should_signal_at_completion());

	CHECK(problem.jobs[0].get_deadline() == 100);
	CHECK(problem.jobs[1].get_priority() == 2);
}

#endif
