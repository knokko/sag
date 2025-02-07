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
	// TODO Fix later
	REQUIRE(false);
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

TEST_CASE("Cut enforcer single cut failure regression test") {
	auto jobs_file_input = std::ifstream("../src/tests/failed-cut-loop.csv", std::ios::in);
	auto prec_file_input = std::ifstream("../src/tests/failed-cut-loop.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input), 3);

	Rating_graph old_rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, old_rating_graph);
	REQUIRE(old_rating_graph.nodes[0].get_rating() > 0.0);
	REQUIRE(old_rating_graph.nodes[0].get_rating() < 1.0);
	const auto bounds = compute_simple_bounds(problem);

	Feasibility_graph<dtime_t> feasibility_graph(old_rating_graph);
	feasibility_graph.explore_forward(problem, bounds, create_predecessor_mapping(problem));
	feasibility_graph.explore_backward();

	auto cuts = cut_rating_graph(old_rating_graph, feasibility_graph);
	REQUIRE(!cuts.empty());
	cuts.resize(1);

	enforce_cuts(problem, old_rating_graph, cuts, bounds);

	Rating_graph new_rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, new_rating_graph);
	CHECK(new_rating_graph.nodes[0].get_rating() > 0.0);
}

TEST_CASE("Cut enforcer single cut failure regression test (2)") {
	auto jobs_file_input = std::ifstream("../src/tests/failed-cut-loop2.csv", std::ios::in);
	auto prec_file_input = std::ifstream("../src/tests/failed-cut-loop2.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input), 3);

	Rating_graph old_rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, old_rating_graph);
	REQUIRE(old_rating_graph.nodes[0].get_rating() > 0.0);
	REQUIRE(old_rating_graph.nodes[0].get_rating() < 1.0);
	const auto bounds = compute_simple_bounds(problem);

	Feasibility_graph<dtime_t> feasibility_graph(old_rating_graph);
	feasibility_graph.explore_forward(problem, bounds, create_predecessor_mapping(problem));
	feasibility_graph.explore_backward();

	auto cuts = cut_rating_graph(old_rating_graph, feasibility_graph);
	REQUIRE(!cuts.empty());
	cuts.resize(1);
	std::cout << std::endl;
	old_rating_graph.generate_focused_dot_file("cut-enforce-failure-before.dot", problem, 8, 8);

	enforce_cuts(problem, old_rating_graph, cuts, bounds);

	Rating_graph new_rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, new_rating_graph);
	new_rating_graph.generate_focused_dot_file("cut-enforce-failure-after.dot", problem, 8, 8);
	CHECK(new_rating_graph.nodes[0].get_rating() > 0.0);
}

TEST_CASE("Cut enforcer single cut failure regression test (3)") {
	auto jobs_file_input = std::ifstream("../src/tests/failed-cut-loop3.csv", std::ios::in);
	auto prec_file_input = std::ifstream("../src/tests/failed-cut-loop3.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input), 3);

	Rating_graph old_rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, old_rating_graph);
	REQUIRE(old_rating_graph.nodes[0].get_rating() > 0.0);
	REQUIRE(old_rating_graph.nodes[0].get_rating() < 1.0);

	{
		const auto bounds = compute_simple_bounds(problem);

		Feasibility_graph<dtime_t> feasibility_graph(old_rating_graph);
		feasibility_graph.explore_forward(problem, bounds, create_predecessor_mapping(problem));
		feasibility_graph.explore_backward();

		auto cuts = cut_rating_graph(old_rating_graph, feasibility_graph);
		for (const auto &cut : cuts) REQUIRE(!cut.safe_jobs.empty());
		REQUIRE(!cuts.empty());
		cuts.resize(1);
		std::cout << std::endl;
		old_rating_graph.generate_focused_dot_file("cut-enforce-failure-before.dot", problem, 8, 8);

		enforce_cuts(problem, old_rating_graph, cuts, bounds);
	}

	Rating_graph new_rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, new_rating_graph);
	new_rating_graph.generate_focused_dot_file("cut-enforce-failure-after.dot", problem, 8, 8);
	REQUIRE(new_rating_graph.nodes[0].get_rating() > 0.0);
	const auto bounds = compute_simple_bounds(problem);

	Feasibility_graph<dtime_t> feasibility_graph(new_rating_graph);
	feasibility_graph.explore_forward(problem, bounds, create_predecessor_mapping(problem));
	feasibility_graph.explore_backward();

	const auto cuts = cut_rating_graph(new_rating_graph, feasibility_graph);
	for (const auto &cut : cuts) REQUIRE(!cut.safe_jobs.empty());
}

#endif
