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

using namespace NP;
using namespace NP::Feasibility;
using namespace NP::Reconfiguration;

TEST_CASE("Reconfigure annoying 30-jobs case") {
	auto jobs_file_input = std::ifstream("../examples/30-jobs-unschedulable.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input));
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Rating_graph rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, rating_graph, true);

	REQUIRE(rating_graph.nodes[0].get_rating() > 0.0);
	REQUIRE(rating_graph.nodes[0].get_rating() < 1.0);

	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);
	feasibility_graph.explore_backward();

	REQUIRE(feasibility_graph.is_node_feasible(0));
	const auto safe_path = feasibility_graph.create_safe_path(problem);

	auto cuts = cut_rating_graph(rating_graph, safe_path);
	REQUIRE(!cuts.empty());

	enforce_cuts_with_path(problem, cuts, safe_path);
	CHECK(problem.prec.size() == 14);

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

#endif
