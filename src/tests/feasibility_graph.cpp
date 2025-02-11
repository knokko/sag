#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "feasibility/simple_bounds.hpp"
#include "feasibility/graph.hpp"
#include "global/space.hpp"
#include "reconfiguration/graph_cutter.hpp"
#include "reconfiguration/cut_enforcer.hpp"

using namespace NP;
using namespace NP::Feasibility;
using namespace NP::Reconfiguration;

TEST_CASE("Feasibility graph on small very simple problem with 1 core") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 20), 60, 2, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 10), Interval<dtime_t>(1, 30), 50, 1, 1, 1},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Rating_graph rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	REQUIRE(rating_graph.nodes[0].get_rating() == 1.0);
	REQUIRE(rating_graph.edges.size() == 4);

	// Any job ordering is feasible

	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);

	for (size_t index = 0; index < 4; index++) {
		CHECK(feasibility_graph.is_edge_feasible(index));
		CHECK(feasibility_graph.is_node_feasible(index));
	}

	// Check that nothing changes after backward exploration
	feasibility_graph.explore_backward();
	for (size_t index = 0; index < 4; index++) {
		CHECK(feasibility_graph.is_edge_feasible(index));
		CHECK(feasibility_graph.is_node_feasible(index));
	}
}

TEST_CASE("Feasibility graph on very small infeasible problem with 1 core") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(1, 20), 50, 2, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 1), Interval<dtime_t>(1, 30), 50, 1, 1, 1},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	// From the perspective of the rating graph, 2 paths are possible:
	// - Start with job 0, which is only possible when job 0 arrives at time 0 and job 1 arrives at time 1.
	//   This case will lead to a schedulable result.
	// - Start with job 1, which is unschedulable because it could arrive at time 1

	Rating_graph rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	rating_graph.generate_full_dot_file("test.dot", problem, {}, false);
	REQUIRE(rating_graph.nodes.size() == 5);
	REQUIRE(rating_graph.nodes[0].get_rating() == 0.5);
	REQUIRE((rating_graph.nodes[1].get_rating() == 1.0) != (rating_graph.nodes[2].get_rating() == 1.0));
	REQUIRE((rating_graph.nodes[3].get_rating() == 1.0) != (rating_graph.nodes[4].get_rating() == 1.0));
	REQUIRE(rating_graph.edges.size() == 4);

	// Neither job ordering is feasible

	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);

	CHECK(feasibility_graph.is_node_feasible(0));
	for (size_t index = 1; index < 4; index++) CHECK(!feasibility_graph.is_node_feasible(index));
	CHECK(feasibility_graph.is_edge_feasible(0) != feasibility_graph.is_edge_feasible(1));
	CHECK(!feasibility_graph.is_edge_feasible(2));
	CHECK(!feasibility_graph.is_edge_feasible(3));

	// Since neither ordering is feasible, all nodes and edges should be marked as infeasible after backward exploration
	feasibility_graph.explore_backward();
	for (size_t node_index = 0; node_index < 5; node_index++) CHECK(!feasibility_graph.is_node_feasible(node_index));
	for (size_t edge_index = 0; edge_index < 4; edge_index++) CHECK(!feasibility_graph.is_edge_feasible(edge_index));
}

