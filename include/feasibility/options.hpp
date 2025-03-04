#ifndef FEASIBILITY_OPTIONS_H
#define FEASIBILITY_OPTIONS_H

namespace NP::Feasibility {
    struct Options {
        bool run_necessary = false;
        bool run_exact = false;
        int z3_model = 0;
        bool run_cplex = false;
        bool run_uppaal = false;
        bool hide_schedule = false;
        int num_threads = 1;
    };
}

#endif
