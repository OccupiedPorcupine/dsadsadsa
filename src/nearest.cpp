// Benchmarks: the classic nearest-neighbour heuristic for the TSP, as given in the brief.
//
// One tour per politician: start at their home base, keep moving to the closest unvisited town,
// then go home. It knows nothing about population or the other politicians. Each rally starts
// the moment they arrive, or the next morning if it wouldn't finish before the day's window closes.
//
// Two versions:
//   - full tour: visits every town, exactly as in the brief. It runs far past the end of the
//     campaign, so the validator marks it invalid and reports how long it really takes.
//   - budgeted: the same rule, but only towns whose rally still leaves time to get home count as
//     candidates, and it stops when none is left. This is the smallest change that makes the
//     classic heuristic valid for our problem (#12).

#include "planners.hpp"

using namespace std;

static vector<Visit> buildNearestNeighbourTour(const Instance& inst, int politician, bool budgeted) {
    vector<Visit> tour;
    vector<bool> visited(inst.numTowns(), false);
    int home = inst.homeIndex(politician);
    int currentPlace = home;
    double currentTime = 0;

    while (true) {
        int nearest = -1;
        double nearestStart = 0;
        for (int town = 0; town < inst.numTowns(); ++town) {
            if (visited[town]) continue;
            if (nearest != -1 && inst.travelTime(currentPlace, town) >= inst.travelTime(currentPlace, nearest)) continue;

            double arrive = currentTime + inst.travelTime(currentPlace, town);
            double start = inst.clock.fitRally(arrive, inst.rallyHours(town));
            double backHome = start + inst.rallyHours(town) + inst.travelTime(town, home);
            if (budgeted && backHome > inst.campaignHours() + EPS) continue;  // no time to get home after it

            nearest = town;
            nearestStart = start;
        }
        if (nearest == -1) break;  // every town visited, or (budgeted) none fits any more

        double arrive = currentTime + inst.travelTime(currentPlace, nearest);
        double finish = nearestStart + inst.rallyHours(nearest);
        tour.push_back({nearest, arrive, nearestStart, finish});
        visited[nearest] = true;
        currentPlace = nearest;
        currentTime = finish;
    }
    return tour;
}

Solution solveNearestNeighbour(const Instance& inst) {
    Solution solution;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        solution.routes.push_back(buildNearestNeighbourTour(inst, p, false));
    }
    return solution;
}

Solution solveBudgetedNearestNeighbour(const Instance& inst) {
    Solution solution;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        solution.routes.push_back(buildNearestNeighbourTour(inst, p, true));
    }
    return solution;
}