TEST_CASE("Feasibility graph on very small problem with 1 core") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 20), 50, 2, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 10), Interval<dtime_t>(1, 30), 50, 1, 1, 1},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Rating_graph rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	REQUIRE(rating_graph.nodes[0].get_rating() == 1.0);
	REQUIRE(rating_graph.edges.size() == 4);

	// Taking job 0 first and job 1 second is feasible
	// Taking job 1 first and job 0 second is infeasible because the deadline of job 0 is missed when job 1 arrives after time 0

	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);

	CHECK(feasibility_graph.is_node_feasible(0));
	CHECK(feasibility_graph.is_node_feasible(1) != feasibility_graph.is_node_feasible(2));
	CHECK(feasibility_graph.is_node_feasible(3));
	CHECK(feasibility_graph.is_edge_feasible(0));
	CHECK(feasibility_graph.is_edge_feasible(1));
	CHECK(feasibility_graph.is_edge_feasible(2) != feasibility_graph.is_edge_feasible(3));

	// After backward exploration, it should be observed that either edge 0 or edge 1 is infeasible because either node 1 or node 2 is infeasible
	feasibility_graph.explore_backward();
	CHECK(feasibility_graph.is_node_feasible(0));
	CHECK(feasibility_graph.is_node_feasible(1) != feasibility_graph.is_node_feasible(2));
	CHECK(feasibility_graph.is_node_feasible(3));
	CHECK(feasibility_graph.is_edge_feasible(0) != feasibility_graph.is_edge_feasible(1));
	CHECK(feasibility_graph.is_edge_feasible(2) != feasibility_graph.is_edge_feasible(3));

	// Since the problem is schedulable, no cuts are needed
	const auto safe_path = feasibility_graph.create_safe_path(problem);
	const auto cuts = cut_rating_graph(rating_graph, safe_path);
	CHECK(cuts.size() == 0);
}

TEST_CASE("Feasibility graph on rating_graph problem") {
	Global::State_space<dtime_t>::Workload jobs{
			// high-frequency task
			Job<dtime_t>{0, Interval<dtime_t>(0,  0), Interval<dtime_t>(1, 2), 10, 10, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(10, 10), Interval<dtime_t>(1, 2), 20, 20, 1, 1},
			Job<dtime_t>{2, Interval<dtime_t>(20, 20), Interval<dtime_t>(1, 2), 30, 30, 2, 2},
			Job<dtime_t>{3, Interval<dtime_t>(30, 30), Interval<dtime_t>(1, 2), 40, 40, 3, 3},
			Job<dtime_t>{4, Interval<dtime_t>(40, 40), Interval<dtime_t>(1, 2), 50, 50, 4, 4},
			Job<dtime_t>{5, Interval<dtime_t>(50, 50), Interval<dtime_t>(1, 2), 60, 60, 5, 5},

			// middle task
			Job<dtime_t>{6, Interval<dtime_t>(0,  0), Interval<dtime_t>(7, 8), 30, 30, 6, 6},
			Job<dtime_t>{7, Interval<dtime_t>(30, 30), Interval<dtime_t>(7, 8), 60, 60, 7, 7},

			// the long task
			Job<dtime_t>{8, Interval<dtime_t>(0,  0), Interval<dtime_t>(3, 13), 60, 60, 8, 8}
	};

	auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Rating_graph rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	rating_graph.generate_full_dot_file("rating_graph_without_cuts.dot", problem, {}, false);
	REQUIRE(rating_graph.nodes[0].get_rating() == 0.5);
	REQUIRE(rating_graph.nodes.size() == 12);
	REQUIRE(rating_graph.edges.size() == 11);

	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);

	// The feasible path is 0 -> 6 -> 1 -> 8 -> 2 -> 3 -> 7 -> 4 -> 5
	// The infeasible and unschedulable path is 0 -> 6 -> 8 -> job 1 is doomed to miss its deadline

	CHECK(feasibility_graph.is_node_feasible(0));
	CHECK(feasibility_graph.is_node_feasible(1));
	for (size_t node_index = 0; node_index < 3; node_index++) CHECK(feasibility_graph.is_node_feasible(node_index));
	CHECK(feasibility_graph.is_node_feasible(3) != feasibility_graph.is_node_feasible(4));
	for (size_t node_index = 7; node_index < rating_graph.nodes.size(); node_index++) CHECK(feasibility_graph.is_node_feasible(node_index));

	CHECK(feasibility_graph.is_edge_feasible(0));
	CHECK(feasibility_graph.is_edge_feasible(1));
	CHECK(feasibility_graph.is_edge_feasible(2) != feasibility_graph.is_edge_feasible(3));
	CHECK(feasibility_graph.is_edge_feasible(4) != feasibility_graph.is_edge_feasible(5));
	for (size_t edge_index = 6; edge_index < rating_graph.edges.size(); edge_index++) CHECK(feasibility_graph.is_edge_feasible(edge_index));

	// Check that nothing changes after backward exploration
	feasibility_graph.explore_backward();
	for (size_t node_index = 0; node_index < 3; node_index++) CHECK(feasibility_graph.is_node_feasible(node_index));
	CHECK(feasibility_graph.is_node_feasible(3) != feasibility_graph.is_node_feasible(4));
	CHECK(feasibility_graph.is_node_feasible(5) != feasibility_graph.is_node_feasible(6));
	for (size_t node_index = 7; node_index < rating_graph.nodes.size(); node_index++) CHECK(feasibility_graph.is_node_feasible(node_index));

	CHECK(feasibility_graph.is_edge_feasible(0));
	CHECK(feasibility_graph.is_edge_feasible(1));
	CHECK(feasibility_graph.is_edge_feasible(2) != feasibility_graph.is_edge_feasible(3));
	CHECK(feasibility_graph.is_edge_feasible(4) != feasibility_graph.is_edge_feasible(5));
	for (size_t edge_index = 6; edge_index < rating_graph.edges.size(); edge_index++) CHECK(feasibility_graph.is_edge_feasible(edge_index));

	const auto safe_path = feasibility_graph.create_safe_path(problem);
	auto cuts = cut_rating_graph(rating_graph, safe_path);
	rating_graph.generate_full_dot_file("rating_graph_with_cuts.dot", problem, cuts);
	REQUIRE(cuts.size() == 1);
	const auto &cut = cuts[0];

	CHECK(cut.node_index == 2);
	REQUIRE(cut.forbidden_jobs.size() == 1);
	CHECK(cut.forbidden_jobs[0] == 8);
	CHECK(cut.safe_job == 1);
	CHECK(cut.allowed_jobs.size() == 0);

	enforce_cuts_with_path(problem, cuts, safe_path);
	REQUIRE(problem.prec.size() == 1);
	CHECK(problem.prec[0].get_fromIndex() == 1);
	CHECK(problem.prec[0].get_toIndex() == 8);
	CHECK(!problem.prec[0].should_signal_at_completion());

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

