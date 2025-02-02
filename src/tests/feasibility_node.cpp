#ifndef CONFIG_PARALLEL
#include "doctest.h"
#undef NDEBUG

#include "reconfiguration/cut_loop.hpp"
#include "feasibility/simple_bounds.hpp"
#include "feasibility/node.hpp"
#include "global/space.hpp"
#include "io.hpp"

using namespace NP;
using namespace NP::Feasibility;

TEST_CASE("Feasibility node test on small simple problem with 1 core") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 20), 50, 1, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 10), Interval<dtime_t>(1, 30), 50, 2, 1, 1},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Active_node<dtime_t> good_node(1);
	CHECK(good_node.get_num_dispatched_jobs() == 0);
	good_node.schedule(problem.jobs[0], bounds, predecessor_mapping);
	CHECK(good_node.get_num_dispatched_jobs() == 1);
	good_node.schedule(problem.jobs[1], bounds, predecessor_mapping);
	CHECK(good_node.get_num_dispatched_jobs() == 2);
	CHECK(!good_node.has_missed_deadline());

	Active_node<dtime_t> bad_node(1);
	CHECK(bad_node.get_num_dispatched_jobs() == 0);
	bad_node.schedule(problem.jobs[1], bounds, predecessor_mapping);
	CHECK(bad_node.get_num_dispatched_jobs() == 1);
	bad_node.schedule(problem.jobs[0], bounds, predecessor_mapping);
	CHECK(bad_node.get_num_dispatched_jobs() == 2);
	CHECK(bad_node.has_missed_deadline());
}

TEST_CASE("Feasibility node test on rating_graph problem") {
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

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Active_node<dtime_t> good_node(1);
	good_node.schedule(problem.jobs[0], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[6], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[1], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[8], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[2], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[3], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[7], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[4], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[5], bounds, predecessor_mapping);
	CHECK(good_node.get_num_dispatched_jobs() == 9);
	CHECK(!good_node.has_missed_deadline());

	Active_node<dtime_t> bad_node(1);
	bad_node.schedule(problem.jobs[0], bounds, predecessor_mapping);
	bad_node.schedule(problem.jobs[6], bounds, predecessor_mapping);
	bad_node.schedule(problem.jobs[8], bounds, predecessor_mapping);
	auto early_bad_copy = bad_node.copy();
	bad_node.schedule(problem.jobs[1], bounds, predecessor_mapping);
	CHECK(bad_node.has_missed_deadline());
	CHECK(bad_node.get_num_dispatched_jobs() == 4);

	CHECK(!early_bad_copy->has_missed_deadline());
	CHECK(early_bad_copy->get_num_dispatched_jobs() == 3);
	early_bad_copy->schedule(problem.jobs[1], bounds, predecessor_mapping);
	CHECK(early_bad_copy->has_missed_deadline());
	CHECK(early_bad_copy->get_num_dispatched_jobs() == 4);

	auto late_bad_copy = bad_node.copy();
	CHECK(late_bad_copy->has_missed_deadline());
	CHECK(late_bad_copy->get_num_dispatched_jobs() == 4);
}

TEST_CASE("Feasibility node test with precedence constraints") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 20), 100, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 30), 52, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 30), 100, 2, 2, 2},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints {
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>(0, 2))
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Active_node<dtime_t> violation(1);
	CHECK_THROWS(violation.schedule(problem.jobs[1], bounds, predecessor_mapping));

	Active_node<dtime_t> early_failure(1);
	early_failure.schedule(problem.jobs[2], bounds, predecessor_mapping);
	early_failure.schedule(problem.jobs[0], bounds, predecessor_mapping);
	CHECK(early_failure.has_missed_deadline());
	CHECK(early_failure.get_num_dispatched_jobs() == 2);

	Active_node<dtime_t> good_node(1);
	good_node.schedule(problem.jobs[0], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[1], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[2], bounds, predecessor_mapping);
	CHECK(good_node.get_num_dispatched_jobs() == 3);
	CHECK(!good_node.has_missed_deadline());
}

