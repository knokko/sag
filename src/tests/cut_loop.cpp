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

template<class Time> static Cut_loop<Time> create_cut_loop(Scheduling_problem<Time> &problem) {
	const auto bounds = compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	Rating_graph rating_graph;
	Agent_rating_graph<Time>::generate(problem, rating_graph);
	Feasibility_graph<Time> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);
	feasibility_graph.explore_backward();
	REQUIRE(feasibility_graph.is_node_feasible(0));
	const auto safe_path = feasibility_graph.create_safe_path(problem);
	return Cut_loop(problem, safe_path);
}

TEST_CASE("Cut loop on annoying 30-jobs case") {
	auto jobs_file_input = std::ifstream("../examples/30-jobs-unschedulable.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input));
	auto cut_loop = create_cut_loop(problem);
	cut_loop.cut_until_finished(false, 1);

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

TEST_CASE("Cut loop on easiest almost-unschedulable problem") {
	auto jobs_file_input = std::ifstream("../examples/almost-unschedulable-job-sets/jitter15.csv", std::ios::in);
	auto prec_file_input = std::ifstream("../examples/almost-unschedulable-job-sets/jitter15.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input), 3);
	auto cut_loop = create_cut_loop(problem);
	cut_loop.cut_until_finished(false, 0);

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

TEST_CASE("Cut enforcer single cut failure regression test (1)") {
	auto jobs_file_input = std::ifstream("../src/tests/failed-cut-loop.csv", std::ios::in);
	auto prec_file_input = std::ifstream("../src/tests/failed-cut-loop.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input), 3);

	auto cut_loop = create_cut_loop(problem);
	cut_loop.cut_until_finished(false, 0);

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

TEST_CASE("Cut enforcer single cut failure regression test (2)") {
	auto jobs_file_input = std::ifstream("../src/tests/failed-cut-loop2.csv", std::ios::in);
	auto prec_file_input = std::ifstream("../src/tests/failed-cut-loop2.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input), 3);

	auto cut_loop = create_cut_loop(problem);
	cut_loop.cut_until_finished(false, 1);

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

TEST_CASE("Cut enforcer single cut failure regression test (3)") {
	auto jobs_file_input = std::ifstream("../src/tests/failed-cut-loop3.csv", std::ios::in);
	auto prec_file_input = std::ifstream("../src/tests/failed-cut-loop3.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input), 3);

	auto cut_loop = create_cut_loop(problem);
	cut_loop.cut_until_finished(false, 0);

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

#endif