static size_t get_edge_destination(Rating_graph &rating_graph, int node_index, Job_index taken_job) {
	for (const auto &edge : rating_graph.edges) {
		if (edge.get_parent_node_index() == node_index && edge.get_taken_job_index() == taken_job) return edge.get_child_node_index();
	}
	REQUIRE(false);
	return 0;
}

static size_t get_edge_index(Rating_graph &rating_graph, size_t parent_node_index, size_t child_node_index) {
	for (size_t edge_index = 0; edge_index < rating_graph.edges.size(); edge_index++) {
		const auto &edge = rating_graph.edges[edge_index];
		if (edge.get_parent_node_index() == parent_node_index && edge.get_child_node_index() == child_node_index) return edge_index;
	}
	REQUIRE(false);
	return 0;
}

TEST_CASE("Feasibility graph test with precedence constraints") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(1, 20), 100, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 30), 55, 2, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 50), Interval<dtime_t>(1, 30), 100, 1, 2, 2},
	};

	// If job 0 arrives at time 1 and job 2 arrives at time 0, job 2 will go first -> job 1 will miss its deadline
	// If job 0 goes first and job 2 is ready when job 0 is finished, job 2 will go after job 0 -> job 1 will miss its deadline
	// If job 0 goes first and job 2 is not ready when job 0 is finished, job 1 will go after job 0 -> all jobs meet their deadline

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints {
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>(0, 4))
	};

	auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Rating_graph rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	rating_graph.generate_full_dot_file("feasibility_graph_precedence_witout_cuts.dot", problem, {});

	REQUIRE(rating_graph.nodes[0].get_rating() > 0.2);
	REQUIRE(rating_graph.nodes[0].get_rating() < 0.3);

	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);

	size_t node_after0 = get_edge_destination(rating_graph, 0, 0);
	size_t node_after2 = get_edge_destination(rating_graph, 0, 2);
	CHECK(feasibility_graph.is_node_feasible(node_after0));
	CHECK(!feasibility_graph.is_node_feasible(node_after2));
	CHECK(feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, 0, node_after0)));
	CHECK(!feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, 0, node_after2)));

	size_t node_after01 = get_edge_destination(rating_graph, node_after0, 1);
	size_t node_after02 = get_edge_destination(rating_graph, node_after0, 2);
	size_t node_after20 = get_edge_destination(rating_graph, node_after2, 0);
	CHECK(feasibility_graph.is_node_feasible(node_after01));
	CHECK(!feasibility_graph.is_node_feasible(node_after02));
	CHECK(!feasibility_graph.is_node_feasible(node_after20));
	CHECK(feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, node_after0, node_after01)));
	CHECK(!feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, node_after0, node_after02)));
	CHECK(!feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, node_after2, node_after20)));

	size_t leaf = get_edge_destination(rating_graph, node_after01, 2);
	CHECK(feasibility_graph.is_node_feasible(leaf));
	CHECK(feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, node_after01, leaf)));

	// Check that nothing changes after backward exploration
	feasibility_graph.explore_backward();
	CHECK(feasibility_graph.is_node_feasible(node_after0));
	CHECK(!feasibility_graph.is_node_feasible(node_after2));
	CHECK(feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, 0, node_after0)));
	CHECK(!feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, 0, node_after2)));

	CHECK(feasibility_graph.is_node_feasible(node_after01));
	CHECK(!feasibility_graph.is_node_feasible(node_after02));
	CHECK(!feasibility_graph.is_node_feasible(node_after20));
	CHECK(feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, node_after0, node_after01)));
	CHECK(!feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, node_after0, node_after02)));
	CHECK(!feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, node_after2, node_after20)));

	const auto safe_path = feasibility_graph.create_safe_path(problem);
	auto cuts = cut_rating_graph(rating_graph, safe_path);
	rating_graph.generate_full_dot_file("feasibility_graph_precedence_with_cuts.dot", problem, cuts);
	REQUIRE(cuts.size() == 2);
	const auto &cut0 = cuts[0];
	const auto &cut1 = cuts[1];

	CHECK(cut0.node_index == 0);
	REQUIRE(cut0.forbidden_jobs.size() == 1);
	CHECK(cut0.forbidden_jobs[0] == 2);
	CHECK(cut0.safe_job == 0);
	CHECK(cut0.allowed_jobs.size() == 0);

	CHECK(cut1.node_index == node_after0);
	REQUIRE(cut1.forbidden_jobs.size() == 1);
	CHECK(cut1.forbidden_jobs[0] == 2);
	CHECK(cut1.safe_job == 1);
	CHECK(cut1.allowed_jobs.size() == 0);

	enforce_cuts_with_path(problem, cuts, safe_path);
	REQUIRE(problem.prec.size() == 2);
	CHECK(problem.prec[1].get_fromIndex() == 1);
	CHECK(problem.prec[1].get_toIndex() == 2);
	CHECK(!problem.prec[1].should_signal_at_completion());

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

