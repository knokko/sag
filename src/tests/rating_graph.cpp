#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include <iostream>
#include <sstream>

#include "io.hpp"
#include "global/space.hpp"
#include "reconfiguration/graph_cutter.hpp"
#include "reconfiguration/rating_graph.hpp"

using namespace NP;

static size_t get_edge_destination(Reconfiguration::Rating_graph &rating_graph, size_t node_index, Job_index taken_job) {
	for (const auto &edge : rating_graph.edges) {
		if (edge.get_parent_node_index() == node_index && edge.get_taken_job_index() == taken_job) return edge.get_child_node_index();
	}
	REQUIRE(false);
	return 0;
}

static size_t get_edge_destination(Reconfiguration::Rating_graph &rating_graph, size_t node_index) {
	for (const auto &edge : rating_graph.edges) {
		if (edge.get_parent_node_index() == node_index) return edge.get_child_node_index();
	}
	REQUIRE(false);
	return 0;
}

static size_t get_number_of_edges(Reconfiguration::Rating_graph &rating_graph, size_t node_index) {
	size_t result = 0;
	for (const auto &edge : rating_graph.edges) {
		if (edge.get_parent_node_index() == node_index) result += 1;
	}
	return result;
}

TEST_CASE("Rating graph size") {
	CHECK(sizeof(Reconfiguration::Rating_edge) == 12);
	CHECK(sizeof(Reconfiguration::Rating_node) == 1);
	REQUIRE(sizeof(size_t) == 8);

	Reconfiguration::Rating_edge small(1, 22, 33);
	CHECK(small.get_parent_node_index() == 1);
	CHECK(small.get_child_node_index() == 22);
	CHECK(small.get_taken_job_index() == 33);

	Reconfiguration::Rating_edge test(123456789012, 123456789012 + 123456789, 12345678);
	CHECK(test.get_parent_node_index() == 123456789012);
	CHECK(test.get_child_node_index() == 123456789012 + 123456789);
	CHECK(test.get_taken_job_index() == 12345678);
}

TEST_CASE("Rating_node.get_rating() and set_rating()") {
	Reconfiguration::Rating_node node;
	CHECK(node.get_rating() == 0.0);
	node.set_rating(-1.0);
	CHECK(node.get_rating() == -1.0);
	node.set_rating(1.0);
	CHECK(node.get_rating() == 1.0);
	node.set_rating(1.001);
	CHECK(node.get_rating() == 1.0);
	node.set_rating(0.00001);
	CHECK(node.get_rating() > 0.0);
	CHECK(node.get_rating() < 0.01);
	node.set_rating(0.9999);
	CHECK(node.get_rating() < 1.0);
	CHECK(node.get_rating() > 0.99);
}

TEST_CASE("Rating graph basic test") {
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

	auto problem = Scheduling_problem<dtime_t>(jobs, std::vector<Precedence_constraint<dtime_t>>());

	Reconfiguration::Rating_graph rating_graph;
	Reconfiguration::Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	rating_graph.generate_dot_file("rating_graph_basic.dot", problem, std::vector<Reconfiguration::Rating_graph_cut>(), false);

	REQUIRE(rating_graph.nodes.size() == 12);

	// Node 0 is the root, and can only take job 0
	CHECK(rating_graph.nodes[0].get_rating() == 0.5);
	REQUIRE(get_number_of_edges(rating_graph, 0) == 1);
	REQUIRE(get_edge_destination(rating_graph, 0, 0) == 1); // Takes job 0 to node 1

	// Node 1 can only take job 6
	CHECK(rating_graph.nodes[1].get_rating() == 0.5);
	REQUIRE(get_number_of_edges(rating_graph, 1) == 1);
	REQUIRE(get_edge_destination(rating_graph, 1, 6) == 2); // Takes job 6 to node 2

	// Node 2 can take either job 1 or job 8, where job 8 is a poor choice
	CHECK(rating_graph.nodes[2].get_rating() == 0.5);
	REQUIRE(get_number_of_edges(rating_graph, 2) == 2);

	int failed_node_index1 = get_edge_destination(rating_graph, 2, 8);
	CHECK(rating_graph.nodes[failed_node_index1].get_rating() == 0.0);
	REQUIRE(get_number_of_edges(rating_graph, failed_node_index1) == 1);
	int failed_node_index2 = get_edge_destination(rating_graph, failed_node_index1, 1);
	CHECK(rating_graph.nodes[failed_node_index2].get_rating() == 0.0);
	REQUIRE(get_number_of_edges(rating_graph, failed_node_index2) == 0);

	int right_node_index1 = get_edge_destination(rating_graph, 2, 1);
	CHECK(rating_graph.nodes[right_node_index1].get_rating() == 1.0);
	REQUIRE(get_number_of_edges(rating_graph, right_node_index1) == 1);
	int right_node_index2 = get_edge_destination(rating_graph, right_node_index1, 8);
	CHECK(rating_graph.nodes[right_node_index2].get_rating() == 1.0);
	CHECK(get_edge_destination(rating_graph, right_node_index2, 2) == 7);

	for (int index = 7; index < rating_graph.nodes.size(); index++) {
		CHECK(rating_graph.nodes[index].get_rating() == 1.0);

		if (index != rating_graph.nodes.size() - 1) {
			REQUIRE(get_number_of_edges(rating_graph, index) == 1);
			CHECK(get_edge_destination(rating_graph, index) == index + 1);
		} else {
			REQUIRE(get_number_of_edges(rating_graph, index) == 0);
		}
	}
}

