// The baseline, Approach A and its multi-start version. All use the same greedy route builder.
// The baseline differs from A in whether politicians can see each other's bookings; the
// multi-start version in the order politicians are planned and how strictly the best is taken.

#include "booking.hpp"
#include "planners.hpp"
#include "reach.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

using namespace std;

// One (town, timing) choice for the next rally, and its reach per hour.
struct Option {
    Visit visit;
    double reachPerHour;
};

// Picks which of the ranked options to take: the best with probability beta, the second best
// with beta (1 - beta), and so on (a geometric distribution). Past the end of the list it wraps
// round to the start. Without a random generator it always takes the best, as Approach A does.
struct Chooser {
    mt19937* rng = nullptr;
    double beta = 1;

    int pick(int numOptions) const {
        if (rng == nullptr || beta >= 1) return 0;
        double u = uniform_real_distribution<double>(0, 1)(*rng);
        double rank = floor(log(1 - u) / log(1 - beta));  // rank r with chance beta (1 - beta)^r
        return (int)fmod(rank, numOptions);
    }
};

// Builds one politician's route. At each step it looks at every town not yet visited and picks
// the one that adds the most reach per hour spent (travel + waiting + the rally). Under the soft
// clash rule each town has two possible timings, and both are scored:
//   - wait:  start once the town is free of other politicians' rallies;
//   - clash: start as soon as the window allows, overlapping someone, and pay the overlap penalty.
// "Go somewhere else" is simply another town winning. It stops when no town can be added with
// enough time left to get back home.
// With a random `chooser` it doesn't always take the best option, but one picked from the ranked
// list (see Chooser). That is how the multi-start version gets a different plan each time.
static vector<Visit> buildGreedyRoute(const Instance& inst, int politician, const BookingTable& bookings,
                                      const Chooser& chooser = Chooser()) {
    vector<Visit> route;
    vector<bool> visited(inst.numTowns(), false);
    int home = inst.homeIndex(politician);
    int currentPlace = home;
    double currentTime = 0;

    while (true) {
        int bestTown = -1;
        double bestReachPerHour = 0;
        Visit bestVisit{};
        vector<Option> options;  // only filled when choosing at random

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
                if (chooser.rng != nullptr) {
                    if (reachPerHour > 0) options.push_back({{town, arrive, start, finish, clash}, reachPerHour});
                } else if (reachPerHour > bestReachPerHour) {
                    bestReachPerHour = reachPerHour;
                    bestTown = town;
                    bestVisit = {town, arrive, start, finish, clash};
                }
            }
        }

        if (!options.empty()) {
            // Ranked best first. Ties keep the scan order (lower town, wait before clash), so
            // rank 0 is the option Approach A would take.
            auto better = [](const Option& x, const Option& y) {
                if (x.reachPerHour != y.reachPerHour) return x.reachPerHour > y.reachPerHour;
                if (x.visit.town != y.visit.town) return x.visit.town < y.visit.town;
                return !x.visit.clash && y.visit.clash;
            };
            int rank = chooser.pick(options.size());
            nth_element(options.begin(), options.begin() + rank, options.end(), better);
            bestVisit = options[rank].visit;
            bestTown = bestVisit.town;
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

// Biased-randomised multi-start A (Panadero et al. 2021). Each start shuffles the order the
// politicians are planned in and builds their routes with a random Chooser. The first start is
// plain Approach A, so the result is never below A. The plan with the most reach is kept.
Solution solveMultiStart(const Instance& inst, const MultiStartParams& params) {
    Solution bestPlan = solveSequential(inst);
    double bestReach = evaluate(inst, bestPlan).reach;

    mt19937 rng(params.seed);
    Chooser chooser{&rng, params.beta};
    vector<int> order(inst.numPoliticians);
    iota(order.begin(), order.end(), 0);

    for (int run = 1; run < params.starts; ++run) {
        shuffle(order.begin(), order.end(), rng);

        BookingTable bookings(inst.numTowns(), inst.clock);
        Solution plan;
        plan.routes.resize(inst.numPoliticians);
        for (int p : order) {
            plan.routes[p] = buildGreedyRoute(inst, p, bookings, chooser);
            for (const Visit& visit : plan.routes[p]) {
                bookings.book(visit.town, visit.start, visit.finish, p);
            }
        }

        // Exact reach, from the bookings every politician has made.
        double reach = 0;
        for (int town = 0; town < inst.numTowns(); ++town) {
            reach += townReach(inst, town, bookings.bookingsIn(town));
        }
        if (reach > bestReach + EPS) {
            bestPlan = plan;
            bestReach = reach;
        }
    }
    return bestPlan;
}