TEST_CASE("Feasibility graph merge: schedulable problem that looks infeasible after merging 2 nodes") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 1), Interval<dtime_t>(1, 50), 101, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 101, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 201, 2, 2, 2},
			Job<dtime_t>{3, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 201, 3, 3, 3},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints {
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(0, 10)),
			Precedence_constraint<dtime_t>(jobs[1].get_id(), jobs[3].get_id(), Interval<dtime_t>(0, 10)),
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Rating_graph rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, rating_graph);

	REQUIRE(rating_graph.nodes.size() == 7);
	REQUIRE(rating_graph.edges.size() == 8);
	for (size_t index = 0; index < 7; index++) REQUIRE(rating_graph.nodes[index].get_rating() == 1.0);

	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);

	for (size_t node_index = 0; node_index < 4; node_index++) CHECK(feasibility_graph.is_node_feasible(node_index));
	for (size_t edge_index = 0; edge_index < 6; edge_index++) CHECK(feasibility_graph.is_edge_feasible(edge_index));

	for (size_t node_index = 4; node_index < 7; node_index++) CHECK(!feasibility_graph.is_node_feasible(node_index));
	CHECK(!feasibility_graph.is_edge_feasible(6));
	CHECK(!feasibility_graph.is_edge_feasible(7));

	// Trace when job 0 goes first:
	// - schedule job 0, which starts at time 0 and ends at time 50
	// - schedule job 1, which starts at time 50 and ends at time 100
	// - schedule job 2, which starts at time 100 and ends at time 150
	// - schedule job 3, which starts at time 150 and ends at time 200

	// Trace when job 1 goes first:
	// - schedule job 1, which starts at time 0 and ends at time 50
	// - schedule job 0, which starts at time 50 and ends at time 100
	// - schedule job 3, which starts at time 100 and ends at time 150
	// - schedule job 2, which starts at time 150 and ends at time 100

	// Merged trace:
	// - schedule job 0, which starts at time 0 and ends at time 50
	// - schedule job 1, which starts at time 50 and ends at time 100
	// MERGE: the latest end time of job 0 becomes 100 because job0 ends at time 100 in parent2
	// - schedule job 2, which starts at time 110 (SUSPENSION) and ends at time 160
	// - schedule job 3, which starts at time 160 and ends at time 210 -> DEADLINE MISS

	feasibility_graph.explore_backward();
	for (size_t node_index = 0; node_index < 7; node_index++) CHECK(!feasibility_graph.is_node_feasible(node_index));
	for (size_t edge_index = 0; edge_index < 8; edge_index++) CHECK(!feasibility_graph.is_edge_feasible(edge_index));

	const auto safe_path = feasibility_graph.try_to_find_random_safe_path(problem, 1000, false);
	REQUIRE(safe_path.size() == 4);
	if (safe_path[0] == 0) CHECK(safe_path[1] == 1);
	else {
		CHECK(safe_path[0] == 1);
		CHECK(safe_path[1] == 0);
	}
	if (safe_path[2] == 2) CHECK(safe_path[3] == 3);
	else {
		CHECK(safe_path[2] == 3);
		CHECK(safe_path[3] == 2);
	}
}

