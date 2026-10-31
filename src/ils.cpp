// Approach B: Iterated Local Search, based on Vansteenwegen et al. (2009),
// "Iterated local search for the team orienteering problem with time windows".
//
// All politicians are planned together. Each round:
//   1. Insert:  keep adding the best (town, politician, position, timing) until nothing else fits.
//   2. Shake:   remove a few visits from every route, so the next round can try other choices.
// The best plan seen in any round is the answer.
//
// With --ils-sa, each round's new plan is kept if it reaches at least as many as the plan before
// the shake, or else with probability exp(change / T), as in SAILS (Gunawan et al. 2017). T falls a
// little every round, and the search goes back to the best plan after a while without improvement.
//
// The search runs twice: from empty routes, as in the paper, and from Approach A's plan. Neither
// start is always better (#7), so the better of the two plans is returned.
//
// What makes insertion fast is maxShift. For every planned visit we store how much later it
// could start without causing trouble further along the route (running into another politician's
// rally, running past the end of the day's window, or getting home too late). Checking whether
// an insertion fits is then one comparison instead of re-simulating the rest of the route.
//
// Soft clash rule: each insertion is tried with two timings, "wait" (start once the town is free)
// and "clash" (start as soon as the window allows and overlap someone). Clashes are only ever
// created on purpose, at the visit being inserted: maxShift never lets a later visit drift into one.

#include "booking.hpp"
#include "planners.hpp"
#include "reach.hpp"

#include <algorithm>
#include <cmath>
#include <random>

using namespace std;

struct PlannedVisit {
    int town;
    double arrive;
    double start;
    double finish;
    bool clash;       // timing mode: overlap others (true) or wait until the town is free (false)
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
    bool clash = false;
    double score = -1;
};

class IlsPlanner {
public:
    explicit IlsPlanner(const Instance& instance);

    void load(const Solution& plan);
    void insertUntilFull();
    void shake(int firstPosition, int howMany);

    double totalReach() const;
    int shortestRouteLength() const;
    int numVisits() const;
    void copyFrom(const IlsPlanner& other);
    Solution currentPlan() const;

private:
    void tryInsertion(int politician, int town, int position, Insertion& best) const;
    void applyInsertion(const Insertion& insertion);
    void recalculateTimes(int politician);
    void updateMaxShifts();
    double timeBackHome(int politician) const;

    const Instance& inst;
    vector<vector<PlannedVisit>> routes;
    BookingTable bookings;
    vector<vector<bool>> inRoute;  // inRoute[politician][town]
};

IlsPlanner::IlsPlanner(const Instance& instance)
    : inst(instance),
      routes(instance.numPoliticians),
      bookings(instance.numTowns(), instance.clock),
      inRoute(instance.numPoliticians, vector<bool>(instance.numTowns(), false)) {}

double IlsPlanner::timeBackHome(int politician) const {
    const vector<PlannedVisit>& route = routes[politician];
    if (route.empty()) return 0;
    const PlannedVisit& last = route.back();
    return last.finish + inst.travelTime(last.town, inst.homeIndex(politician));
}

// Goes backwards along each route. A visit can be delayed until either
//   - it would run into another politician's next rally in the same town,
//   - it would run past the end of that day's campaigning window, or
//   - the visit after it can't absorb the delay (its waiting time + its own maxShift).
// For the last visit, the limit is the time left before the campaign ends.
// A visit that already overlaps a rally that starts after it gets 0, so it never moves.
void IlsPlanner::updateMaxShifts() {
    for (int p = 0; p < inst.numPoliticians; ++p) {
        vector<PlannedVisit>& route = routes[p];
        double delayNextCanTake = inst.campaignHours() - timeBackHome(p);

        for (int i = (int)route.size() - 1; i >= 0; --i) {
            PlannedVisit& visit = route[i];
            double slackBeforeClash = bookings.nextOtherBooking(visit.town, visit.start, p) - visit.finish;
            double slackInDay = inst.clock.dayEnd(visit.start) - visit.finish;
            visit.maxShift = max(0.0, min({slackBeforeClash, slackInDay, delayNextCanTake}));
            delayNextCanTake = visit.waitTime() + visit.maxShift;
        }
    }
}

