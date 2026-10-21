#pragma once

#include "instance.hpp"

#include <string>
#include <vector>

// One rally in a politician's route. If start > arrive, the politician waited for the town to be free.
struct Visit {
    int town;
    double arrive;
    double start;
    double finish;
};

// A full plan: routes[p] is politician p's list of rallies, in order.
struct Solution {
    std::vector<std::vector<Visit>> routes;
};

// Everything we report about a plan.
struct Metrics {
    double reach = 0;
    std::vector<double> reachByPolitician;  // in a shared town, whoever came first gets the bigger share
    std::vector<double> returnTime;         // when each politician gets back to HQ
    int visits = 0;
    int clashes = 0;                        // pairs of rallies in the same town at overlapping times
    double travelHours = 0;
    double waitHours = 0;
    std::vector<std::string> errors;        // timing mistakes or late returns

    bool valid() const { return clashes == 0 && errors.empty(); }

    // Lowest politician reach divided by highest. 1 means everyone did equally well.
    double fairness() const;
};

// Checks a plan from scratch, without trusting the planner that made it, and measures it.
Metrics evaluate(const Instance& inst, const Solution& solution);
