#include <iostream>
#include <sstream>
#include <fstream>
#include <algorithm>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#include "OptionParser.h"

#include "config.h"

#ifdef CONFIG_PARALLEL

#include <oneapi/tbb/info.h>
#include <oneapi/tbb/parallel_for.h>
#include <oneapi/tbb/task_arena.h>

#endif

#include "problem.hpp"
#include "global/space.hpp"
#include "io.hpp"
#include "clock.hpp"
#include "memory_used.hpp"
#include "reconfiguration/manager.hpp"
#include "feasibility/options.hpp"
#include "feasibility/manager.hpp"

#define MAX_PROCESSORS 512

// command line options
static bool want_verbose;
static bool want_naive;
static bool merge_conservative;
static bool merge_use_job_finish_times;
static int merge_depth;
static bool want_dense;

static bool want_precedence = false;
static std::string precedence_file;

static bool want_aborts = false;
static std::string aborts_file;

static bool want_multiprocessor = false;
static unsigned int num_processors = 1;

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
static bool want_dot_graph;
#endif
static double timeout;
static unsigned int max_depth = 0;

static bool want_rta_file;
static bool want_width_file;

static bool continue_after_dl_miss = false;

static NP::Feasibility::Options feasibility_options;
static NP::Reconfiguration::Options reconfigure_options;

#ifdef CONFIG_PARALLEL
static unsigned int num_worker_threads = 0;
#endif

struct Analysis_result {
	bool schedulable;
	bool timeout;
	unsigned long long number_of_nodes, number_of_states, number_of_edges, max_width, number_of_jobs;
	double cpu_time;
	std::string graph;
	std::string response_times_csv;
	std::string width_evolution_csv;
};


template<class Time, class Space>
static Analysis_result analyze(
	std::istream &in,
	std::istream &prec_in,
	std::istream &aborts_in,
    bool &is_yaml)
{
#ifdef CONFIG_PARALLEL
	oneapi::tbb::task_arena arena(num_worker_threads ? num_worker_threads : oneapi::tbb::info::default_concurrency());
#endif


	// Parse input files and create NP scheduling problem description
    typename NP::Job<Time>::Job_set jobs = is_yaml ? NP::parse_yaml_job_file<Time>(in) : NP::parse_csv_job_file<Time>(in);
	// Parse precedence constraints
	std::vector<NP::Precedence_constraint<Time>> edges = is_yaml ? NP::parse_yaml_dag_file<Time>(prec_in) : NP::parse_precedence_file<Time>(prec_in);

	NP::Scheduling_problem<Time> problem{
        jobs,
		edges,
		NP::parse_abort_file<Time>(aborts_in),
		num_processors};

	if (feasibility_options.run_necessary) {
		NP::Feasibility::run_necessary_tests(problem);
		exit(0);
	}
	if (feasibility_options.run_exact) {
		NP::Feasibility::run_exact_test(problem, reconfigure_options.safe_search, feasibility_options.num_threads, timeout, !feasibility_options.hide_schedule);
		exit(0);
	}
	if (feasibility_options.z3_model != 0 || feasibility_options.run_cplex || feasibility_options.run_uppaal) {
		if constexpr (std::is_same_v<Time, dtime_t>) {
			if (feasibility_options.z3_model != 0) {
				NP::Feasibility::run_z3(problem, !feasibility_options.hide_schedule, feasibility_options.z3_model, timeout);
				exit(0);
			}
			if (feasibility_options.run_cplex) {
				NP::Feasibility::run_cplex(problem, !feasibility_options.hide_schedule, timeout);
				exit(0);
			}
			if (feasibility_options.run_uppaal) {
				NP::Feasibility::run_uppaal(problem, !feasibility_options.hide_schedule);
				exit(0);
			}
		} else {
			throw std::runtime_error("Our z3, cplex, and uppaal models only support discrete time");
		}
	}

	if (reconfigure_options.enabled) {
		NP::Reconfiguration::run(reconfigure_options, problem);
		exit(0);
	}

	// Set common analysis options
	NP::Analysis_options opts;
	opts.verbose = want_verbose;
	opts.timeout = timeout;
	opts.max_depth = max_depth;
	opts.early_exit = !continue_after_dl_miss;
	opts.be_naive = want_naive;
	opts.merge_conservative = merge_conservative;
	opts.merge_depth = merge_depth;
	opts.merge_use_job_finish_times = merge_use_job_finish_times;

	// Actually call the analysis engine
	auto space = Space::explore(problem, opts);

	// Extract the analysis results
	auto graph = std::ostringstream();
#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
	if (want_dot_graph)
		graph << *space;
#endif

	auto rta = std::ostringstream();

	if (want_rta_file) {
		rta << "Task ID, Job ID, BCCT, WCCT, BCRT, WCRT" << std::endl;
		for (const auto& j : problem.jobs) {
			Interval<Time> finish = space->get_finish_times(j);
			rta << j.get_task_id() << ", "
			    << j.get_job_id() << ", "
			    << finish.from() << ", "
			    << finish.until() << ", "
			    << std::max<long long>(0,
			                           (finish.from() - j.earliest_arrival()))
			    << ", "
			    << (finish.until() - j.earliest_arrival())
			    << std::endl;
		}
	}

	auto width_stream = std::ostringstream();
	if (want_width_file) {
		width_stream << "Depth, Width (#Nodes), Width (#States)" << std::endl;
		const std::vector<std::pair<unsigned long, unsigned long>>& width = space->evolution_exploration_front_width();
		for (int d = 0; d < problem.jobs.size(); d++) {
			width_stream << d << ", "
					   << width[d].first
					   << ", "
					   << width[d].second
					   << std::endl;
		}
	}

	Analysis_result results = Analysis_result{
		space->is_schedulable(),
		space->was_timed_out(),
		space->number_of_nodes(),
		space->number_of_states(),
		space->number_of_edges(),
		space->max_exploration_front_width(),
		(unsigned long)(problem.jobs.size()),
		space->get_cpu_time(),
		graph.str(),
		rta.str(),
		width_stream.str()
	};
	delete space;
	return results;
}