// Works out what happens if `town` is added to `politician`'s route just before `position`,
// with each of the two timings, and stores the move in `best` if it scores higher.
void IlsPlanner::tryInsertion(int politician, int town, int position, Insertion& best) const {
    const vector<PlannedVisit>& route = routes[politician];
    bool atStart = (position == 0);
    bool atEnd = (position == (int)route.size());
    int home = inst.homeIndex(politician);

    // Where the politician comes from.
    int previousPlace = atStart ? home : route[position - 1].town;
    double leaveTime = atStart ? 0.0 : route[position - 1].finish;

    // Where they go next (the next rally, or home), and how much delay that can take.
    int nextPlace = atEnd ? home : route[position].town;
    double oldArrivalAtNext = atEnd ? timeBackHome(politician) : route[position].arrive;
    double allowedDelay = atEnd ? inst.campaignHours() - timeBackHome(politician)
                                : route[position].waitTime() + route[position].maxShift;

    double length = inst.rallyHours(town);
    double population = inst.towns[town].population;
    double arrive = leaveTime + inst.travelTime(previousPlace, town);
    double waitStart = bookings.earliestFreeTime(town, arrive, length, politician);
    double clashStart = inst.clock.fitRally(arrive, length);

    for (bool clash : {false, true}) {
        double start = clash ? clashStart : waitStart;
        if (clash && clashStart >= waitStart - EPS) continue;  // nobody to clash with: same as waiting

        double finish = start + length;
        double delay = finish + inst.travelTime(town, nextPlace) - oldArrivalAtNext;
        if (delay > allowedDelay + EPS) continue;  // would cause a clash, an overrun or a late return later on

        // Vansteenwegen's score: reach squared, divided by the extra time used.
        // Squaring favours big towns over many small ones. A rally never reaches more than the
        // whole population, so skip the exact (slower) reach calculation if even that can't win.
        double extraTime = max(delay, EPS);
        if (population * population / extraTime <= best.score) continue;

        double gain = reachGain(inst, town, bookings.bookingsIn(town), {start, finish, politician});
        if (gain <= EPS) continue;
        double score = gain * gain / extraTime;
        if (score > best.score) {
            best = {politician, position, town, arrive, start, clash, score};
        }
    }
}

// Starts from an existing valid plan instead of empty routes. Its rallies keep their times and
// timing modes; only maxShift has to be worked out.
void IlsPlanner::load(const Solution& plan) {
    for (int p = 0; p < inst.numPoliticians; ++p) {
        for (const Visit& visit : plan.routes[p]) {
            routes[p].push_back({visit.town, visit.arrive, visit.start, visit.finish, visit.clash, 0});
            bookings.book(visit.town, visit.start, visit.finish, p);
            inRoute[p][visit.town] = true;
        }
    }
    updateMaxShifts();
}

