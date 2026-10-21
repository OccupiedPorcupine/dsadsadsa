// The validator. It re-checks every plan independently, so a bug in a planner shows up
// here instead of quietly producing wrong numbers.

#include "solution.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace std;

// Allowed rounding error when checking times.
const double TOLERANCE = 1e-6;

struct Rally {
    double start;
    double finish;
    int politician;
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
    int currentPlace = inst.hqIndex();
    double currentTime = 0;

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

        alreadyVisited[visit.town] = true;
        metrics.travelHours += travel;
        metrics.waitHours += visit.start - earliestArrival;
        metrics.visits++;
        ralliesByTown[visit.town].push_back({visit.start, visit.finish, politician});

        currentPlace = visit.town;
        currentTime = visit.finish;
    }

    double tripHome = inst.travelTime(currentPlace, inst.hqIndex());
    metrics.travelHours += tripHome;
    metrics.returnTime[politician] = currentTime + tripHome;
    if (metrics.returnTime[politician] > inst.campaignHours + TOLERANCE) {
        char problem[100];
        snprintf(problem, sizeof problem, "returns to HQ at %.1f h, after the campaign ends (%.0f h)",
                 metrics.returnTime[politician], inst.campaignHours);
        metrics.errors.push_back(describeError(politician, -1, problem));
    }
}

Metrics evaluate(const Instance& inst, const Solution& solution) {
    int numPoliticians = solution.routes.size();
    Metrics metrics;
    metrics.reachByPolitician.assign(numPoliticians, 0.0);
    metrics.returnTime.assign(numPoliticians, 0.0);

    vector<vector<Rally>> ralliesByTown(inst.numTowns());
    for (int p = 0; p < numPoliticians; ++p) {
        checkRoute(inst, p, solution.routes[p], ralliesByTown, metrics);
    }

    // For each town, go through its rallies in time order: the first gets full reach, the next
    // gets alpha times that, and so on. Any two that overlap in time are a clash.
    for (int town = 0; town < inst.numTowns(); ++town) {
        vector<Rally>& rallies = ralliesByTown[town];
        sort(rallies.begin(), rallies.end(), [](const Rally& a, const Rally& b) { return a.start < b.start; });

        for (int i = 0; i < (int)rallies.size(); ++i) {
            double reach = inst.rallyReach(town, i);
            metrics.reach += reach;
            metrics.reachByPolitician[rallies[i].politician] += reach;

            for (int j = i + 1; j < (int)rallies.size(); ++j) {
                if (rallies[j].start < rallies[i].finish - TOLERANCE) metrics.clashes++;
            }
        }
    }
    return metrics;
}
