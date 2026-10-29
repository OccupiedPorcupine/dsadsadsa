// Benchmark: the classic nearest-neighbour heuristic for the TSP, as given in the brief.
//
// One tour per politician: start at their home base, keep moving to the closest unvisited town
// until every town has been visited, then go home. It knows nothing about population, the time
// budget or the other politicians. Each rally starts the moment they arrive, or the next morning
// if it wouldn't finish before the day's window closes. The validator then shows which of the
// problem's rules this breaks.

#include "planners.hpp"

using namespace std;

static vector<Visit> buildNearestNeighbourTour(const Instance& inst, int politician) {
    vector<Visit> tour;
    vector<bool> visited(inst.numTowns(), false);
    int currentPlace = inst.homeIndex(politician);
    double currentTime = 0;

    for (int step = 0; step < inst.numTowns(); ++step) {
        int nearest = -1;
        for (int town = 0; town < inst.numTowns(); ++town) {
            if (visited[town]) continue;
            if (nearest == -1 || inst.travelTime(currentPlace, town) < inst.travelTime(currentPlace, nearest)) {
                nearest = town;
            }
        }

        double arrive = currentTime + inst.travelTime(currentPlace, nearest);
        double start = inst.clock.fitRally(arrive, inst.rallyHours(nearest));
        double finish = start + inst.rallyHours(nearest);
        tour.push_back({nearest, arrive, start, finish});
        visited[nearest] = true;
        currentPlace = nearest;
        currentTime = finish;
    }
    return tour;
}

Solution solveNearestNeighbour(const Instance& inst) {
    Solution solution;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        solution.routes.push_back(buildNearestNeighbourTour(inst, p));
    }
    return solution;
}
