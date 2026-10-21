#pragma once

#include "instance.hpp"

#include <string>
#include <vector>

struct Visit {
    int town;
    double arrive, start, finish;  // start > arrive means the politician waited for the town to be free
};

struct Solution {
    std::vector<std::vector<Visit>> routes;  // routes[p] = politician p's visits in order
};

struct Metrics {
    double reach = 0;
    std::vector<double> reachByPolitician;  // the earlier rally in a town gets the larger share
    std::vector<double> returnTime;
    int visits = 0;
    int clashes = 0;       // pairs of rallies in the same town at overlapping times
    double travelHours = 0;
    double waitHours = 0;
    std::vector<std::string> errors;  // timing or horizon violations

    bool valid() const { return clashes == 0 && errors.empty(); }
    double fairness() const;  // lowest politician reach / highest politician reach
};

// Checks a solution from scratch, independent of how it was produced.
Metrics evaluate(const Instance& inst, const Solution& sol);