static Analysis_result process_stream(
	std::istream &in,
	std::istream &prec_in,
	std::istream &aborts_in,
    bool is_yaml)
{
	if (want_multiprocessor && want_dense)
		return analyze<dense_t, NP::Global::State_space<dense_t>>(in, prec_in, aborts_in, is_yaml);
	else if (want_multiprocessor && !want_dense)
		return analyze<dtime_t, NP::Global::State_space<dtime_t>>(in, prec_in, aborts_in, is_yaml);
	else if (want_dense)
		return analyze<dense_t, NP::Global::State_space<dense_t>>(in, prec_in, aborts_in, is_yaml);
	else
		return analyze<dtime_t, NP::Global::State_space<dtime_t>>(in, prec_in, aborts_in, is_yaml);
}

static void process_file(const std::string& fname)
{
	try {
		Analysis_result result;

		auto empty_dag_stream = std::istringstream("\n");
		auto empty_aborts_stream = std::istringstream("\n");
		auto dag_stream = std::ifstream();
		auto aborts_stream = std::ifstream();

		if (want_precedence)
			dag_stream.open(precedence_file);

		if (want_aborts)
			aborts_stream.open(aborts_file);

		std::istream &dag_in = want_precedence ?
			static_cast<std::istream&>(dag_stream) :
			static_cast<std::istream&>(empty_dag_stream);

		std::istream &aborts_in = want_aborts ?
			static_cast<std::istream&>(aborts_stream) :
			static_cast<std::istream&>(empty_aborts_stream);

		if (fname == "-")
		{
			result = process_stream(std::cin, dag_in, aborts_in, false);
		}
		else {
            // check the extension of the file
            std::string ext = fname.substr(fname.find_last_of(".") + 1);
            bool is_yaml = false;
            if (ext == "yaml" || ext == "yml") {
                is_yaml = true;
            }

			auto in = std::ifstream(fname, std::ios::in);
			result = process_stream(in, dag_in, aborts_in, is_yaml);

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
			if (want_dot_graph) {
				DM("\nDot graph being made\n");
				std::string dot_name = fname;
				auto p = is_yaml ? dot_name.find(".yaml") : dot_name.find(".csv");
				if (p != std::string::npos) {
					dot_name.replace(p, std::string::npos, ".dot");
					auto out  = std::ofstream(dot_name,  std::ios::out);
					out << result.graph;
					out.close();
				}
			}
#endif
			if (want_rta_file) {
				std::string rta_name = fname;
				auto p = is_yaml ? rta_name.find(".yaml") : rta_name.find(".csv");
				if (p != std::string::npos) {
					rta_name.replace(p, std::string::npos, ".rta.csv");
					auto out  = std::ofstream(rta_name,  std::ios::out);
					out << result.response_times_csv;
					out.close();
				}
			}

			if (want_width_file) {
				std::string width_file_name = fname;
				auto p = is_yaml ? width_file_name.find(".yaml") : width_file_name.find(".csv");
				if (p != std::string::npos) {
					width_file_name.replace(p, std::string::npos, ".width.csv");
					auto out = std::ofstream(width_file_name, std::ios::out);
					out << result.width_evolution_csv;
					out.close();
				}
			}
		}

		std::cout << fname;

		if (max_depth && max_depth < result.number_of_jobs)
			// mark result as invalid due to debug abort
			std::cout << ",  X";
		else
			std::cout << ",  " << (int) result.schedulable;

		std::cout << ",  " << result.number_of_jobs
			  << ",  " << result.number_of_nodes
		          << ",  " << result.number_of_states
		          << ",  " << result.number_of_edges
		          << ",  " << result.max_width
		          << ",  " << std::fixed << result.cpu_time
		          << ",  " << ((double) NP::get_peak_memory_usage()) / (1024.0)
		          << ",  " << (int) result.timeout
		          << ",  " << num_processors
		          << std::endl;
	} catch (std::ios_base::failure& ex) {
		std::cerr << fname;
		if (want_precedence)
			std::cerr << " + " << precedence_file;
		std::cerr <<  ": parse error" << std::endl;
		exit(1);
	} catch (NP::InvalidJobReference& ex) {
		std::cerr << precedence_file << ": bad job reference: job "
				  << ex.ref.job << " of task " << ex.ref.task
				  << " is not part of the job set given in "
				  << fname
				  << std::endl;
		exit(3);
	} catch (NP::InvalidAbortParameter& ex) {
		std::cerr << aborts_file << ": invalid abort parameter: job "
				  << ex.ref.job << " of task " << ex.ref.task
				  << " has an impossible abort time (abort before release)"
				  << std::endl;
		exit(4);
	} catch (NP::InvalidPrecParameter& ex) {
		std::cerr << precedence_file << ": invalid self-suspending parameter: job "
				  << ex.ref.job << " of task " << ex.ref.task
				  << " has an invalid self-suspending time"
				  << std::endl;
		exit(5);
	} catch (std::exception& ex) {
		std::cerr << fname << ": '" << ex.what() << "'" << std::endl;
		exit(1);
	}
}

