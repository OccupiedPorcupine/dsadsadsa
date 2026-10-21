// The baseline and Approach A. Both use the same greedy route builder; the difference is
// whether politicians can see each other's bookings.

#include "booking.hpp"
#include "planners.hpp"

using namespace std;

// Builds one politician's route. At each step it looks at every town not yet visited and picks
// the one with the most reach per hour spent (travel + waiting for the town to be free + the rally).
// It stops when no town can be added with enough time left to get back to HQ.
static vector<Visit> buildGreedyRoute(const Instance& inst, int politician, const BookingTable& bookings,
                                      const vector<int>& ralliesInTown) {
    vector<Visit> route;
    vector<bool> visited(inst.numTowns(), false);
    int currentPlace = inst.hqIndex();
    double currentTime = 0;

    while (true) {
        int bestTown = -1;
        double bestReachPerHour = -1;
        double bestArrive = 0;
        double bestStart = 0;

        for (int town = 0; town < inst.numTowns(); ++town) {
            if (visited[town]) continue;

            double arrive = currentTime + inst.travelTime(currentPlace, town);
            double start = bookings.earliestFreeTime(town, arrive, inst.rallyHours(town), politician);
            double finish = start + inst.rallyHours(town);
            double backAtHq = finish + inst.travelTime(town, inst.hqIndex());
            if (backAtHq > inst.campaignHours + EPS) continue;  // no time to get home afterwards

            double reachPerHour = inst.rallyReach(town, ralliesInTown[town]) / (finish - currentTime);
            if (reachPerHour > bestReachPerHour) {
                bestReachPerHour = reachPerHour;
                bestTown = town;
                bestArrive = arrive;
                bestStart = start;
            }
        }

        if (bestTown == -1) break;  // nothing else fits

        double finish = bestStart + inst.rallyHours(bestTown);
        route.push_back({bestTown, bestArrive, bestStart, finish});
        visited[bestTown] = true;
        currentPlace = bestTown;
        currentTime = finish;
    }
    return route;
}

Solution solveUncoordinated(const Instance& inst) {
    // Every politician gets an empty booking table and thinks no town has been visited yet.
    BookingTable emptyBookings(inst.numTowns());
    vector<int> noRallies(inst.numTowns(), 0);

    Solution solution;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        solution.routes.push_back(buildGreedyRoute(inst, p, emptyBookings, noRallies));
    }
    return solution;
}

Solution solveSequential(const Instance& inst) {
    // Shared between politicians: later ones see what earlier ones booked.
    BookingTable bookings(inst.numTowns());
    vector<int> ralliesInTown(inst.numTowns(), 0);

    Solution solution;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        vector<Visit> route = buildGreedyRoute(inst, p, bookings, ralliesInTown);
        for (const Visit& visit : route) {
            bookings.book(visit.town, visit.start, visit.finish, p);
            ralliesInTown[visit.town]++;
        }
        solution.routes.push_back(route);
    }
    return solution;
}
