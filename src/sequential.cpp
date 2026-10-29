// The baseline and Approach A. Both use the same greedy route builder; the difference is
// whether politicians can see each other's bookings.

#include "booking.hpp"
#include "planners.hpp"
#include "reach.hpp"

using namespace std;

// Builds one politician's route. At each step it looks at every town not yet visited and picks
// the one that adds the most reach per hour spent (travel + waiting + the rally). Under the soft
// clash rule each town has two possible timings, and both are scored:
//   - wait:  start once the town is free of other politicians' rallies;
//   - clash: start as soon as the window allows, overlapping someone, and pay the overlap penalty.
// "Go somewhere else" is simply another town winning. It stops when no town can be added with
// enough time left to get back home.
static vector<Visit> buildGreedyRoute(const Instance& inst, int politician, const BookingTable& bookings) {
    vector<Visit> route;
    vector<bool> visited(inst.numTowns(), false);
    int home = inst.homeIndex(politician);
    int currentPlace = home;
    double currentTime = 0;

    while (true) {
        int bestTown = -1;
        double bestReachPerHour = 0;
        Visit bestVisit{};

        for (int town = 0; town < inst.numTowns(); ++town) {
            if (visited[town]) continue;

            double length = inst.rallyHours(town);
            double arrive = currentTime + inst.travelTime(currentPlace, town);
            double waitStart = bookings.earliestFreeTime(town, arrive, length, politician);
            double clashStart = inst.clock.fitRally(arrive, length);

            for (bool clash : {false, true}) {
                double start = clash ? clashStart : waitStart;
                if (clash && clashStart >= waitStart - EPS) continue;  // nobody to clash with: same as waiting

                double finish = start + length;
                double backHome = finish + inst.travelTime(town, home);
                if (backHome > inst.campaignHours() + EPS) continue;  // no time to get home afterwards

                double gain = reachGain(inst, town, bookings.bookingsIn(town), {start, finish, politician});
                double reachPerHour = gain / (finish - currentTime);
                if (reachPerHour > bestReachPerHour) {
                    bestReachPerHour = reachPerHour;
                    bestTown = town;
                    bestVisit = {town, arrive, start, finish, clash};
                }
            }
        }

        if (bestTown == -1) break;  // nothing else fits, or nothing left adds any reach

        route.push_back(bestVisit);
        visited[bestTown] = true;
        currentPlace = bestTown;
        currentTime = bestVisit.finish;
    }
    return route;
}

Solution solveUncoordinated(const Instance& inst) {
    // Every politician gets an empty booking table and thinks no town has been visited yet.
    BookingTable emptyBookings(inst.numTowns(), inst.clock);

    Solution solution;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        solution.routes.push_back(buildGreedyRoute(inst, p, emptyBookings));
    }
    return solution;
}

Solution solveSequential(const Instance& inst) {
    // Shared between politicians: later ones see what earlier ones booked.
    BookingTable bookings(inst.numTowns(), inst.clock);

    Solution solution;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        vector<Visit> route = buildGreedyRoute(inst, p, bookings);
        for (const Visit& visit : route) {
            bookings.book(visit.town, visit.start, visit.finish, p);
        }
        solution.routes.push_back(route);
    }
    return solution;
}