static void print_header(){
	std::cout << "# file name"
	          << ", schedulable?(1Y/0N)"
	          << ", #jobs"
	          << ", #nodes"
	          << ", #states"
	          << ", #edges"
	          << ", max width"
	          << ", CPU time"
	          << ", memory"
	          << ", timeout"
	          << ", #CPUs"
	          << std::endl;
}

int main(int argc, char** argv)
{
	auto parser = optparse::OptionParser();

	parser.description("Exact NP Schedulability Tester");
	parser.usage("usage: %prog [OPTIONS]... [JOB SET FILES]...");

	// add an option to show the version
	parser.add_option("-v", "--version").dest("version")
		.action("store_true").set_default("0")
		.help("show program's version number and exit");


	parser.add_option("--merge").dest("merge_opts")
		.metavar("MERGE-LEVEL")
		.choices({ "no", "c1", "c2", "l1", "l2", "l3", "lmax"}).choices({ "no", "c1", "c2","l1","l2","l3","lmax"}).set_default("l1")
		.help("choose type of state merging approach used during the analysis. 'no': no merging, 'c1': conservative level 1, 'c2': conservative level 2, 'lx': lossy with depth=x, 'lmax': lossy with max depth. (default: l1)");

	parser.add_option("-t", "--time").dest("time_model")
	      .metavar("TIME-MODEL")
	      .choices({"dense", "discrete"}).set_default("discrete")
	      .help("choose 'discrete' or 'dense' time (default: discrete)");

	parser.add_option("-l", "--time-limit").dest("timeout")
	      .help("maximum CPU time allowed (in seconds, zero means no limit)")
	      .set_default("0");

	parser.add_option("-d", "--depth-limit").dest("depth")
	      .help("abort graph exploration after reaching given depth (>= 2)")
	      .set_default("0");

	/*parser.add_option("-n", "--naive").dest("naive").set_default("0")
	      .action("store_const").set_const("1")
	      .help("use the naive exploration method (default: merging)");*/

	parser.add_option("-p", "--precedence").dest("precedence_file")
	      .help("name of the file that contains the job set's precedence DAG")
	      .set_default("");

	parser.add_option("-a", "--abort-actions").dest("abort_file")
	      .help("name of the file that contains the job set's abort actions")
	      .set_default("");

	parser.add_option("-m", "--multiprocessor").dest("num_processors")
	      .help("set the number of processors of the platform")
	      .set_default("1");

	parser.add_option("--threads").dest("num_threads")
	      .help("set the number of worker threads (parallel analysis)")
	      .set_default("0");

	parser.add_option("--header").dest("print_header")
	      .help("print a column header")
	      .action("store_const").set_const("1")
	      .set_default("0");

	parser.add_option("-g", "--save-graph").dest("dot").set_default("0")
	      .action("store_const").set_const("1")
	      .help("store the state graph in Graphviz dot format (default: off)");

	parser.add_option("--verbose").dest("verbose").set_default("0")
		.action("store_const").set_const("1")
		.help("show the current status of the analysis (default: off)");

	parser.add_option("-r", "--report").dest("rta").set_default("0")
	      .action("store_const").set_const("1")
	      .help("Reporting: store the best- and worst-case response times and store the evolution of the width of the graph (default: off)");

	parser.add_option("-c", "--continue-after-deadline-miss")
	      .dest("go_on_after_dl").set_default("0")
	      .action("store_const").set_const("1")
	      .help("do not abort the analysis on the first deadline miss "
	            "(default: off)");

	parser.add_option("--reconfigure").dest("reconfigure")
			.help("try to automatically reconfigure the system when the job set is deemed unschedulable")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--reconfigure-skip-rating-graph").dest("reconfigure-skip-rating-graph")
			.help("when --reconfigure is enabled, skips the rating graph generation, and always tries to create a safe path from scratch")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--reconfigure-dry-rating-graphs").dest("reconfigure-dry-rating-graphs")
			.help("when --reconfigure is enabled, this determines whether a dry run will be done before every rating graph construction, which reduces memory, but takes more time")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--reconfigure-rating-timeout").dest("reconfigure-rating-timeout")
			.help("when --reconfigure is enabled, this specifies the timeout (seconds) for the initial rating graph construction")
			.set_default(0);
	parser.add_option("--reconfigure-threads").dest("reconfigure-threads")
			.help("when --reconfigure is enabled, this specifies the number of threads that will be used for several analyses (1 by default)")
			.set_default(1);
	parser.add_option("--reconfigure-z3").dest("reconfigure-z3")
			.help("when --reconfigure is enabled, and the root node is unsafe, this option determines whether it should use z3 to find a safe job ordering")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--reconfigure-cplex").dest("reconfigure-cplex")
			.help("when --reconfigure is enabled, and the root node is unsafe, this option determines whether it should use cplex to find a safe job ordering")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--reconfigure-feasibility-graph-timeout").dest("reconfigure-feasibility-graph-timeout")
			.help("when --reconfigure is enabled and the root node is unsafe, this specifies how much time will be spent to search for a safe job ordering in the feasibility graph, before trying to build it from scratch")
			.set_default(2.0);
	parser.add_option("--reconfigure-safe-search-job-skip-chance").dest("reconfigure-safe-search-job-skip-chance")
			.help("when --reconfigure is enabled and a safe job ordering needs to be made from scratch, this determines the chance to skip the job selected by heuristics")
			.set_default(50);
	parser.add_option("--reconfigure-safe-search-history-size").dest("reconfigure-safe-search-history-size")
			.help("when --reconfigure is enabled and a safe job ordering needs to be made from scratch, this determines the number of most promising prefixes that will be remembered and used")
			.set_default(100);
	parser.add_option("--reconfigure-safe-search-timeout").dest("reconfigure-safe-search-timeout")
			.help("when --reconfigure is enabled, this specifies the timeout (seconds) of the safe job ordering search")
			.set_default(0);
	parser.add_option("--reconfigure-enforce-safe-path").dest("reconfigure-enforce-safe-path")
			.help("when --reconfigure is enabled, always start by enforcing the entire safe path/job ordering, rather than trying to start with a minimal version")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--reconfigure-max-cuts-per-iteration").dest("reconfigure-max-cuts-per-iteration")
			.help("when --reconfigure is enabled, this specifies the maximum number of cuts that can be performed per cut iteration (unlimited by default)")
			.set_default(0);
	parser.add_option("--reconfigure-enforce-timeout").dest("reconfigure-enforce-timeout")
			.help("when --reconfigure is enabled, this specifies the timeout (seconds) of the cut enforcement")
			.set_default(0);
	parser.add_option("--reconfigure-random-trials").dest("reconfigure-random-trials")
			.help("when --reconfigure is enabled, this option causes the manager to reduce the number of redundant constraints using random trial-and-error, rather than tail trial-and-error")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--reconfigure-minimize-timeout").dest("reconfigure-minimize-timeout")
			.help("when --reconfigure is enabled, this specifies the timeout (seconds) of the constraint minimization")
			.set_default(0);

	parser.add_option("--feasibility-necessary").dest("feasibility-necessary")
			.help("Instead of doing a schedulability analysis, we will run some necessary feasibility tests")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--feasibility-exact").dest("feasibility-exact")
			.help("Instead of doing a schedulability analysis, we will run some necessary feasibility tests, followed by a possibly-endless sufficient feasibility test")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--feasibility-z3").dest("feasibility-z3")
			.help("Instead of doing a schedulability analysis, we will run an exact feasibility test using our z3 model 1 or 2")
			.set_default(0);
	parser.add_option("--feasibility-cplex").dest("feasibility-cplex")
			.help("Instead of doing a schedulability analysis, we will run an exact feasibility test using our cplex model")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--feasibility-uppaal").dest("feasibility-uppaal")
			.help("Instead of doing a schedulability analysis, we will run an exact feasibility test using our uppaal model")
			.action("store_const").set_const("1").set_default("0");
	parser.add_option("--feasibility-threads").dest("feasibility-threads")
			.help("when --feasibility-exact is enabled, this specifies the number of threads that will be used for the sufficient feasibility test")
			.set_default(1);
	parser.add_option("--feasibility-hide-schedule").dest("feasibility-hide-schedule")
			.help("When --feasibility-exact, --feasibility-z3, or --feasibility-cplex is enabled, don't print the feasible schedule")
			.action("store_const").set_const("1").set_default("0");

	auto options = parser.parse_args(argc, argv);
	//all the options that could have been entered above are processed below and appropriate variables
	// are assigned their respective values.

	if(options.get("version")){
		std::cout << parser.prog() <<" version "
				  	<< VERSION_MAJOR << "."
					<< VERSION_MINOR << "."
					<< VERSION_PATCH << std::endl;
		return 0;
	}

	std::string merge_opts = (const std::string&)options.get("merge_opts");
	want_naive = (merge_opts == "no");
	if (merge_opts == "c1") {
		merge_conservative = true;
		merge_use_job_finish_times = false;
	}
	else if (merge_opts == "c2") {
		merge_conservative = true;
		merge_use_job_finish_times = true;
	}
	if (merge_opts == "l2")
		merge_depth = 2;
	else if (merge_opts == "l3")
		merge_depth = 3;
	else if (merge_opts == "lmax")
		merge_depth = -1;
	else
		merge_depth = 1;

	std::string time_model = (const std::string&)options.get("time_model");
	want_dense = time_model == "dense";

	//want_naive = options.get("naive");

	timeout = options.get("timeout");

	max_depth = options.get("depth");
	if (options.is_set_by_user("depth")) {
		if (max_depth <= 1) {
			std::cerr << "Error: invalid depth argument\n" << std::endl;
			return 1;
		}
		max_depth -= 1;
	}

	want_precedence = options.is_set_by_user("precedence_file");
	if (want_precedence && parser.args().size() > 1) {
		std::cerr << "[!!] Warning: multiple job sets "
		          << "with a single precedence DAG specified."
		          << std::endl;
	}
	precedence_file = (const std::string&) options.get("precedence_file");

	want_aborts = options.is_set_by_user("abort_file");
	if (want_aborts && parser.args().size() > 1) {
		std::cerr << "[!!] Warning: multiple job sets "
		          << "with a single abort action list specified."
		          << std::endl;
	}
	aborts_file = (const std::string&) options.get("abort_file");

	want_multiprocessor = options.is_set_by_user("num_processors");
	num_processors = options.get("num_processors");
	if (!num_processors || num_processors > MAX_PROCESSORS) {
		std::cerr << "Error: invalid number of processors\n" << std::endl;
		return 1;
	}

	want_rta_file = options.get("rta");
	want_width_file = options.get("rta");

	want_verbose = options.get("verbose");

	continue_after_dl_miss = options.get("go_on_after_dl");

	feasibility_options.run_necessary = options.get("feasibility-necessary");
	feasibility_options.run_exact = options.get("feasibility-exact");
	feasibility_options.z3_model = options.get("feasibility-z3");
	feasibility_options.run_cplex = options.get("feasibility-cplex");
	feasibility_options.run_uppaal = options.get("feasibility-uppaal");
	feasibility_options.num_threads = options.get("feasibility-threads");
	feasibility_options.hide_schedule = options.get("feasibility-hide-schedule");

	reconfigure_options.enabled = options.get("reconfigure");
	reconfigure_options.skip_rating_graph = options.get("reconfigure-skip-rating-graph");
	reconfigure_options.dry_rating_graphs = options.get("reconfigure-dry-rating-graphs");
	reconfigure_options.rating_timeout = options.get("reconfigure-rating-timeout");
	reconfigure_options.num_threads = options.get("reconfigure-threads");
	reconfigure_options.use_z3 = options.get("reconfigure-z3");
	reconfigure_options.use_cplex = options.get("reconfigure-cplex");
	reconfigure_options.feasibility_graph_timeout = options.get("reconfiguration-feasibility-graph-timeout");
	reconfigure_options.safe_search.job_skip_chance = options.get("reconfigure-safe-search-job-skip-chance");
	reconfigure_options.safe_search.history_size = options.get("reconfigure-safe-search-history-size");
	reconfigure_options.safe_search.timeout = options.get("reconfigure-safe-search-timeout");
	reconfigure_options.enforce_safe_path = options.get("reconfigure-enforce-safe-path");
	reconfigure_options.max_cuts_per_iteration = options.get("reconfigure-max-cuts-per-iteration");
	reconfigure_options.enforce_timeout = options.get("reconfigure-enforce-timeout");
	reconfigure_options.use_random_analysis = options.get("reconfigure-random-trials");
	reconfigure_options.minimize_timeout = options.get("reconfigure-minimize-timeout");

#ifdef CONFIG_COLLECT_SCHEDULE_GRAPH
	want_dot_graph = options.get("dot");
	DM("Dot graph"<<want_dot_graph<<std::endl);
#else
	if (options.is_set_by_user("dot")) {
		std::cerr << "Error: graph collection support must be enabled "
		          << "during compilation (CONFIG_COLLECT_SCHEDULE_GRAPH "
		          << "is not set)." << std::endl;
		return 2;
	}
#endif

#ifdef CONFIG_PARALLEL
	num_worker_threads = options.get("num_threads");
#else
	if (options.is_set_by_user("num_threads")) {
		std::cerr << "Error: parallel analysis must be enabled "
		          << "during compilation (CONFIG_PARALLEL "
		          << "is not set)." << std::endl;
		return 3;
	}
#endif

	// this prints the header in the output on the console
	if (options.get("print_header"))
		print_header();

	// process_file is given the arguments that have been passed
	for (auto f : parser.args())
		process_file(f);

	if (parser.args().empty())
		process_file("-");

	return 0;
}