TEST_CASE("Feasibility graph complex cuts") {
	Global::State_space<dtime_t>::Workload jobs{
			// It would be very convenient if J0 arrives early, but we can't rely on this
			Job<dtime_t>{0, Interval<dtime_t>(0, 70), Interval<dtime_t>(20, 20), 100, 0, 0, 0},

			// J1 is a very natural node to schedule first, but it does not always happen
			Job<dtime_t>{1, Interval<dtime_t>(0, 5), Interval<dtime_t>(5, 10), 75, 1, 1, 1},

			// J2 could be scheduled first, but that would be a big mistake
			Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(5, 100), 200, 4, 2, 2},

			// J3 should be scheduled after J1, but it does not always happen
			Job<dtime_t>{3, Interval<dtime_t>(0, 15), Interval<dtime_t>(5, 20), 70, 3, 3, 3},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints {
			Precedence_constraint<dtime_t>(jobs[1].get_id(), jobs[3].get_id(), Interval<dtime_t>(0, 0))
	};

	auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Rating_graph rating_graph;
	Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	rating_graph.generate_full_dot_file("feasibility_graph_complex_without_cuts.dot", problem, {});

	CHECK(rating_graph.nodes[0].get_rating() == 0.5);

	const size_t node_after0 = get_edge_destination(rating_graph, 0, 0);
	const size_t node_after1 = get_edge_destination(rating_graph, 0, 1);
	const size_t node_after2 = get_edge_destination(rating_graph, 0, 2);
	CHECK(rating_graph.nodes[node_after0].get_rating() == 1.0);
	CHECK(rating_graph.nodes[node_after1].get_rating() == 0.5);
	CHECK(rating_graph.nodes[node_after2].get_rating() == 0.0);

	const size_t node_after10 = get_edge_destination(rating_graph, node_after1, 0);
	const size_t node_after12 = get_edge_destination(rating_graph, node_after1, 2);
	const size_t node_after13 = get_edge_destination(rating_graph, node_after1, 3);
	CHECK(rating_graph.nodes[node_after10].get_rating() == 1.0);
	CHECK(rating_graph.nodes[node_after12].get_rating() == 0.0);
	CHECK(rating_graph.nodes[node_after13].get_rating() == 0.5);

	CHECK(rating_graph.nodes[get_edge_destination(rating_graph, node_after13, 0)].get_rating() == 1.0);
	CHECK(rating_graph.nodes[get_edge_destination(rating_graph, node_after13, 2)].get_rating() == 0.0);

	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);

	feasibility_graph.explore_forward(problem, bounds, predecessor_mapping);
	feasibility_graph.explore_backward();

	CHECK(feasibility_graph.is_node_feasible(0));
	CHECK(!feasibility_graph.is_node_feasible(node_after0));
	CHECK(feasibility_graph.is_node_feasible(node_after1));
	CHECK(!feasibility_graph.is_node_feasible(node_after2));
	CHECK(!feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, 0, node_after0)));
	CHECK(feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, 0, node_after1)));
	CHECK(!feasibility_graph.is_edge_feasible(get_edge_index(rating_graph, 0, node_after2)));

	CHECK(!feasibility_graph.is_node_feasible(node_after10));
	CHECK(!feasibility_graph.is_node_feasible(node_after12));
	CHECK(feasibility_graph.is_node_feasible(node_after13));

	const auto safe_path = feasibility_graph.create_safe_path(problem);
	auto cuts = cut_rating_graph(rating_graph, safe_path);
	rating_graph.generate_full_dot_file("feasibility_graph_complex_with_cuts.dot", problem, cuts);
	REQUIRE(cuts.size() == 3);

	CHECK(cuts[0].node_index == 0);
	REQUIRE(cuts[0].allowed_jobs.size() == 1);
	REQUIRE(cuts[0].forbidden_jobs.size() == 1);
	CHECK(cuts[0].allowed_jobs[0] == 0);
	CHECK(cuts[0].safe_job == 1);
	CHECK(cuts[0].forbidden_jobs[0] == 2);

	CHECK(cuts[1].node_index == node_after1);
	REQUIRE(cuts[1].allowed_jobs.size() == 1);
	REQUIRE(cuts[1].forbidden_jobs.size() == 1);
	CHECK(cuts[1].allowed_jobs[0] == 0);
	CHECK(cuts[1].safe_job == 3);
	CHECK(cuts[1].forbidden_jobs[0] == 2);

	CHECK(cuts[2].node_index == node_after13);
	CHECK(cuts[2].allowed_jobs.size() == 0);
	REQUIRE(cuts[2].forbidden_jobs.size() == 1);
	CHECK(cuts[2].safe_job == 0);
	CHECK(cuts[2].forbidden_jobs[0] == 2);

	enforce_cuts_with_path(problem, cuts, safe_path);
	REQUIRE(problem.prec.size() == 2);
	CHECK(problem.prec[1].get_fromIndex() == 0);
	CHECK(problem.prec[1].get_toIndex() == 2);
	CHECK(!problem.prec[1].should_signal_at_completion());

	const auto space = Global::State_space<dtime_t>::explore(problem, {}, nullptr);
	CHECK(space->is_schedulable());
	delete space;
}

#endif
