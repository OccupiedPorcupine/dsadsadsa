// Benchmark: the classic nearest-neighbour heuristic for the TSP, as given in the brief.
//
// One tour: start at HQ, keep moving to the closest unvisited town until every town has been
// visited, then go back to HQ. It knows nothing about population, the time budget or the other
// politicians, so every politician is handed the same tour, and each rally starts the moment
// they arrive. The validator then shows which of the problem's rules this breaks.

#include "planners.hpp"

using namespace std;

static vector<Visit> buildNearestNeighbourTour(const Instance& inst) {
    vector<Visit> tour;
    vector<bool> visited(inst.numTowns(), false);
    int currentPlace = inst.hqIndex();
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
        double finish = arrive + inst.rallyHours(nearest);
        tour.push_back({nearest, arrive, arrive, finish});
        visited[nearest] = true;
        currentPlace = nearest;
        currentTime = finish;
    }
    return tour;
}

Solution solveNearestNeighbour(const Instance& inst) {
    vector<Visit> tour = buildNearestNeighbourTour(inst);

    Solution solution;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        solution.routes.push_back(tour);
    }
    return solution;
}