TEST_CASE("Feasibility node test with mixed signal-at precedence constraints") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 20), 20, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 30), 32, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 30), 100, 2, 2, 2},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints {
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[1].get_id(), Interval<dtime_t>(0, 2), false),
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(0, 10)),
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints, 2);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Active_node<dtime_t> violation(2);
	CHECK_THROWS(violation.schedule(problem.jobs[1], bounds, predecessor_mapping));

	Active_node<dtime_t> bad_node(2);
	bad_node.schedule(problem.jobs[0], bounds, predecessor_mapping);
	bad_node.schedule(problem.jobs[2], bounds, predecessor_mapping);
	bad_node.schedule(problem.jobs[1], bounds, predecessor_mapping);
	CHECK(bad_node.has_missed_deadline());
	CHECK(bad_node.get_num_dispatched_jobs() == 3);

	Active_node<dtime_t> good_node(2);
	good_node.schedule(problem.jobs[0], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[1], bounds, predecessor_mapping);
	good_node.schedule(problem.jobs[2], bounds, predecessor_mapping);
	CHECK(good_node.get_num_dispatched_jobs() == 3);
	CHECK(!good_node.has_missed_deadline());
}

TEST_CASE("Feasibility node merge: get failing child from 2 succeeding parents") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 100, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 100, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 200, 2, 2, 2},
			Job<dtime_t>{3, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 200, 3, 3, 3},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints {
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(0, 10)),
			Precedence_constraint<dtime_t>(jobs[1].get_id(), jobs[3].get_id(), Interval<dtime_t>(0, 10)),
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Active_node<dtime_t> parent1(1);
	parent1.schedule(jobs[0], bounds, predecessor_mapping);
	parent1.schedule(jobs[1], bounds, predecessor_mapping);

	auto parent1_copy = parent1.copy();
	parent1_copy->schedule(jobs[2], bounds, predecessor_mapping);
	parent1_copy->schedule(jobs[3], bounds, predecessor_mapping);
	CHECK(!parent1_copy->has_missed_deadline());

	// Trace of parent1_copy:
	// - schedule job 0, which starts at time 0 and ends at time 50
	// - schedule job 1, which starts at time 50 and ends at time 100
	// - schedule job 2, which starts at time 100 and ends at time 150
	// - schedule job 3, which starts at time 150 and ends at time 200

	Active_node<dtime_t> parent2(1);
	parent2.schedule(jobs[1], bounds, predecessor_mapping);
	parent2.schedule(jobs[0], bounds, predecessor_mapping);

	parent1.merge(parent2);
	parent1.schedule(jobs[2], bounds, predecessor_mapping);
	parent1.schedule(jobs[3], bounds, predecessor_mapping);
	CHECK(parent1.has_missed_deadline());
	parent2.schedule(jobs[3], bounds, predecessor_mapping);
	parent2.schedule(jobs[2], bounds, predecessor_mapping);
	CHECK(!parent2.has_missed_deadline());

	// Trace of parent2:
	// - schedule job 1, which starts at time 0 and ends at time 50
	// - schedule job 0, which starts at time 50 and ends at time 100
	// - schedule job 3, which starts at time 100 and ends at time 150
	// - schedule job 2, which starts at time 150 and ends at time 100

	// Trace of parent1 (merged):
	// - schedule job 0, which starts at time 0 and ends at time 50
	// - schedule job 1, which starts at time 50 and ends at time 100
	// MERGE with parent2: the latest end time of job 0 becomes 100 because job0 ends at time 100 in parent2
	// - schedule job 2, which starts at time 110 (SUSPENSION) and ends at time 160
	// - schedule job 3, which starts at time 160 and ends at time 210 -> DEADLINE MISS
}

TEST_CASE("Feasibility node merge: get succeeding child from 2 succeeding parents") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 100, 0, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 100, 1, 1, 1},

			Job<dtime_t>{2, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 200, 2, 2, 2},
			Job<dtime_t>{3, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 50), 200, 3, 3, 3},
	};

	std::vector<Precedence_constraint<dtime_t>> precedence_constraints { // Note that the suspensions are gone
			Precedence_constraint<dtime_t>(jobs[0].get_id(), jobs[2].get_id(), Interval<dtime_t>(0, 0)),
			Precedence_constraint<dtime_t>(jobs[1].get_id(), jobs[3].get_id(), Interval<dtime_t>(0, 0)),
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, precedence_constraints);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Active_node<dtime_t> parent1(1);
	parent1.schedule(jobs[0], bounds, predecessor_mapping);
	parent1.schedule(jobs[1], bounds, predecessor_mapping);

	Active_node<dtime_t> parent2(1);
	parent2.schedule(jobs[1], bounds, predecessor_mapping);
	parent2.schedule(jobs[0], bounds, predecessor_mapping);

	parent1.merge(parent2);
	parent1.schedule(jobs[2], bounds, predecessor_mapping);
	parent1.schedule(jobs[3], bounds, predecessor_mapping);
	CHECK(!parent1.has_missed_deadline());

	// Unlike the previous test case, this will succeed because there are no suspensions
}

