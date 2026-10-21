#pragma once

#include "instance.hpp"
#include "solution.hpp"

// Baseline: each politician plans as if campaigning alone, the way a classic single-traveller
// planner would. It ignores the clash rule, so it shows what goes wrong without coordination.
Solution solveUncoordinated(const Instance& inst);

// Approach A: plan the politicians one after another. Each one keeps adding the town that gives
// the most reach per hour, and has to work around the rallies already booked by earlier politicians.
Solution solveSequential(const Instance& inst);

struct IlsParams {
    int maxIterations = 5000;
    int maxNoImprove = 150;  // give up after this many rounds without finding a better plan
};

struct IlsStats {
    int iterations = 0;
    int improvements = 0;
};

// Approach B: iterated local search over all politicians together (see ils.cpp).
Solution solveIls(const Instance& inst, const IlsParams& params, IlsStats* stats = nullptr);
