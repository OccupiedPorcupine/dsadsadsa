#pragma once

#include "instance.hpp"
#include "solution.hpp"

// Baseline: every politician plans greedily as if campaigning alone. Ignores the clash rule,
// so it shows what happens when classic single-traveller planning is reused unchanged.
Solution solveUncoordinated(const Instance& inst);

// Approach A: politicians are planned one after another. Each appends the town with the best
// reach per hour, waiting for or skipping towns that earlier politicians booked.
Solution solveSequential(const Instance& inst);

struct IlsParams {
    int maxIterations = 5000;
    int maxNoImprove = 150;  // stop after this many shake/insert rounds without a better solution
};

struct IlsStats {
    int iterations = 0;
    int improvements = 0;
};

// Approach B: iterated local search (Vansteenwegen et al., 2009) over all politicians at once.
// Towns are inserted anywhere in any route, ranked by gain^2 / extra time; each visit stores how
// far it can be delayed (maxShift) without breaking a later visit, a clash or the horizon, so an
// insertion is checked in O(1). A shake step removes runs of visits to escape local optima.
Solution solveIls(const Instance& inst, const IlsParams& params, IlsStats* stats = nullptr);