void IlsPlanner::insertUntilFull() {
    while (true) {
        Insertion best;
        for (int p = 0; p < inst.numPoliticians; ++p) {
            for (int town = 0; town < inst.numTowns(); ++town) {
                if (inRoute[p][town]) continue;
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
    PlannedVisit newVisit = {insertion.town, insertion.arrive, insertion.start, finish, insertion.clash, 0};
    route.insert(route.begin() + insertion.position, newVisit);
    bookings.book(insertion.town, insertion.start, finish, p);
    inRoute[p][insertion.town] = true;

    // Push the later rallies back. The maxShift check in tryInsertion already made sure
    // none of them ends up clashing, running past the day's window, or getting home late.
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

// Re-simulates a route from home, starting every rally as early as its timing mode allows.
// After a shake this never moves a rally later, so the route still gets home in time:
//   - a "clash" rally starts as soon as the window allows, which is no later than before;
//   - a "wait" rally starts when the town is free. Its old start is normally still free, so this
//     is no later either. But another politician's clash rally may have moved into that slot. If
//     so, it keeps its old start and becomes a clash itself, rather than moving later.
void IlsPlanner::recalculateTimes(int politician) {
    int currentPlace = inst.homeIndex(politician);
    double currentTime = 0;
    for (PlannedVisit& visit : routes[politician]) {
        double length = inst.rallyHours(visit.town);
        double oldStart = visit.start;
        visit.arrive = currentTime + inst.travelTime(currentPlace, visit.town);
        if (visit.clash) {
            visit.start = inst.clock.fitRally(visit.arrive, length);
        } else {
            visit.start = bookings.earliestFreeTime(visit.town, visit.arrive, length, politician);
            if (visit.start > oldStart + EPS) {
                visit.start = oldStart;
                visit.clash = true;
            }
        }
        visit.finish = visit.start + length;
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
        total += townReach(inst, town, bookings.bookingsIn(town));
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

int IlsPlanner::numVisits() const {
    int total = 0;
    for (const vector<PlannedVisit>& route : routes) total += route.size();
    return total;
}

// Makes this planner hold the same plan as `other` (both are for the same instance).
void IlsPlanner::copyFrom(const IlsPlanner& other) {
    routes = other.routes;
    bookings = other.bookings;
    inRoute = other.inRoute;
}

Solution IlsPlanner::currentPlan() const {
    Solution plan;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        vector<Visit> visits;
        for (const PlannedVisit& v : routes[p]) {
            bool overlaps = bookings.overlapHours(v.town, v.start, v.finish, p) > EPS;
            visits.push_back({v.town, v.arrive, v.start, v.finish, overlaps});
        }
        plan.routes.push_back(visits);
    }
    return plan;
}

// One run of the search, from whatever `planner` holds now. Returns the best plan seen and sets
// `bestReach` to its reach. Adds its rounds and improvements to `stats`.
static Solution search(const Instance& inst, IlsPlanner& planner, const IlsParams& params,
                       double& bestReach, IlsStats& stats) {
    // The starting plan counts too. Filling it up can lower its reach, because insertions are
    // ranked on the inserted town alone, and a run from A's plan must never end below A.
    Solution bestPlan = planner.currentPlan();
    bestReach = planner.totalReach();
    IlsPlanner bestState = planner;  // only used with annealing, to go back to the best plan
    planner.insertUntilFull();
    if (planner.totalReach() > bestReach + EPS) {
        bestPlan = planner.currentPlan();
        bestReach = planner.totalReach();
        if (params.annealing) bestState.copyFrom(planner);
    }

    // Annealing: `current` is the plan the next shake starts from. The temperature starts at a
    // share of the average rally's reach, so it means the same at any size of instance.
    // Like instance.cpp, mt19937's raw output is turned into a number in [0, 1) by hand, so the
    // same seed gives the same run on every compiler.
    IlsPlanner current = planner;
    double currentReach = planner.totalReach();
    double temperature = params.startTemperature * currentReach / max(1, planner.numVisits());
    mt19937 random(params.seed);

    // Shake schedule from the paper: the removal point moves along the route and the number
    // removed grows by one each round. It goes back to 1 after an improvement, or once it
    // gets too big.
    int maxRemove = max(1, inst.numTowns() / (3 * inst.numPoliticians));
    int removeFrom = 0;
    int removeCount = 1;
    int roundsWithoutImprovement = 0;
    int iterations = 0;

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
            stats.improvements++;
            if (params.annealing) bestState.copyFrom(planner);
        } else {
            roundsWithoutImprovement++;
        }

        if (params.annealing) {
            double change = reach - currentReach;
            bool accept = change >= -EPS || random() / 4294967296.0 < exp(change / temperature);
            if (roundsWithoutImprovement > 0 && roundsWithoutImprovement % params.restartAfter == 0) {
                // Restart from the best plan found so far.
                current.copyFrom(bestState);
                currentReach = bestReach;
                planner.copyFrom(bestState);
            } else if (accept) {
                current.copyFrom(planner);
                currentReach = reach;
            } else {
                planner.copyFrom(current);
            }
            temperature *= params.cooling;
        }

        removeFrom += removeCount;
        removeCount++;
        removeFrom %= max(1, planner.shortestRouteLength());
        if (removeCount > maxRemove) removeCount = 1;
    }

    stats.iterations += iterations;
    return bestPlan;
}

Solution solveIls(const Instance& inst, const IlsParams& params, IlsStats* stats) {
    IlsStats counts;

    IlsPlanner fromEmpty(inst);
    Solution planFromEmpty = search(inst, fromEmpty, params, counts.reachFromEmpty, counts);

    // Starting from A's plan means B can never end up below A.
    IlsPlanner fromA(inst);
    fromA.load(solveSequential(inst));
    Solution planFromA = search(inst, fromA, params, counts.reachFromA, counts);

    if (stats != nullptr) *stats = counts;
    return counts.reachFromA > counts.reachFromEmpty + EPS ? planFromA : planFromEmpty;
}
