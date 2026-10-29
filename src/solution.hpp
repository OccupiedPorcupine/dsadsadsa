#pragma once

#include "instance.hpp"

#include <string>
#include <vector>

// One rally in a politician's route, in active hours. If start > arrive, the politician waited,
// either for the town to be free or for the next morning because the rally didn't fit in the day.
struct Visit {
    int town;
    double arrive;
    double start;
    double finish;
    bool clash = false;  // the planner chose to overlap another politician's rally here instead of waiting
};

// A full plan: routes[p] is politician p's list of rallies, in order.
struct Solution {
    std::vector<std::vector<Visit>> routes;
};

// Everything we report about a plan.
struct Metrics {
    double reach = 0;
    std::vector<double> reachByPolitician;
    std::vector<std::vector<double>> visitReach;  // visitReach[p][i]: reach of politician p's i-th rally
    std::vector<double> returnTime;         // when each politician gets back to their home base
    int visits = 0;
    int clashes = 0;                        // pairs of rallies in the same town at overlapping times
    double clashHours = 0;                  // total overlapping time over those pairs
    double travelHours = 0;
    double waitHours = 0;                   // waiting for a town to be free or for the next morning
    std::vector<std::string> errors;        // timing mistakes, rallies outside the window, late returns

    // Clashes are allowed (they cost reach), so only rule-breaking makes a plan invalid.
    bool valid() const { return errors.empty(); }

    // Lowest politician reach divided by highest. 1 means everyone did equally well.
    double fairness() const;
};

// Checks a plan from scratch, without trusting the planner that made it, and measures it.
Metrics evaluate(const Instance& inst, const Solution& solution);
