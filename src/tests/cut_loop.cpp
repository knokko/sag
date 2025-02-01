#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include <iostream>
#include <sstream>
#include <fstream>

#include "io.hpp"
#include "global/space.hpp"
#include "feasibility/simple_bounds.hpp"
#include "feasibility/graph.hpp"
#include "reconfiguration/graph_cutter.hpp"
#include "reconfiguration/cut_enforcer.hpp"
#include "reconfiguration/cut_loop.hpp"
#include "reconfiguration/result_printer.hpp"

using namespace NP;
using namespace NP::Feasibility;
using namespace NP::Reconfiguration;

TEST_CASE("Cut loop on annoying 30-jobs case") {
	auto jobs_file_input = std::ifstream("../examples/30-jobs-unschedulable.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input));
	const auto bounds = compute_simple_bounds(problem);

	Cut_loop<dtime_t> cut_loop(problem, 0, {});
	cut_loop.cut_until_finished(false, 5);
	CHECK(cut_loop.has_finished());
	CHECK(!cut_loop.has_failed());

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

TEST_CASE("Cut loop on easiest almost-unschedulable problem") {
	auto jobs_file_input = std::ifstream("../examples/almost-unschedulable-job-sets/jitter15.csv", std::ios::in);
	auto prec_file_input = std::ifstream("../examples/almost-unschedulable-job-sets/jitter15.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input), 3);

	Cut_loop<dtime_t> cut_loop(problem, 0, {});
	cut_loop.cut_until_finished(true, 5);
	CHECK(cut_loop.has_finished());
	CHECK(!cut_loop.has_failed());

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}


#endif
