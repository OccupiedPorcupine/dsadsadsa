// Approach B: Iterated Local Search, based on Vansteenwegen et al. (2009),
// "Iterated local search for the team orienteering problem with time windows".
//
// All politicians are planned together. Each round:
//   1. Insert:  keep adding the best (town, politician, position) until nothing else fits.
//   2. Shake:   remove a few visits from every route, so the next round can try other choices.
// The best plan seen in any round is the answer.
//
// What makes insertion fast is maxShift. For every planned visit we store how much later it
// could start without causing trouble further along the route (a clash with another politician,
// or getting home too late). Checking whether an insertion fits is then one comparison instead
// of re-simulating the rest of the route.

#include "booking.hpp"
#include "planners.hpp"

#include <algorithm>

using namespace std;

struct PlannedVisit {
    int town;
    double arrive;
    double start;
    double finish;
    double maxShift;  // how much later this rally could start without breaking anything after it

    double waitTime() const { return start - arrive; }
};

// A possible move: put `town` into `politician`'s route, just before `position`.
struct Insertion {
    int politician = -1;  // -1 means "no insertion found"
    int position = 0;
    int town = 0;
    double arrive = 0;
    double start = 0;
    double score = -1;
};

class IlsPlanner {
public:
    explicit IlsPlanner(const Instance& instance);

    void insertUntilFull();
    void shake(int firstPosition, int howMany);

    double totalReach() const;
    int shortestRouteLength() const;
    Solution currentPlan() const;

private:
    void tryInsertion(int politician, int town, int position, Insertion& best) const;
    void applyInsertion(const Insertion& insertion);
    void recalculateTimes(int politician);
    void updateMaxShifts();
    double timeBackAtHq(int politician) const;

    const Instance& inst;
    vector<vector<PlannedVisit>> routes;
    BookingTable bookings;
    vector<int> ralliesInTown;
    vector<vector<bool>> inRoute;  // inRoute[politician][town]
};

IlsPlanner::IlsPlanner(const Instance& instance)
    : inst(instance),
      routes(instance.numPoliticians),
      bookings(instance.numTowns()),
      ralliesInTown(instance.numTowns(), 0),
      inRoute(instance.numPoliticians, vector<bool>(instance.numTowns(), false)) {}

double IlsPlanner::timeBackAtHq(int politician) const {
    const vector<PlannedVisit>& route = routes[politician];
    if (route.empty()) return 0;
    const PlannedVisit& last = route.back();
    return last.finish + inst.travelTime(last.town, inst.hqIndex());
}

// Goes backwards along each route. A visit can be delayed until either
//   - it would run into another politician's next rally in the same town, or
//   - the visit after it can't absorb the delay (its waiting time + its own maxShift).
// For the last visit, the limit is the time left before the campaign ends.
void IlsPlanner::updateMaxShifts() {
    for (int p = 0; p < inst.numPoliticians; ++p) {
        vector<PlannedVisit>& route = routes[p];
        double delayNextCanTake = inst.campaignHours - timeBackAtHq(p);

        for (int i = (int)route.size() - 1; i >= 0; --i) {
            PlannedVisit& visit = route[i];
            double slackBeforeClash = bookings.nextOtherBooking(visit.town, visit.start, p) - visit.finish;
            visit.maxShift = min(slackBeforeClash, delayNextCanTake);
            delayNextCanTake = visit.waitTime() + visit.maxShift;
        }
    }
}

// Works out what happens if `town` is added to `politician`'s route just before `position`,
// and stores it in `best` if it scores higher than what we have.
void IlsPlanner::tryInsertion(int politician, int town, int position, Insertion& best) const {
    const vector<PlannedVisit>& route = routes[politician];
    bool atStart = (position == 0);
    bool atEnd = (position == (int)route.size());

    // Where the politician comes from.
    int previousPlace = atStart ? inst.hqIndex() : route[position - 1].town;
    double leaveTime = atStart ? 0.0 : route[position - 1].finish;

    // The new rally.
    double arrive = leaveTime + inst.travelTime(previousPlace, town);
    double start = bookings.earliestFreeTime(town, arrive, inst.rallyHours(town), politician);
    double finish = start + inst.rallyHours(town);

    // Where they go next (the next rally, or home), and how much later they now get there.
    int nextPlace = atEnd ? inst.hqIndex() : route[position].town;
    double oldArrivalAtNext = atEnd ? timeBackAtHq(politician) : route[position].arrive;
    double delay = finish + inst.travelTime(town, nextPlace) - oldArrivalAtNext;

    double allowedDelay;
    if (atEnd) {
        allowedDelay = inst.campaignHours - timeBackAtHq(politician);
    } else {
        allowedDelay = route[position].waitTime() + route[position].maxShift;
    }
    if (delay > allowedDelay + EPS) return;  // would cause a clash or a late return later on

    // Vansteenwegen's score: reach squared, divided by the extra time used.
    // Squaring favours big towns over many small ones.
    double reach = inst.rallyReach(town, ralliesInTown[town]);
    double score = reach * reach / max(delay, EPS);
    if (score > best.score) {
        best = {politician, position, town, arrive, start, score};
    }
}

void IlsPlanner::insertUntilFull() {
    while (true) {
        Insertion best;
        for (int p = 0; p < inst.numPoliticians; ++p) {
            for (int town = 0; town < inst.numTowns(); ++town) {
                if (inRoute[p][town]) continue;
                if (inst.rallyReach(town, ralliesInTown[town]) <= 0) continue;  // nothing to gain (alpha = 0)

                for (int position = 0; position <= (int)routes[p].size(); ++position) {
                    tryInsertion(p, town, position, best);
                }
            }
        }
        if (best.politician == -1) return;  // nothing else fits anywhere
        applyInsertion(best);
    }
}