TEST_CASE("Rating graph sanity 1") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(100, 100), Interval<dtime_t>(100, 200), 1100, 1, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(10, 18), Interval<dtime_t>(8, 8), 50, 2, 1, 1},
	};

	auto problem = Scheduling_problem<dtime_t>(jobs, std::vector<Precedence_constraint<dtime_t>>());

	Reconfiguration::Rating_graph rating_graph;
	Reconfiguration::Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	rating_graph.generate_dot_file("test_rating_graph_sanity1.dot", problem, std::vector<Reconfiguration::Rating_graph_cut>());
	REQUIRE(rating_graph.nodes[0].get_rating() == 1.0);
	REQUIRE(get_number_of_edges(rating_graph, 0) == 1);
}

TEST_CASE("Rating graph sanity 2") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(10, 18), Interval<dtime_t>(8, 8), 50, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(10, 17), Interval<dtime_t>(8, 8), 50, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(10, 17), Interval<dtime_t>(100, 100), 900, 2, 2, 2},
	};

	auto problem = Scheduling_problem<dtime_t>(jobs, std::vector<Precedence_constraint<dtime_t>>());

	Reconfiguration::Rating_graph rating_graph;
	Reconfiguration::Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	REQUIRE(rating_graph.nodes[0].get_rating() < 1.0);
}

TEST_CASE("25 jobs hang regression") {
	const std::string test_case = "\
Task ID,Job ID,Arrival min,Arrival max,Cost min,Cost max,Deadline,Priority\n\
1,1,0,500,104,209,2000,2\n\
1,2,0,500,14,29,2000,2\n\
1,3,0,500,94,188,2000,2\n\
1,4,5000,5500,104,209,10000,1\n\
1,5,5000,5500,14,29,10000,1\n\
1,6,5000,5500,94,188,10000,1\n\
1,7,10000,10500,104,209,15000,1\n\
1,8,10000,10500,14,29,15000,1\n\
1,9,10000,10500,94,188,15000,1\n\
1,10,15000,15500,104,209,20000,1\n\
1,11,15000,15500,14,29,20000,1\n\
1,12,15000,15500,94,188,20000,1\n\
2,1,0,1000,39,79,10000,2\n\
2,2,0,1000,46,92,10000,2\n\
2,3,0,1000,23,46,10000,2\n\
2,4,10000,11000,39,79,20000,2\n\
2,5,10000,11000,46,92,20000,2\n\
2,6,10000,11000,23,46,20000,2\n\
3,1,0,1000,457,914,10000,3\n\
3,2,0,1000,229,459,10000,3\n\
3,3,0,1000,207,414,10000,3\n\
3,4,10000,11000,457,914,20000,3\n\
3,5,10000,11000,229,459,20000,3\n\
3,6,10000,11000,207,414,20000,3\n\
4,1,0,1000,908,1817,20000,1\n";

	auto jobs = std::istringstream(test_case);

	Scheduling_problem<dtime_t> problem{
			parse_csv_job_file<dtime_t>(jobs),
			std::vector<Precedence_constraint<dtime_t>>()
	};
	REQUIRE(problem.jobs.size() == 25);

	Reconfiguration::Rating_graph rating_graph;
	Reconfiguration::Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	CHECK(rating_graph.nodes[0].get_rating() < 1.0);
}

TEST_CASE("Rating graph with precedence constraints") {
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

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);

	Reconfiguration::Rating_graph rating_graph;
	Reconfiguration::Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	rating_graph.generate_dot_file("rating_graph_precedence.dot", problem, std::vector<Reconfiguration::Rating_graph_cut>());

	CHECK(rating_graph.nodes[0].get_rating() > 0.2);
	CHECK(rating_graph.nodes[0].get_rating() < 0.3);

	size_t node_after0 = get_edge_destination(rating_graph, 0, 0);
	size_t node_after2 = get_edge_destination(rating_graph, 0, 2);
	CHECK(rating_graph.nodes[node_after0].get_rating() == 0.5);
	CHECK(rating_graph.nodes[node_after2].get_rating() == 0.0);

	size_t node_after01 = get_edge_destination(rating_graph, node_after0, 1);
	size_t node_after02 = get_edge_destination(rating_graph, node_after0, 2);
	CHECK(rating_graph.nodes[node_after01].get_rating() == 1.0);
	CHECK(rating_graph.nodes[node_after02].get_rating() == 0.0);
}

#endif