TEST_CASE("Almost-unschedulable simple feasibility node regression test") {
	auto jobs_file_input = std::ifstream("../examples/almost-unschedulable-job-sets/jitter15.csv", std::ios::in);
	auto prec_file_input = std::ifstream("../examples/almost-unschedulable-job-sets/jitter15.prec.csv", std::ios::in);
	auto problem = Scheduling_problem<dtime_t>(NP::parse_csv_job_file<dtime_t>(jobs_file_input), NP::parse_precedence_file<dtime_t>(prec_file_input), 3);
	REQUIRE(problem.jobs[13].get_task_id() == 4);
	REQUIRE(problem.jobs[13].get_job_id() == 1);
	REQUIRE(problem.jobs[13].maximal_exec_time() == 24170000);
	REQUIRE(problem.jobs[16].get_task_id() == 4);
	REQUIRE(problem.jobs[16].get_job_id() == 4);

	const auto bounds = Feasibility::compute_simple_bounds(problem);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	REQUIRE(bounds.earliest_pessimistic_start_times[16] == 24170000);

	Reconfiguration::Rating_graph rating_graph;
	Reconfiguration::Agent_rating_graph<dtime_t>::generate(problem, rating_graph);
	Feasibility_graph<dtime_t> feasibility_graph(rating_graph);
	feasibility_graph.explore_forward(problem, compute_simple_bounds(problem), predecessor_mapping);
}

TEST_CASE("Feasibility node predict start time with 1 core") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 20), 50, 1, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 10), Interval<dtime_t>(1, 30), 50, 2, 1, 1},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Active_node<dtime_t> node(1);
	CHECK(node.predict_start_time(problem.jobs[0], predecessor_mapping) == 0);
	CHECK(node.predict_next_start_time(problem.jobs[0], predecessor_mapping) == 20);
	CHECK(node.predict_start_time(problem.jobs[1], predecessor_mapping) == 10);
	CHECK(node.predict_next_start_time(problem.jobs[1], predecessor_mapping) == 40);

	node.schedule(jobs[0], bounds, predecessor_mapping);
	CHECK(node.predict_start_time(problem.jobs[1], predecessor_mapping) == 20);
	CHECK(node.predict_next_start_time(problem.jobs[1], predecessor_mapping) == 50);
}

TEST_CASE("Feasibility node predict start time with 2 cores - start with job 0") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 20), 50, 1, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 10), Interval<dtime_t>(1, 30), 50, 2, 1, 1},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, 2);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Active_node<dtime_t> node(2);
	CHECK(node.predict_start_time(problem.jobs[0], predecessor_mapping) == 0);
	CHECK(node.predict_next_start_time(problem.jobs[0], predecessor_mapping) == 0);
	CHECK(node.predict_start_time(problem.jobs[1], predecessor_mapping) == 10);
	CHECK(node.predict_next_start_time(problem.jobs[1], predecessor_mapping) == 10);

	node.schedule(jobs[0], bounds, predecessor_mapping);
	CHECK(node.predict_start_time(problem.jobs[1], predecessor_mapping) == 10);
	CHECK(node.predict_next_start_time(problem.jobs[1], predecessor_mapping) == 20);
}

TEST_CASE("Feasibility node predict start time with 2 cores - start with job 1") {
	Global::State_space<dtime_t>::Workload jobs {
			Job<dtime_t>{0, Interval<dtime_t>(0, 0), Interval<dtime_t>(1, 20), 50, 1, 0, 0},
			Job<dtime_t>{1, Interval<dtime_t>(0, 10), Interval<dtime_t>(1, 30), 50, 2, 1, 1},
	};

	const auto problem = Scheduling_problem<dtime_t>(jobs, 2);
	const auto predecessor_mapping = create_predecessor_mapping(problem);
	const auto bounds = compute_simple_bounds(problem);

	Active_node<dtime_t> node(2);
	node.schedule(jobs[1], bounds, predecessor_mapping);
	CHECK(node.predict_start_time(problem.jobs[0], predecessor_mapping) == 10);
	CHECK(node.predict_next_start_time(problem.jobs[0], predecessor_mapping) == 30);
}

#endif
