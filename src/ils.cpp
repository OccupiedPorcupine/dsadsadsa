// Approach B: Iterated Local Search, based on Vansteenwegen et al. (2009),
// "Iterated local search for the team orienteering problem with time windows".
//
// All politicians are planned together. Each round:
//   1. Insert:  keep adding the best (town, politician, position, timing) until nothing else fits.
//   2. Shake:   remove a few visits from every route, so the next round can try other choices.
// The best plan seen in any round is the answer.
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
//
// With --ils-alns (#15) each round is an adaptive large neighbourhood search step instead, after
// Ropke & Pisinger (2006): one of several destroy operators removes visits, one of several repair
// operators fills the routes up again, and operators that keep finding better plans get picked
// more often. B's shake and insertion are one destroy and one repair operator among the others.

#include "booking.hpp"
#include "planners.hpp"
#include "reach.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <stdexcept>
#include <unordered_set>

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
    void insertUntilFull(double noise = 0, mt19937* rng = nullptr);
    void shake(int firstPosition, int howMany);

    // The other ALNS operators (#15).
    void removeRandom(int howMany, mt19937& rng);
    void removeWorst(int howMany, mt19937& rng);
    void removeCluster(int howMany, mt19937& rng);
    void removeDay(mt19937& rng);
    void insertByRegret();

    double totalReach() const;
    int shortestRouteLength() const;
    int totalVisits() const;
    size_t planHash() const;
    Solution currentPlan() const;

