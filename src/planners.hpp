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

struct IlsParams {
    int maxIterations = 5000;
    int maxNoImprove = 150;  // give up after this many rounds without finding a better plan
    bool cacheScores = false;  // keep candidates' timings and reach gains between insertion steps (same plans, faster)
    double granularity = 0;    // if > 0, only try a town in a gap it is close to one end of (see ils.cpp)

    // Simulated-annealing acceptance, as in SAILS (Gunawan et al. 2017). Off by default: the
    // search then always carries on from the shaken plan.
    bool annealing = false;
    double startTemperature = 0.5;  // as a share of the average rally's reach in the first full plan
    double cooling = 0.99;          // the temperature is multiplied by this every round
    int restartAfter = 50;          // go back to the best plan after this many rounds without improvement
    unsigned seed = 1;              // for the random acceptance; separate from the home-base seed
    bool localSearch = false;  // after each round's insertions, try replace, move and swap moves (#13)
};

struct IlsStats {
    int iterations = 0;    // over both runs
    int improvements = 0;
    int localMoves = 0;    // moves the local search kept, over both runs (only with localSearch)
    double reachFromEmpty = 0;  // best reach of the run that starts from empty routes
    double reachFromA = 0;      // best reach of the run that starts from Approach A's plan
};

// Approach B: iterated local search over all politicians together (see ils.cpp). It runs twice,
// once from empty routes and once from Approach A's plan, and returns the better plan.
Solution solveIls(const Instance& inst, const IlsParams& params, IlsStats* stats = nullptr);
