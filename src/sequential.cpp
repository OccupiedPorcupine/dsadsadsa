#include "booking.hpp"
#include "planners.hpp"

namespace {

// Builds one politician's route by repeatedly appending the town with the best reach per hour,
// where the hour count includes travel, any wait for the town to be free, and the rally itself.
// Stops when no remaining town can be added with enough time left to get back to HQ.
std::vector<Visit> greedyRoute(const Instance& inst, int politician, const BookingTable& booked,
                               const std::vector<int>& priorVisits) {
    std::vector<Visit> route;
    std::vector<char> visited(inst.n(), 0);
    int cur = inst.depotIndex();
    double t = 0;
    while (true) {
        int bestTown = -1;
        double bestRatio = -1, bestArrive = 0, bestStart = 0;
        for (int c = 0; c < inst.n(); ++c) {
            if (visited[c]) continue;
            double arrive = t + inst.travelTime(cur, c);
            double start = booked.earliestFree(c, arrive, inst.stay(c), politician);
            double finish = start + inst.stay(c);
            if (finish + inst.travelTime(c, inst.depotIndex()) > inst.horizon + kEps) continue;
            double ratio = inst.gain(c, priorVisits[c]) / (finish - t);
            if (ratio > bestRatio) {
                bestRatio = ratio;
                bestTown = c;
                bestArrive = arrive;
                bestStart = start;
            }
        }
        if (bestTown < 0) break;
        double finish = bestStart + inst.stay(bestTown);
        route.push_back({bestTown, bestArrive, bestStart, finish});
        visited[bestTown] = 1;
        cur = bestTown;
        t = finish;
    }
    return route;
}

}  // namespace

Solution solveUncoordinated(const Instance& inst) {
    BookingTable nobodyElse(inst.n());
    std::vector<int> noPriorVisits(inst.n(), 0);
    Solution sol;
    for (int p = 0; p < inst.k; ++p) sol.routes.push_back(greedyRoute(inst, p, nobodyElse, noPriorVisits));
    return sol;
}

Solution solveSequential(const Instance& inst) {
    BookingTable booked(inst.n());
    std::vector<int> visits(inst.n(), 0);
    Solution sol;
    for (int p = 0; p < inst.k; ++p) {
        sol.routes.push_back(greedyRoute(inst, p, booked, visits));
        for (const Visit& v : sol.routes.back()) {
            booked.add(v.town, v.start, v.finish, p);
            ++visits[v.town];
        }
    }
    return sol;
}