private:
    void tryInsertion(int politician, int town, int position, Insertion& best, double noiseFactor = 1) const;
    void removeVisits(const vector<vector<bool>>& remove);
    vector<pair<int, int>> allVisits() const;  // (politician, position) of every planned visit
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
// The score is multiplied by `noiseFactor`, which is 1 except in ALNS's noisy repair.
void IlsPlanner::tryInsertion(int politician, int town, int position, Insertion& best, double noiseFactor) const {
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
        if (population * population / extraTime * noiseFactor <= best.score) continue;

        double gain = reachGain(inst, town, bookings.bookingsIn(town), {start, finish, politician});
        if (gain <= EPS) continue;
        double score = gain * gain / extraTime * noiseFactor;
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

// With `noise` > 0 (ALNS's noisy repair), each candidate's score is multiplied by a random
// factor between 1 - noise and 1 + noise, so the insertion isn't always the greedy one.
void IlsPlanner::insertUntilFull(double noise, mt19937* rng) {
    uniform_real_distribution<double> unit(-1.0, 1.0);
    while (true) {
        Insertion best;
        for (int p = 0; p < inst.numPoliticians; ++p) {
            for (int town = 0; town < inst.numTowns(); ++town) {
                if (inRoute[p][town]) continue;
                for (int position = 0; position <= (int)routes[p].size(); ++position) {
                    double noiseFactor = noise > 0 ? 1 + noise * unit(*rng) : 1;
                    tryInsertion(p, town, position, best, noiseFactor);
                }
            }
        }
        if (best.politician == -1) return;  // nothing else fits anywhere
        applyInsertion(best);
    }
}

// Regret-2 insertion (Ropke & Pisinger 2006). For each town, find its best insertion in each
// route that doesn't have it yet. The regret is how much worse the second-best route is than the
// best (the whole best score if no other route can take it). Insert the town with the biggest
// regret first, because it has the most to lose by waiting, then repeat until nothing fits.
// This stops the politicians all grabbing the same towns first.
void IlsPlanner::insertByRegret() {
    while (true) {
        Insertion chosen;
        double chosenRegret = -1;
        for (int town = 0; town < inst.numTowns(); ++town) {
            Insertion first, second;  // best and second best, each in a different route
            for (int p = 0; p < inst.numPoliticians; ++p) {
                if (inRoute[p][town]) continue;
                Insertion bestInRoute;
                for (int position = 0; position <= (int)routes[p].size(); ++position) {
                    tryInsertion(p, town, position, bestInRoute);
                }
                if (bestInRoute.politician == -1) continue;
                if (bestInRoute.score > first.score) {
                    second = first;
                    first = bestInRoute;
                } else if (bestInRoute.score > second.score) {
                    second = bestInRoute;
                }
            }
            if (first.politician == -1) continue;
            double regret = first.score - max(0.0, second.score);
            if (regret > chosenRegret) {
                chosen = first;
                chosenRegret = regret;
            }
        }
        if (chosen.politician == -1) return;  // nothing else fits anywhere
        applyInsertion(chosen);
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

// Removes the visits marked in `remove[politician][position]`, then re-times every route.
void IlsPlanner::removeVisits(const vector<vector<bool>>& remove) {
    for (int p = 0; p < inst.numPoliticians; ++p) {
        vector<PlannedVisit>& route = routes[p];
        vector<PlannedVisit> kept;
        for (int i = 0; i < (int)route.size(); ++i) {
            if (remove[p][i]) {
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

vector<pair<int, int>> IlsPlanner::allVisits() const {
    vector<pair<int, int>> visits;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        for (int i = 0; i < (int)routes[p].size(); ++i) {
            visits.push_back({p, i});
        }
    }
    return visits;
}

// Removes `howMany` visits in a row from every route, starting at `firstPosition`
// (wrapping round to the start of the route if needed).
void IlsPlanner::shake(int firstPosition, int howMany) {
    vector<vector<bool>> remove(inst.numPoliticians);
    for (int p = 0; p < inst.numPoliticians; ++p) {
        int length = routes[p].size();
        remove[p].assign(length, false);
        for (int i = 0; i < min(howMany, length); ++i) {
            remove[p][(firstPosition + i) % length] = true;
        }
    }
    removeVisits(remove);
}

// ALNS destroy: removes `howMany` visits picked at random from all the routes.
void IlsPlanner::removeRandom(int howMany, mt19937& rng) {
    vector<pair<int, int>> visits = allVisits();
    shuffle(visits.begin(), visits.end(), rng);

    vector<vector<bool>> remove(inst.numPoliticians);
    for (int p = 0; p < inst.numPoliticians; ++p) remove[p].assign(routes[p].size(), false);
    for (int i = 0; i < min(howMany, (int)visits.size()); ++i) {
        remove[visits[i].first][visits[i].second] = true;
    }
    removeVisits(remove);
}

// ALNS destroy: removes visits that reach the fewest people per hour they take up (the rally
// plus the detour to get there). As in Ropke & Pisinger, the pick is randomised towards the worst:
// a uniform number in [0, 1) is cubed, so it usually lands near the start of the sorted list.
void IlsPlanner::removeWorst(int howMany, mt19937& rng) {
    struct Candidate {
        double reachPerHour;
        int politician;
        int position;
    };
    vector<Candidate> candidates;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        const vector<PlannedVisit>& route = routes[p];
        int home = inst.homeIndex(p);
        for (int i = 0; i < (int)route.size(); ++i) {
            int town = route[i].town;
            int previousPlace = i == 0 ? home : route[i - 1].town;
            int nextPlace = i + 1 == (int)route.size() ? home : route[i + 1].town;
            double hoursUsed = inst.rallyHours(town) + inst.travelTime(previousPlace, town) +
                               inst.travelTime(town, nextPlace) - inst.travelTime(previousPlace, nextPlace);

            // This politician's rally in the town is the one with their number on it.
            const vector<Booking>& rallies = bookings.bookingsIn(town);
            vector<double> reaches = rallyReaches(inst, town, rallies);
            double reach = 0;
            for (int j = 0; j < (int)rallies.size(); ++j) {
                if (rallies[j].politician == p) reach = reaches[j];
            }
            candidates.push_back({reach / max(hoursUsed, EPS), p, i});
        }
    }
    sort(candidates.begin(), candidates.end(),
         [](const Candidate& a, const Candidate& b) { return a.reachPerHour < b.reachPerHour; });

    vector<vector<bool>> remove(inst.numPoliticians);
    for (int p = 0; p < inst.numPoliticians; ++p) remove[p].assign(routes[p].size(), false);
    uniform_real_distribution<double> unit(0.0, 1.0);
    for (int n = 0; n < howMany && !candidates.empty(); ++n) {
        int pick = (int)(pow(unit(rng), 3) * candidates.size());
        remove[candidates[pick].politician][candidates[pick].position] = true;
        candidates.erase(candidates.begin() + pick);
    }
    removeVisits(remove);
}

// ALNS destroy: picks a visited town at random and removes every visit (by anyone) to it and to
// the towns nearest it, closest first, until at least `howMany` visits are gone. This clears out
// a whole area, such as the Klang Valley, so it can be planned again from scratch.
void IlsPlanner::removeCluster(int howMany, mt19937& rng) {
    vector<pair<int, int>> visits = allVisits();
    if (visits.empty()) return;
    pair<int, int> seed = visits[uniform_int_distribution<int>(0, visits.size() - 1)(rng)];
    int seedTown = routes[seed.first][seed.second].town;

    vector<vector<pair<int, int>>> visitsByTown(inst.numTowns());
    for (const pair<int, int>& visit : visits) {
        visitsByTown[routes[visit.first][visit.second].town].push_back(visit);
    }
    vector<int> townsByDistance(inst.numTowns());
    for (int town = 0; town < inst.numTowns(); ++town) townsByDistance[town] = town;
    stable_sort(townsByDistance.begin(), townsByDistance.end(), [&](int a, int b) {
        return inst.travelTime(seedTown, a) < inst.travelTime(seedTown, b);
    });

    vector<vector<bool>> remove(inst.numPoliticians);
    for (int p = 0; p < inst.numPoliticians; ++p) remove[p].assign(routes[p].size(), false);
    int removed = 0;
    for (int town : townsByDistance) {
        if (removed >= howMany) break;
        for (const pair<int, int>& visit : visitsByTown[town]) {
            remove[visit.first][visit.second] = true;
            removed++;
        }
    }
    removeVisits(remove);
}

// ALNS destroy: picks a day (that of a random visit, so busy days come up more often) and removes
// every politician's rallies on it, so the whole day can be planned again together.
void IlsPlanner::removeDay(mt19937& rng) {
    vector<pair<int, int>> visits = allVisits();
    if (visits.empty()) return;
    pair<int, int> seed = visits[uniform_int_distribution<int>(0, visits.size() - 1)(rng)];
    int day = inst.clock.dayOf(routes[seed.first][seed.second].start);

    vector<vector<bool>> remove(inst.numPoliticians);
    for (int p = 0; p < inst.numPoliticians; ++p) {
        remove[p].assign(routes[p].size(), false);
        for (int i = 0; i < (int)routes[p].size(); ++i) {
            if (inst.clock.dayOf(routes[p][i].start) == day) remove[p][i] = true;
        }
    }
    removeVisits(remove);
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

int IlsPlanner::totalVisits() const {
    int total = 0;
    for (const vector<PlannedVisit>& route : routes) total += route.size();
    return total;
}

// A fingerprint of the plan (each route's towns and start times), so ALNS can tell whether a
// round produced a plan it has seen before.
size_t IlsPlanner::planHash() const {
    size_t hash = 0;
    auto mix = [&hash](size_t value) { hash = hash * 1000003 + value; };
    for (const vector<PlannedVisit>& route : routes) {
        mix(1000000007);  // marks where a new route starts
        for (const PlannedVisit& visit : route) {
            mix(visit.town);
            mix((size_t)llround(visit.start * 1000));
        }
    }
    return hash;
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
    planner.insertUntilFull();
    if (planner.totalReach() > bestReach + EPS) {
        bestPlan = planner.currentPlan();
        bestReach = planner.totalReach();
    }

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
        } else {
            roundsWithoutImprovement++;
        }

        removeFrom += removeCount;
        removeCount++;
        removeFrom %= max(1, planner.shortestRouteLength());
        if (removeCount > maxRemove) removeCount = 1;
    }

    stats.iterations += iterations;
    return bestPlan;
}

// ---- Adaptive large neighbourhood search (#15) ----

enum OperatorKind {
    REMOVE_RANDOM, REMOVE_WORST, REMOVE_CLUSTER, REMOVE_DAY, REMOVE_SHAKE,
    INSERT_GREEDY, INSERT_REGRET, INSERT_NOISY
};

struct Operator {
    OperatorKind kind;
    string name;
    double weight = 1;
    double segmentScore = 0;  // points earned since the weights were last updated
    int segmentUses = 0;
    int uses = 0;
    int newBests = 0;
};

// Points for a round that finds a new best plan, a plan better than the one before the round, or
// any other plan not seen before. Every SEGMENT_ROUNDS rounds each operator's weight moves
// REACTION of the way towards its average points per use. Ropke & Pisinger give 33, 9 and 13,
// but their search sometimes rejects a worse plan. Ours always carries on from the new plan, so
// nearly every round would earn the 13 and all the weights stayed level. With 1 they spread out,
// and reach went up a little (#15).
const double SCORE_NEW_BEST = 33;
const double SCORE_BETTER = 9;
const double SCORE_NEW_PLAN = 1;
const int SEGMENT_ROUNDS = 25;
const double REACTION = 0.2;
const double INSERT_NOISE = 0.2;  // the noisy repair scales each score by a random 0.8 to 1.2

// Roulette wheel: operator i is picked with probability weight(i) / total weight.
static Operator& pickOperator(vector<Operator>& operators, mt19937& rng) {
    double total = 0;
    for (const Operator& op : operators) total += op.weight;
    double spin = uniform_real_distribution<double>(0.0, total)(rng);
    for (Operator& op : operators) {
        spin -= op.weight;
        if (spin <= 0) return op;
    }
    return operators.back();
}

static void updateWeights(vector<Operator>& operators) {
    for (Operator& op : operators) {
        if (op.segmentUses > 0) {
            op.weight = (1 - REACTION) * op.weight + REACTION * op.segmentScore / op.segmentUses;
        }
        op.segmentScore = 0;
        op.segmentUses = 0;
    }
}

// The operators not switched off with --ils-alns-skip.
static vector<Operator> makeOperators(const vector<pair<OperatorKind, string>>& all, const IlsParams& params) {
    vector<Operator> operators;
    for (const auto& [kind, name] : all) {
        if (find(params.alnsSkip.begin(), params.alnsSkip.end(), name) == params.alnsSkip.end()) {
            operators.push_back({kind, name});
        }
    }
    if (operators.empty()) throw runtime_error("--ils-alns-skip left no destroy or no repair operator");
    return operators;
}

// Like search(), but each round picks its destroy and repair operators by weight. It keeps B's
// acceptance rule: always carry on from the new plan, and remember the best one seen.
static Solution searchAlns(const Instance& inst, IlsPlanner& planner, const IlsParams& params, unsigned seed,
                           double& bestReach, IlsStats& stats, vector<Operator>& destroyOps,
                           vector<Operator>& repairOps) {
    mt19937 rng(seed);

    Solution bestPlan = planner.currentPlan();
    bestReach = planner.totalReach();
    planner.insertUntilFull();
    if (planner.totalReach() > bestReach + EPS) {
        bestPlan = planner.currentPlan();
        bestReach = planner.totalReach();
    }
    double currentReach = planner.totalReach();
    unordered_set<size_t> seenPlans = {planner.planHash()};

    // B's shake keeps its own schedule, which only moves on when the shake is picked. The other
    // destroy operators remove a random number of visits: up to half as many as the shake's
    // largest, and at most an eighth of the plan. The shake's limit grows with the number of
    // towns, not with the plan, so at 253 towns it took out a third of the plan and did worse (#15).
    int maxRemove = max(1, inst.numTowns() / (3 * inst.numPoliticians));
    int removeFrom = 0;
    int removeCount = 1;
    int roundsWithoutImprovement = 0;
    int iterations = 0;

    while (iterations < params.maxIterations && roundsWithoutImprovement < params.maxNoImprove) {
        iterations++;
        Operator& destroy = pickOperator(destroyOps, rng);
        Operator& repair = pickOperator(repairOps, rng);
        int mostRemoved = min(maxRemove * inst.numPoliticians / 2, planner.totalVisits() / 8);
        int howMany = uniform_int_distribution<int>(1, max(1, mostRemoved))(rng);

        switch (destroy.kind) {
            case REMOVE_RANDOM: planner.removeRandom(howMany, rng); break;
            case REMOVE_WORST: planner.removeWorst(howMany, rng); break;
            case REMOVE_CLUSTER: planner.removeCluster(howMany, rng); break;
            case REMOVE_DAY: planner.removeDay(rng); break;
            default: planner.shake(removeFrom, removeCount); break;
        }
        switch (repair.kind) {
            case INSERT_REGRET: planner.insertByRegret(); break;
            case INSERT_NOISY: planner.insertUntilFull(INSERT_NOISE, &rng); break;
            default: planner.insertUntilFull(); break;
        }

        double reach = planner.totalReach();
        bool isNewPlan = seenPlans.insert(planner.planHash()).second;
        double score = 0;
        if (reach > bestReach + EPS) {
            bestPlan = planner.currentPlan();
            bestReach = reach;
            removeCount = 1;
            roundsWithoutImprovement = 0;
            stats.improvements++;
            destroy.newBests++;
            repair.newBests++;
            score = SCORE_NEW_BEST;
        } else {
            roundsWithoutImprovement++;
            if (isNewPlan) score = reach > currentReach + EPS ? SCORE_BETTER : SCORE_NEW_PLAN;
        }
        currentReach = reach;

        for (Operator* op : {&destroy, &repair}) {
            op->segmentScore += score;
            op->segmentUses++;
            op->uses++;
        }
        if (iterations % SEGMENT_ROUNDS == 0) {
            updateWeights(destroyOps);
            updateWeights(repairOps);
        }

        if (destroy.kind == REMOVE_SHAKE) {
            removeFrom += removeCount;
            removeCount++;
            removeFrom %= max(1, planner.shortestRouteLength());
            if (removeCount > maxRemove) removeCount = 1;
        }
    }

    stats.iterations += iterations;
    return bestPlan;
}

// Runs searchAlns from `planner` with a fresh set of operators, and adds how they did to `stats`.
static Solution runAlns(const Instance& inst, IlsPlanner& planner, const IlsParams& params, unsigned seed,
                        double& bestReach, IlsStats& stats) {
    vector<Operator> destroyOps = makeOperators({{REMOVE_RANDOM, "random"},
                                                 {REMOVE_WORST, "worst"},
                                                 {REMOVE_CLUSTER, "cluster"},
                                                 {REMOVE_DAY, "day"},
                                                 {REMOVE_SHAKE, "shake"}},
                                                params);
    vector<Operator> repairOps =
        makeOperators({{INSERT_GREEDY, "greedy"}, {INSERT_REGRET, "regret"}, {INSERT_NOISY, "noisy"}}, params);
    Solution plan = searchAlns(inst, planner, params, seed, bestReach, stats, destroyOps, repairOps);

    vector<Operator> operators = destroyOps;
    operators.insert(operators.end(), repairOps.begin(), repairOps.end());
    for (const Operator& op : operators) {
        auto known = find_if(stats.operators.begin(), stats.operators.end(),
                             [&](const OperatorStats& s) { return s.name == op.name; });
        if (known == stats.operators.end()) {
            stats.operators.push_back({op.name, 0, 0, 0});
            known = stats.operators.end() - 1;
        }
        known->weight += op.weight / 2;  // the average over the two runs
        known->uses += op.uses;
        known->newBests += op.newBests;
    }
    return plan;
}

Solution solveIls(const Instance& inst, const IlsParams& params, IlsStats* stats) {
    IlsStats counts;

    IlsPlanner fromEmpty(inst);
    Solution planFromEmpty = params.alns
                                 ? runAlns(inst, fromEmpty, params, params.alnsSeed, counts.reachFromEmpty, counts)
                                 : search(inst, fromEmpty, params, counts.reachFromEmpty, counts);

    // Starting from A's plan means B can never end up below A.
    IlsPlanner fromA(inst);
    fromA.load(solveSequential(inst));
    Solution planFromA = params.alns ? runAlns(inst, fromA, params, params.alnsSeed + 1, counts.reachFromA, counts)
                                     : search(inst, fromA, params, counts.reachFromA, counts);

    if (stats != nullptr) *stats = counts;
    return counts.reachFromA > counts.reachFromEmpty + EPS ? planFromA : planFromEmpty;
}
