#pragma once

#include "instance.hpp"
#include "solution.hpp"

// Benchmark: the textbook nearest-neighbour TSP tour through every town, closest town first,
// handed to every politician. Ignores population, the time budget and the clash rule.
Solution solveNearestNeighbour(const Instance& inst);

// Benchmark: the same nearest-neighbour rule, but it only goes to towns whose rally still leaves
// time to get home, and stops when none is left. Valid, so it can be compared with A and B.
Solution solveBudgetedNearestNeighbour(const Instance& inst);

// Baseline: each politician plans as if campaigning alone, the way a classic single-traveller
// planner would. It ignores the clash rule, so it shows what goes wrong without coordination.
Solution solveUncoordinated(const Instance& inst);

// Approach A: plan the politicians one after another. Each one keeps adding the town that gives
// the most reach per hour, and has to work around the rallies already booked by earlier politicians.
Solution solveSequential(const Instance& inst);

struct MultiStartParams {
    int starts = 200;   // plans to build; the first is plain Approach A
    double beta = 0.3;  // chance of taking the best option at each step (1 = always, as in A)
    unsigned seed = 1;
};

// Approach A run many times, with the politicians in a random order and each rally picked at
// random from the ranked options, biased towards the best (see sequential.cpp). Keeps the best plan.
Solution solveMultiStart(const Instance& inst, const MultiStartParams& params);

struct IlsParams {
    int maxIterations = 5000;
    int maxNoImprove = 150;  // give up after this many rounds without finding a better plan
};

struct IlsStats {
    int iterations = 0;    // over both runs
    int improvements = 0;
    double reachFromEmpty = 0;  // best reach of the run that starts from empty routes
    double reachFromA = 0;      // best reach of the run that starts from Approach A's plan
};

// Approach B: iterated local search over all politicians together (see ils.cpp). It runs twice,
// once from empty routes and once from Approach A's plan, and returns the better plan.
Solution solveIls(const Instance& inst, const IlsParams& params, IlsStats* stats = nullptr);
