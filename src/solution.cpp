// The validator. It re-checks every plan independently, so a bug in a planner shows up
// here instead of quietly producing wrong numbers.

#include "solution.hpp"

#include "reach.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace std;

// Allowed rounding error when checking times.
const double TOLERANCE = 1e-6;

// A rally as the validator sees it: the booking, plus where it sits in its politician's route.
struct Rally {
    Booking booking;
    int visitNumber;
};

static string describeError(int politician, int visitNumber, const string& problem) {
    return "politician " + to_string(politician) + " visit " + to_string(visitNumber) + ": " + problem;
}

double Metrics::fairness() const {
    if (reachByPolitician.empty()) return 1.0;
    double lowest = *min_element(reachByPolitician.begin(), reachByPolitician.end());
    double highest = *max_element(reachByPolitician.begin(), reachByPolitician.end());
    if (highest <= 0) return 1.0;
    return lowest / highest;
}

// Walks through one politician's route and checks that every time is physically possible.
// Also records each rally under its town so clashes can be found afterwards.
static void checkRoute(const Instance& inst, int politician, const vector<Visit>& route,
                       vector<vector<Rally>>& ralliesByTown, Metrics& metrics) {
    vector<bool> alreadyVisited(inst.numTowns(), false);
    int home = inst.homeIndex(politician);
    int currentPlace = home;
    double currentTime = 0;
    vector<int> lateRallies;  // visits that finish after the campaign ends (and so reach nobody)

    for (int i = 0; i < (int)route.size(); ++i) {
        const Visit& visit = route[i];
        double travel = inst.travelTime(currentPlace, visit.town);
        double earliestArrival = currentTime + travel;

        if (alreadyVisited[visit.town]) {
            metrics.errors.push_back(describeError(politician, i, "repeats town " + inst.towns[visit.town].name));
        }
        if (visit.arrive + TOLERANCE < earliestArrival) {
            metrics.errors.push_back(describeError(politician, i, "arrives before it could"));
        }
        if (visit.start + TOLERANCE < visit.arrive) {
            metrics.errors.push_back(describeError(politician, i, "starts before arriving"));
        }
        if (fabs(visit.finish - visit.start - inst.rallyHours(visit.town)) > TOLERANCE) {
            metrics.errors.push_back(describeError(politician, i, "rally length is wrong"));
        }
        if (visit.start < -TOLERANCE || visit.finish > inst.clock.dayEnd(visit.start) + TOLERANCE) {
            metrics.errors.push_back(describeError(politician, i, "rally runs outside the day's campaigning window"));
        }
        if (visit.finish > inst.campaignHours() + TOLERANCE) {
            lateRallies.push_back(i);
        }

        alreadyVisited[visit.town] = true;
        metrics.travelHours += travel;
        metrics.waitHours += visit.start - earliestArrival;
        metrics.visits++;
        ralliesByTown[visit.town].push_back({{visit.start, visit.finish, politician}, i});

        currentPlace = visit.town;
        currentTime = visit.finish;
    }

    // One error for all of them, so the benchmark's dozens of late rallies don't flood the output.
    // Routes run in time order, so the late ones are always the last visits.
    if (!lateRallies.empty()) {
        char problem[160];
        snprintf(problem, sizeof problem, "%d rallies run past the end of the campaign (visits %d to %d)",
                 (int)lateRallies.size(), lateRallies.front(), lateRallies.back());
        metrics.errors.push_back(describeError(politician, -1, problem));
    }

    double tripHome = inst.travelTime(currentPlace, home);
    metrics.travelHours += tripHome;
    metrics.returnTime[politician] = currentTime + tripHome;
    if (metrics.returnTime[politician] > inst.campaignHours() + TOLERANCE) {
        char problem[160];
        snprintf(problem, sizeof problem, "gets home to %s at %.1f h, after the campaign ends (%.0f h)",
                 inst.homes[politician].name.c_str(), metrics.returnTime[politician], inst.campaignHours());
        metrics.errors.push_back(describeError(politician, -1, problem));
    }
}

Metrics evaluate(const Instance& inst, const Solution& solution) {
    int numPoliticians = solution.routes.size();
    Metrics metrics;
    metrics.reachByPolitician.assign(numPoliticians, 0.0);
    metrics.returnTime.assign(numPoliticians, 0.0);
    metrics.visitReach.assign(numPoliticians, {});

    vector<vector<Rally>> ralliesByTown(inst.numTowns());
    for (int p = 0; p < numPoliticians; ++p) {
        checkRoute(inst, p, solution.routes[p], ralliesByTown, metrics);
    }

    // Reach is recomputed from scratch with the full model. Any two rallies in a town that
    // overlap in time are a clash.
    for (int p = 0; p < numPoliticians; ++p) metrics.visitReach[p].assign(solution.routes[p].size(), 0.0);
    for (int town = 0; town < inst.numTowns(); ++town) {
        const vector<Rally>& rallies = ralliesByTown[town];
        vector<Booking> bookings;
        for (const Rally& rally : rallies) bookings.push_back(rally.booking);

        vector<double> reaches = rallyReaches(inst, town, bookings);
        for (int i = 0; i < (int)rallies.size(); ++i) {
            int p = rallies[i].booking.politician;
            metrics.reach += reaches[i];
            metrics.reachByPolitician[p] += reaches[i];
            metrics.visitReach[p][rallies[i].visitNumber] = reaches[i];

            for (int j = i + 1; j < (int)rallies.size(); ++j) {
                const Booking& a = rallies[i].booking;
                const Booking& b = rallies[j].booking;
                double overlap = min(a.end, b.end) - max(a.start, b.start);
                if (overlap > TOLERANCE) {
                    metrics.clashes++;
                    metrics.clashHours += overlap;
                }
            }
        }
    }
    return metrics;
}