void IlsPlanner::applyInsertion(const Insertion& insertion) {
    int p = insertion.politician;
    vector<PlannedVisit>& route = routes[p];

    double finish = insertion.start + inst.rallyHours(insertion.town);
    PlannedVisit newVisit = {insertion.town, insertion.arrive, insertion.start, finish, 0};
    route.insert(route.begin() + insertion.position, newVisit);
    bookings.book(insertion.town, insertion.start, finish, p);
    ralliesInTown[insertion.town]++;
    inRoute[p][insertion.town] = true;

    // Push the later rallies back. The maxShift check in tryInsertion already made sure
    // none of them ends up clashing or getting home late.
    for (int i = insertion.position + 1; i < (int)route.size(); ++i) {
        PlannedVisit& visit = route[i];
        double newArrive = route[i - 1].finish + inst.travelTime(route[i - 1].town, visit.town);
        double newStart = max(newArrive, visit.start);
        visit.arrive = newArrive;

        // If the delay was soaked up by waiting time, nothing after this point moves.
        if (newStart == visit.start) break;

        visit.start = newStart;
        visit.finish = newStart + inst.rallyHours(visit.town);
        bookings.rebook(visit.town, p, visit.start, visit.finish);
    }
    updateMaxShifts();
}

// Re-simulates a route from HQ, starting every rally as early as possible.
// After a shake this only ever moves rallies earlier: each rally's old start time is still free,
// so earliestFreeTime can't return anything later than that.
void IlsPlanner::recalculateTimes(int politician) {
    int currentPlace = inst.hqIndex();
    double currentTime = 0;
    for (PlannedVisit& visit : routes[politician]) {
        visit.arrive = currentTime + inst.travelTime(currentPlace, visit.town);
        visit.start = bookings.earliestFreeTime(visit.town, visit.arrive, inst.rallyHours(visit.town), politician);
        visit.finish = visit.start + inst.rallyHours(visit.town);
        bookings.rebook(visit.town, politician, visit.start, visit.finish);
        currentPlace = visit.town;
        currentTime = visit.finish;
    }
}

// Removes `howMany` visits in a row from every route, starting at `firstPosition`
// (wrapping round to the start of the route if needed).
void IlsPlanner::shake(int firstPosition, int howMany) {
    for (int p = 0; p < inst.numPoliticians; ++p) {
        vector<PlannedVisit>& route = routes[p];
        int length = route.size();
        if (length == 0) continue;

        vector<bool> remove(length, false);
        for (int i = 0; i < min(howMany, length); ++i) {
            remove[(firstPosition + i) % length] = true;
        }

        vector<PlannedVisit> kept;
        for (int i = 0; i < length; ++i) {
            if (remove[i]) {
                bookings.cancel(route[i].town, p);
                ralliesInTown[route[i].town]--;
                inRoute[p][route[i].town] = false;
            } else {
                kept.push_back(route[i]);
            }
        }
        route = kept;
    }

    for (int p = 0; p < inst.numPoliticians; ++p) {
        recalculateTimes(p);
    }
    updateMaxShifts();
}

double IlsPlanner::totalReach() const {
    double total = 0;
    for (int town = 0; town < inst.numTowns(); ++town) {
        for (int rally = 0; rally < ralliesInTown[town]; ++rally) {
            total += inst.rallyReach(town, rally);
        }
    }
    return total;
}

int IlsPlanner::shortestRouteLength() const {
    int shortest = routes[0].size();
    for (const vector<PlannedVisit>& route : routes) {
        shortest = min(shortest, (int)route.size());
    }
    return shortest;
}

Solution IlsPlanner::currentPlan() const {
    Solution plan;
    for (const vector<PlannedVisit>& route : routes) {
        vector<Visit> visits;
        for (const PlannedVisit& v : route) {
            visits.push_back({v.town, v.arrive, v.start, v.finish});
        }
        plan.routes.push_back(visits);
    }
    return plan;
}

Solution solveIls(const Instance& inst, const IlsParams& params, IlsStats* stats) {
    IlsPlanner planner(inst);
    planner.insertUntilFull();
    Solution bestPlan = planner.currentPlan();
    double bestReach = planner.totalReach();

    // Shake schedule from the paper: the removal point moves along the route and the number
    // removed grows by one each round. It goes back to 1 after an improvement, or once it
    // gets too big.
    int maxRemove = max(1, inst.numTowns() / (3 * inst.numPoliticians));
    int removeFrom = 0;
    int removeCount = 1;
    int roundsWithoutImprovement = 0;
    int iterations = 0;
    int improvements = 0;

    while (iterations < params.maxIterations && roundsWithoutImprovement < params.maxNoImprove) {
        iterations++;
        planner.shake(removeFrom, removeCount);
        planner.insertUntilFull();

        double reach = planner.totalReach();
        if (reach > bestReach + EPS) {
            bestPlan = planner.currentPlan();
            bestReach = reach;
            removeCount = 1;
            roundsWithoutImprovement = 0;
            improvements++;
        } else {
            roundsWithoutImprovement++;
        }

        removeFrom += removeCount;
        removeCount++;
        removeFrom %= max(1, planner.shortestRouteLength());
        if (removeCount > maxRemove) removeCount = 1;
    }

    if (stats != nullptr) {
        stats->iterations = iterations;
        stats->improvements = improvements;
    }
    return bestPlan;
}
