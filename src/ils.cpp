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
//
// With --ils-vnd (#13), each round's insertions are followed by a local search, as in the
// variable neighbourhood descent of Shahmanzari et al. (2022): replace a rally with one in a new
// town, move a rally to another politician, swap rallies between two politicians, and swap two
// rallies in one route. A move is kept only if it raises the exact total reach.
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

// A gap in a route where a town could go: the stop before it and the stop after it (home at
// either end). Together with the town's bookings, this is everything a candidate's timing depends on.
struct Gap {
    int previousPlace = -1;  // -1: not worked out yet
    double leaveTime = 0;
    int nextPlace = -1;
    double arrivalAtNext = 0;

    bool operator==(const Gap& other) const {
        return previousPlace == other.previousPlace && leaveTime == other.leaveTime &&
               nextPlace == other.nextPlace && arrivalAtNext == other.arrivalAtNext;
    }
};

// --ils-cache: one candidate (politician, town, gap) with both timings, kept between insertion steps.
// Index 0 is "wait", 1 is "clash". It is worked out in two stages, each only when the pruning in
// scoreCachedTry gets that far: first the timings, then the reach gains.
struct CachedTry {
    bool timed = false;  // false: only `arrive` is known so far
    double arrive = 0;
    double start[2] = {0, 0};
    double delay[2] = {0, 0};  // infinity if that timing isn't tried
    double gain[2] = {0, 0};
    bool gainKnown[2] = {false, false};
};

// --ils-cache: what the scan needs to skip a candidate without looking at the rest of it.
struct QuickCheck {
    double minDelay = -INFINITY;   // no timing causes less delay; -infinity means "work it out again"
    double scoreBound = INFINITY;  // pop² / max(minDelay, EPS): no timing can score more than this
};

// --ils-cache: the cached candidates for one politician and one town, one per gap in the route.
struct TownTries {
    vector<QuickCheck> quick;
    vector<CachedTry> tries;
    double rowBound = INFINITY;  // no up-to-date gap has a higher scoreBound
};

class IlsPlanner {
public:
    IlsPlanner(const Instance& instance, const IlsParams& params);

    void load(const Solution& plan);
    int insertUntilFull(double noise = 0, mt19937* rng = nullptr);
    void shake(int firstPosition, int howMany, bool avoidDelays);
    int localSearch();

    // The other ALNS operators (#15).
    void removeRandom(int howMany, mt19937& rng);
    void removeWorst(int howMany, mt19937& rng);
    void removeCluster(int howMany, mt19937& rng);
    void removeDay(mt19937& rng);
    void insertByRegret();

    double totalReach() const;
    int shortestRouteLength() const;
    int numVisits() const;
    int totalVisits() const;
    size_t planHash() const;
    void copyFrom(const IlsPlanner& other);
    Solution currentPlan() const;

private:
    void tryInsertion(int politician, int town, int position, Insertion& best, double noiseFactor = 1) const;
    bool outsideGranularList(int previousPlace, int town, int nextPlace) const;
    int insertUntilFullCached();
    Gap gapAt(int politician, int position) const;
    double allowedDelayAt(int politician, int position) const;
    void scoreCachedTry(int politician, int town, int position, const Gap& gap, double allowedDelay,
                        Insertion& best);
    void keepRunsThatDelay(int politician, vector<bool>& remove) const;
    void removeVisits(const vector<vector<bool>>& remove);
    vector<pair<int, int>> allVisits() const;  // (politician, position) of every planned visit
    void applyInsertion(const Insertion& insertion);
    void recalculateTimes(int politician);
    void updateMaxShifts();
    double timeBackHome(int politician) const;

    // Local search (--ils-vnd). A move is made for real, scored, and undone if it doesn't help.
    bool replaceVisit(int p, int i);
    int tryReplacing();
    int tryMovingToOthers();
    int trySwappingBetweenRoutes();
    int trySwappingWithinRoutes();
    bool placeFits(int politician, int previous, int next, int town, bool clash, double& start) const;
    bool swapLooksBetter(int politician, int i, int j) const;
    void startMove(int politician, int otherPolitician = -1);
    void markChanged(int town);
    void removeVisit(int politician, int position);
    void addVisit(int politician, int position, int town, bool clash);
    bool retimeFrom(int politician, int first, int keptFrom);
    bool finishMove(bool fits);

    const Instance& inst;
    bool cacheScores;
    double granularity;
    vector<vector<PlannedVisit>> routes;
    BookingTable bookings;
    vector<vector<bool>> inRoute;  // inRoute[politician][town]
    vector<int> rebookedTowns;     // towns whose bookings the last insertion changed

    // --ils-cache only.
    vector<vector<TownTries>> cache;  // cache[politician][town]
    vector<vector<Gap>> cachedGaps;   // cachedGaps[politician][position]: the gap the cache was worked out for

    // Local search bookkeeping.
    vector<double> reachByTown;              // each town's reach in the current plan
    vector<int> changedTowns;                // towns whose rallies the move being tried changed
    vector<int> changedIn;                   // changedIn[town] == moveNumber if it is in changedTowns
    int moveNumber = 0;
    vector<int> movePoliticians;             // the routes the move changes,
    vector<vector<PlannedVisit>> savedRoutes;  // and how they were before it
};

IlsPlanner::IlsPlanner(const Instance& instance, const IlsParams& params)
    : inst(instance),
      cacheScores(params.cacheScores),
      granularity(params.granularity),
      routes(instance.numPoliticians),
      bookings(instance.numTowns(), instance.clock),
      inRoute(instance.numPoliticians, vector<bool>(instance.numTowns(), false)),
      reachByTown(instance.numTowns(), 0.0),
      changedIn(instance.numTowns(), -1),
      savedRoutes(instance.numPoliticians) {}

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

// Granular candidate lists (--ils-granular G, from Shahmanzari, Aksen & Salhi 2022, §4.7): a town is
// only tried between stops j and k if it is close to one of them, min(travel(j, town), travel(town, k))
// <= G × travel(j, k). A far-off town would make a long detour there, so is unlikely to be the best
// move. Gaps next to home are always tried: a home is often in a town of its own, so the first gap
// in a route can have length 0, and an empty route could never get started.
bool IlsPlanner::outsideGranularList(int previousPlace, int town, int nextPlace) const {
    if (granularity <= 0 || previousPlace >= inst.numTowns() || nextPlace >= inst.numTowns()) return false;
    double direct = inst.travelTime(previousPlace, nextPlace);
    return min(inst.travelTime(previousPlace, town), inst.travelTime(town, nextPlace)) > granularity * direct;
}

// The gap just before `position` in `politician`'s route, as tryInsertion sees it.
Gap IlsPlanner::gapAt(int politician, int position) const {
    const vector<PlannedVisit>& route = routes[politician];
    bool atStart = (position == 0);
    bool atEnd = (position == (int)route.size());
    int home = inst.homeIndex(politician);
    Gap gap;
    gap.previousPlace = atStart ? home : route[position - 1].town;
    gap.leaveTime = atStart ? 0.0 : route[position - 1].finish;
    gap.nextPlace = atEnd ? home : route[position].town;
    gap.arrivalAtNext = atEnd ? timeBackHome(politician) : route[position].arrive;
    return gap;
}

// How much later the next stop after the gap can be reached, as in tryInsertion.
double IlsPlanner::allowedDelayAt(int politician, int position) const {
    const vector<PlannedVisit>& route = routes[politician];
    if (position == (int)route.size()) return inst.campaignHours() - timeBackHome(politician);
    return route[position].waitTime() + route[position].maxShift;
}

// tryInsertion for --ils-cache. A candidate's timing and the delay it causes depend only on the gap
// and the town's bookings, so they are kept until one of those changes. maxShift changes after
// almost every insertion, in every route, but it only decides whether the delay is allowed, so that
// is checked fresh each time. The arithmetic and the order of the checks are the same as in
// tryInsertion, so the same move wins.
//
// A changed candidate first gets only the delay of starting as soon as the window allows. Waiting
// for the town to be free can only start later, so this is the least delay either timing can cause.
// Most candidates don't fit even then, and skip the booking-table lookup.
void IlsPlanner::scoreCachedTry(int politician, int town, int position, const Gap& gap, double allowedDelay,
                                Insertion& best) {
    TownTries& townTries = cache[politician][town];
    CachedTry& candidate = townTries.tries[position];
    QuickCheck& quick = townTries.quick[position];
    double length = inst.rallyHours(town);
    double population = inst.towns[town].population;

    if (quick.minDelay == -INFINITY) {
        candidate.gainKnown[0] = candidate.gainKnown[1] = false;
        if (outsideGranularList(gap.previousPlace, town, gap.nextPlace)) {
            candidate.timed = true;
            candidate.delay[0] = candidate.delay[1] = INFINITY;
            quick = {INFINITY, 0};
            return;
        }
        candidate.timed = false;
        candidate.arrive = gap.leaveTime + inst.travelTime(gap.previousPlace, town);
        double finish = inst.clock.fitRally(candidate.arrive, length) + length;
        quick.minDelay = finish + inst.travelTime(town, gap.nextPlace) - gap.arrivalAtNext;
        quick.scoreBound = population * population / max(quick.minDelay, EPS);
        if (quick.minDelay > allowedDelay + EPS || quick.scoreBound <= best.score) return;
    }

    if (!candidate.timed) {
        double waitStart = bookings.earliestFreeTime(town, candidate.arrive, length, politician);
        double clashStart = inst.clock.fitRally(candidate.arrive, length);
        for (bool clash : {false, true}) {
            double start = clash ? clashStart : waitStart;
            candidate.start[clash] = start;
            candidate.delay[clash] = INFINITY;
            if (clash && clashStart >= waitStart - EPS) continue;  // nobody to clash with: same as waiting
            double finish = start + length;
            candidate.delay[clash] = finish + inst.travelTime(town, gap.nextPlace) - gap.arrivalAtNext;
        }
        candidate.timed = true;
        quick.minDelay = min(candidate.delay[0], candidate.delay[1]);
        quick.scoreBound = population * population / max(quick.minDelay, EPS);
    }

    for (bool clash : {false, true}) {
        double delay = candidate.delay[clash];
        if (delay > allowedDelay + EPS) continue;
        double extraTime = max(delay, EPS);
        if (population * population / extraTime <= best.score) continue;

        double start = candidate.start[clash];
        if (!candidate.gainKnown[clash]) {
            candidate.gain[clash] = reachGain(inst, town, bookings.bookingsIn(town), {start, start + length, politician});
            candidate.gainKnown[clash] = true;
        }
        double gain = candidate.gain[clash];
        if (gain <= EPS) continue;
        double score = gain * gain / extraTime;
        if (score > best.score) {
            best = {politician, position, town, candidate.arrive, start, clash, score};
        }
    }
}

// insertUntilFull with --ils-cache. Each step still goes through the candidates in the same order,
// but skips most of them with two comparisons: the cached delay against the gap's allowed delay,
// and the cached score bound against the best so far. A candidate is worked out again only if its
// gap changed (the stops on either side moved, or the gap is new) or its town's bookings changed
// (the inserted town, and towns whose rallies were pushed later). Nothing else goes into its
// timing and gain, so they stay the same.
//
// rowBound lets a whole town be skipped: if no up-to-date gap there can beat the best so far, only
// the gaps that changed since the last step need a look.
int IlsPlanner::insertUntilFullCached() {
    int added = 0;
    // Shakes re-time every route, so start from scratch each time.
    cache.assign(inst.numPoliticians, vector<TownTries>(inst.numTowns()));
    cachedGaps.assign(inst.numPoliticians, {});
    for (int p = 0; p < inst.numPoliticians; ++p) {
        int gaps = routes[p].size() + 1;
        cachedGaps[p].assign(gaps, Gap());
        for (TownTries& townTries : cache[p]) {
            townTries.quick.assign(gaps, QuickCheck());
            townTries.tries.assign(gaps, CachedTry());
        }
    }

    vector<Gap> gaps;
    vector<double> allowedDelays;
    vector<int> changedGaps;
    while (true) {
        Insertion best;
        for (int p = 0; p < inst.numPoliticians; ++p) {
            int numGaps = routes[p].size() + 1;
            gaps.resize(numGaps);
            allowedDelays.resize(numGaps);
            changedGaps.clear();
            for (int position = 0; position < numGaps; ++position) {
                gaps[position] = gapAt(p, position);
                allowedDelays[position] = allowedDelayAt(p, position);
                if (gaps[position] == cachedGaps[p][position]) continue;
                cachedGaps[p][position] = gaps[position];
                changedGaps.push_back(position);
                for (TownTries& townTries : cache[p]) townTries.quick[position] = QuickCheck();
            }

            for (int town = 0; town < inst.numTowns(); ++town) {
                if (inRoute[p][town]) continue;
                TownTries& townTries = cache[p][town];
                if (townTries.rowBound <= best.score) {
                    // Only the changed gaps can beat the best so far.
                    for (int position : changedGaps) {
                        scoreCachedTry(p, town, position, gaps[position], allowedDelays[position], best);
                        townTries.rowBound = max(townTries.rowBound, townTries.quick[position].scoreBound);
                    }
                    continue;
                }

                double rowBound = 0;
                for (int position = 0; position < numGaps; ++position) {
                    const QuickCheck& quick = townTries.quick[position];
                    bool fits = quick.minDelay <= allowedDelays[position] + EPS;
                    if (fits && quick.scoreBound > best.score) {
                        scoreCachedTry(p, town, position, gaps[position], allowedDelays[position], best);
                    }
                    rowBound = max(rowBound, quick.scoreBound);
                }
                townTries.rowBound = rowBound;
            }
        }
        if (best.politician == -1) return added;  // nothing else fits anywhere
        applyInsertion(best);
        added++;

        // The gap the town went into is now two gaps. Add a column so the cache stays lined up
        // with the route; gaps after it keep their entries, and are worked out again if they moved.
        int p = best.politician;
        cachedGaps[p].insert(cachedGaps[p].begin() + best.position, Gap());
        for (TownTries& townTries : cache[p]) {
            townTries.quick.insert(townTries.quick.begin() + best.position, QuickCheck());
            townTries.tries.insert(townTries.tries.begin() + best.position, CachedTry());
        }
        for (int town : rebookedTowns) {
            for (int q = 0; q < inst.numPoliticians; ++q) {
                TownTries& townTries = cache[q][town];
                fill(townTries.quick.begin(), townTries.quick.end(), QuickCheck());
                townTries.rowBound = INFINITY;
            }
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

// Returns how many rallies were added. With `noise` > 0 (ALNS's noisy repair), each candidate's
// score is multiplied by a random factor between 1 - noise and 1 + noise, so the insertion isn't
// always the greedy one.
int IlsPlanner::insertUntilFull(double noise, mt19937* rng) {
    if (cacheScores && noise == 0) return insertUntilFullCached();
    uniform_real_distribution<double> unit(-1.0, 1.0);
    vector<Gap> gaps;
    int added = 0;
    while (true) {
        Insertion best;
        for (int p = 0; p < inst.numPoliticians; ++p) {
            if (granularity > 0) {
                gaps.resize(routes[p].size() + 1);
                for (int position = 0; position < (int)gaps.size(); ++position) gaps[position] = gapAt(p, position);
            }
            for (int town = 0; town < inst.numTowns(); ++town) {
                if (inRoute[p][town]) continue;
                for (int position = 0; position <= (int)routes[p].size(); ++position) {
                    if (granularity > 0 &&
                        outsideGranularList(gaps[position].previousPlace, town, gaps[position].nextPlace)) {
                        continue;
                    }
                    double noiseFactor = noise > 0 ? 1 + noise * unit(*rng) : 1;
                    tryInsertion(p, town, position, best, noiseFactor);
                }
            }
        }
        if (best.politician == -1) return added;  // nothing else fits anywhere
        applyInsertion(best);
        added++;
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
    rebookedTowns = {insertion.town};

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
        rebookedTowns.push_back(visit.town);
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

// Re-timing after a shake assumes that removing visits never makes the next stop later. That
// fails when travel times break the triangle inequality, as flights to and from Borneo do:
// Kuching -> a peninsula town -> Kota Kinabalu is two short flights, but Kuching -> Kota Kinabalu
// is a 15 h drive. This puts back any run of removed visits whose removal would make the next
// rally (or home) be reached after its old start (or after the campaign ends). It uses the old
// times, which is enough because the rally before the run finishes no later after re-timing.
void IlsPlanner::keepRunsThatDelay(int politician, vector<bool>& remove) const {
    const vector<PlannedVisit>& route = routes[politician];
    int length = route.size();
    int home = inst.homeIndex(politician);
    int previous = -1;  // the last kept visit, or -1 for home
    for (int i = 0; i <= length; ++i) {
        if (i < length && remove[i]) continue;
        if (previous + 1 < i) {
            int from = previous == -1 ? home : route[previous].town;
            double leave = previous == -1 ? 0.0 : route[previous].finish;
            int to = i == length ? home : route[i].town;
            double latest = i == length ? inst.campaignHours() : route[i].start;
            if (leave + inst.travelTime(from, to) > latest + EPS) {
                for (int j = previous + 1; j < i; ++j) remove[j] = false;
            }
        }
        previous = i;
    }
}

// Removes `howMany` visits in a row from every route, starting at `firstPosition`
// (wrapping round to the start of the route if needed). With `avoidDelays`, removals that
// would make a later stop late are skipped (see above). Only the annealing search uses this so
// far, so that B's default results stay exactly as they were (#14).
void IlsPlanner::shake(int firstPosition, int howMany, bool avoidDelays) {
    vector<vector<bool>> remove(inst.numPoliticians);
    for (int p = 0; p < inst.numPoliticians; ++p) {
        int length = routes[p].size();
        remove[p].assign(length, false);
        for (int i = 0; i < min(howMany, length); ++i) {
            remove[p][(firstPosition + i) % length] = true;
        }
        if (avoidDelays) keepRunsThatDelay(p, remove[p]);
    }
    removeVisits(remove);
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

int IlsPlanner::numVisits() const {
    int total = 0;
    for (const vector<PlannedVisit>& route : routes) total += route.size();
    return total;
}

int IlsPlanner::totalVisits() const { return numVisits(); }

// Makes this planner hold the same plan as `other` (both are for the same instance).
void IlsPlanner::copyFrom(const IlsPlanner& other) {
    routes = other.routes;
    bookings = other.bookings;
    inRoute = other.inRoute;
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

// ---------------------------------------------------------------------------------------------
// Local search (--ils-vnd, #13).
//
// Every move is made of two steps: take rallies out of routes, and put rallies in. Rallies that
// stay keep their start times unless they now arrive too late, so a move only changes the plan
// where it has to (taking a rally out leaves a gap that the next insertUntilFull can fill).
// maxShift is only used as a quick filter. A move that passes it is made for real, the changed
// routes are re-timed rally by rally, and the move is kept only if every politician still gets
// home in time and the exact reach of the towns it touched goes up. Otherwise it is undone.

// Smaller gains than this are rounding noise. Asking for at least this much stops the search
// going round in circles.
const double MIN_GAIN = 1.0;  // one voter

// Reach of `town` if `politician`'s rally there is taken out (-1: nobody's) and `added` is put in
// (nullptr: nothing).
static double reachIfChanged(const Instance& inst, const BookingTable& bookings, int town, int politician,
                             const Booking* added) {
    vector<Booking> rallies;
    for (const Booking& booking : bookings.bookingsIn(town)) {
        if (booking.politician != politician) rallies.push_back(booking);
    }
    if (added != nullptr) rallies.push_back(*added);
    return townReach(inst, town, rallies);
}

// Can a rally in `town` go between visits `previous` and `next` of `politician`'s route, with
// anything between them taken out? -1 and the route length stand for home. As in tryInsertion,
// only the next stop is checked, against its waiting time and maxShift. Sets `start` to when the
// rally would begin with the given timing.
bool IlsPlanner::placeFits(int politician, int previous, int next, int town, bool clash, double& start) const {
    const vector<PlannedVisit>& route = routes[politician];
    int home = inst.homeIndex(politician);
    bool atEnd = (next == (int)route.size());

    int previousPlace = previous < 0 ? home : route[previous].town;
    double leaveTime = previous < 0 ? 0.0 : route[previous].finish;
    int nextPlace = atEnd ? home : route[next].town;
    double oldArrivalAtNext = atEnd ? timeBackHome(politician) : route[next].arrive;
    double allowedDelay = atEnd ? inst.campaignHours() - timeBackHome(politician)
                                : route[next].waitTime() + route[next].maxShift;

    double length = inst.rallyHours(town);
    double arrive = leaveTime + inst.travelTime(previousPlace, town);
    double clashStart = inst.clock.fitRally(arrive, length);
    double delayIfNoWait = clashStart + length + inst.travelTime(town, nextPlace) - oldArrivalAtNext;
    if (delayIfNoWait > allowedDelay + EPS) return false;  // doesn't fit even without waiting for the town

    double waitStart = bookings.earliestFreeTime(town, arrive, length, politician);
    if (clash && clashStart >= waitStart - EPS) return false;  // nobody to clash with: same as waiting
    start = clash ? clashStart : waitStart;
    double delay = start + length + inst.travelTime(town, nextPlace) - oldArrivalAtNext;
    return delay <= allowedDelay + EPS;
}

void IlsPlanner::startMove(int politician, int otherPolitician) {
    moveNumber++;
    changedTowns.clear();
    movePoliticians = {politician};
    if (otherPolitician >= 0) movePoliticians.push_back(otherPolitician);
    for (int p : movePoliticians) savedRoutes[p] = routes[p];
}

void IlsPlanner::markChanged(int town) {
    if (changedIn[town] == moveNumber) return;
    changedIn[town] = moveNumber;
    changedTowns.push_back(town);
}

void IlsPlanner::removeVisit(int politician, int position) {
    int town = routes[politician][position].town;
    markChanged(town);
    bookings.cancel(town, politician);
    inRoute[politician][town] = false;
    routes[politician].erase(routes[politician].begin() + position);
}

// The new rally is booked when retimeFrom gives it a time. Its start of 0 means "as early as possible".
void IlsPlanner::addVisit(int politician, int position, int town, bool clash) {
    markChanged(town);
    inRoute[politician][town] = true;
    routes[politician].insert(routes[politician].begin() + position, {town, 0, 0, 0, clash, 0});
}

// Re-times `politician`'s route from visit `first` on. Each rally starts at its old start or, if it
// now arrives later than that, as early as its timing mode allows. From visit `keptFrom` on the
// rallies are ones the move didn't touch, so once one of them keeps its old start, nothing after it
// moves either. Returns false if the politician no longer gets home in time.
bool IlsPlanner::retimeFrom(int politician, int first, int keptFrom) {
    vector<PlannedVisit>& route = routes[politician];
    for (int i = first; i < (int)route.size(); ++i) {
        PlannedVisit& visit = route[i];
        int previousPlace = i == 0 ? inst.homeIndex(politician) : route[i - 1].town;
        double leaveTime = i == 0 ? 0.0 : route[i - 1].finish;
        double length = inst.rallyHours(visit.town);

        visit.arrive = leaveTime + inst.travelTime(previousPlace, visit.town);
        double earliest = max(visit.arrive, visit.start);
        double start = visit.clash ? inst.clock.fitRally(earliest, length)
                                   : bookings.earliestFreeTime(visit.town, earliest, length, politician);
        if (i >= keptFrom && start <= visit.start + EPS) return true;  // the rest of the route stays as it was

        visit.start = start;
        visit.finish = start + length;
        bookings.rebook(visit.town, politician, visit.start, visit.finish);
        markChanged(visit.town);
    }
    return timeBackHome(politician) <= inst.campaignHours() + EPS;
}

// Keeps the move if it fits and raises total reach; otherwise puts the changed routes and their
// bookings back as they were. Returns whether the move was kept.
bool IlsPlanner::finishMove(bool fits) {
    if (fits) {
        double gain = 0;
        vector<double> newReach;
        for (int town : changedTowns) {
            newReach.push_back(townReach(inst, town, bookings.bookingsIn(town)));
            gain += newReach.back() - reachByTown[town];
        }
        if (gain > MIN_GAIN) {
            for (int i = 0; i < (int)changedTowns.size(); ++i) reachByTown[changedTowns[i]] = newReach[i];
            updateMaxShifts();
            return true;
        }
    }

    // Only bookings in the changed towns can differ from before the move.
    for (int p : movePoliticians) {
        for (int town : changedTowns) bookings.cancel(town, p);
        for (const PlannedVisit& visit : routes[p]) inRoute[p][visit.town] = false;
        routes[p].swap(savedRoutes[p]);
        for (const PlannedVisit& visit : routes[p]) {
            inRoute[p][visit.town] = true;
            if (changedIn[visit.town] == moveNumber) bookings.book(visit.town, visit.start, visit.finish, p);
        }
    }
    return false;
}

// Move 1: replace visit i of a politician's route with a rally in a town they don't visit yet.
// Keeps the first replacement that raises total reach, and returns whether there was one.
bool IlsPlanner::replaceVisit(int p, int i) {
    int oldTown = routes[p][i].town;
    double loss = reachByTown[oldTown] - reachIfChanged(inst, bookings, oldTown, p, nullptr);

    for (int town = 0; town < inst.numTowns(); ++town) {
        if (inRoute[p][town]) continue;
        if (inst.towns[town].population <= loss + MIN_GAIN) continue;  // even a full crowd can't make up for it
        for (bool clash : {false, true}) {
            double start;
            if (!placeFits(p, i - 1, i + 1, town, clash, start)) continue;
            Booking rally = {start, start + inst.rallyHours(town), p};
            double gain = reachIfChanged(inst, bookings, town, -1, &rally) - reachByTown[town];
            if (gain <= loss + MIN_GAIN) continue;

            startMove(p);
            removeVisit(p, i);
            addVisit(p, i, town, clash);
            if (finishMove(retimeFrom(p, i, i + 1))) return true;
        }
    }
    return false;
}

// Each tryX below goes once through every place its kind of move could be made, keeps each move
// that raises total reach as it finds it, and returns how many it kept.

int IlsPlanner::tryReplacing() {
    int kept = 0;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        for (int i = 0; i < (int)routes[p].size(); ++i) {
            if (replaceVisit(p, i)) kept++;
        }
    }
    return kept;
}

// Move 2: give one of a politician's rallies to another politician, at any point in their route.
int IlsPlanner::tryMovingToOthers() {
    struct Place {
        int politician;
        int position;
        bool clash;
    };

    int kept = 0;
    for (int from = 0; from < inst.numPoliticians; ++from) {
        for (int j = 0; j < (int)routes[from].size(); ++j) {
            PlannedVisit visit = routes[from][j];
            int town = visit.town;
            double length = inst.rallyHours(town);

            // While looking for a new place, the town mustn't count as taken by the rally being moved.
            bookings.cancel(town, from);
            vector<Place> places;
            for (int p = 0; p < inst.numPoliticians; ++p) {
                if (p == from || inRoute[p][town]) continue;
                for (int i = 0; i <= (int)routes[p].size(); ++i) {
                    for (bool clash : {false, true}) {
                        double start;
                        if (!placeFits(p, i - 1, i, town, clash, start)) continue;
                        Booking rally = {start, start + length, p};
                        double gain = reachIfChanged(inst, bookings, town, -1, &rally) - reachByTown[town];
                        if (gain > MIN_GAIN) places.push_back({p, i, clash});
                    }
                }
            }
            bookings.book(town, visit.start, visit.finish, from);

            for (const Place& place : places) {
                startMove(from, place.politician);
                removeVisit(from, j);
                bool fits = retimeFrom(from, j, j);
                addVisit(place.politician, place.position, town, place.clash);
                fits = retimeFrom(place.politician, place.position, place.position + 1) && fits;
                if (finishMove(fits)) {
                    kept++;
                    j--;  // the next rally has moved up into position j
                    break;
                }
            }
        }
    }
    return kept;
}

// Move 3: swap a rally in one politician's route with a rally in another's.
int IlsPlanner::trySwappingBetweenRoutes() {
    int kept = 0;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        for (int q = p + 1; q < inst.numPoliticians; ++q) {
            for (int i = 0; i < (int)routes[p].size(); ++i) {
                for (int j = 0; j < (int)routes[q].size(); ++j) {
                    PlannedVisit visitP = routes[p][i];
                    PlannedVisit visitQ = routes[q][j];
                    int townP = visitP.town;
                    int townQ = visitQ.town;
                    if (inRoute[p][townQ] || inRoute[q][townP]) continue;  // also skips townP == townQ

                    // Check each side with the rally it replaces out of the booking table.
                    bookings.cancel(townP, p);
                    bookings.cancel(townQ, q);
                    double startP, startQ;
                    bool promising = placeFits(p, i - 1, i + 1, townQ, false, startP) &&
                                     placeFits(q, j - 1, j + 1, townP, false, startQ);
                    bookings.book(townP, visitP.start, visitP.finish, p);
                    bookings.book(townQ, visitQ.start, visitQ.finish, q);
                    if (!promising) continue;

                    Booking toP = {startP, startP + inst.rallyHours(townQ), p};
                    Booking toQ = {startQ, startQ + inst.rallyHours(townP), q};
                    double gain = reachIfChanged(inst, bookings, townP, p, &toQ) - reachByTown[townP] +
                                  reachIfChanged(inst, bookings, townQ, q, &toP) - reachByTown[townQ];
                    if (gain <= MIN_GAIN) continue;

                    startMove(p, q);
                    removeVisit(p, i);
                    addVisit(p, i, townQ, false);
                    removeVisit(q, j);
                    addVisit(q, j, townP, false);
                    bool fits = retimeFrom(p, i, i + 1);
                    fits = retimeFrom(q, j, j + 1) && fits;
                    if (finishMove(fits)) kept++;
                }
            }
        }
    }
    return kept;
}

// Works out, without changing anything, whether swapping visits i < j of `politician`'s route is
// worth trying. The rallies from i to j are re-timed as retimeFrom would, and the stop after them is
// checked against its waiting time and maxShift, as in tryInsertion. The swap must also raise the
// reach of the towns from i to j. Rallies further on that get pushed later are left out of that
// sum, so the move is still made and scored for real afterwards.
bool IlsPlanner::swapLooksBetter(int politician, int i, int j) const {
    const vector<PlannedVisit>& route = routes[politician];
    int home = inst.homeIndex(politician);
    int place = i == 0 ? home : route[i - 1].town;
    double time = i == 0 ? 0.0 : route[i - 1].finish;

    vector<double> starts;
    for (int m = i; m <= j; ++m) {
        const PlannedVisit& visit = route[m == i ? j : m == j ? i : m];
        double length = inst.rallyHours(visit.town);
        double arrive = time + inst.travelTime(place, visit.town);
        double earliest = (m == i || m == j) ? arrive : max(arrive, visit.start);
        double start = visit.clash ? inst.clock.fitRally(earliest, length)
                                   : bookings.earliestFreeTime(visit.town, earliest, length, politician);
        starts.push_back(start);
        place = visit.town;
        time = start + length;
    }

    bool atEnd = (j + 1 == (int)route.size());
    int nextPlace = atEnd ? home : route[j + 1].town;
    double oldArrivalAtNext = atEnd ? timeBackHome(politician) : route[j + 1].arrive;
    double allowedDelay = atEnd ? inst.campaignHours() - timeBackHome(politician)
                                : route[j + 1].waitTime() + route[j + 1].maxShift;
    if (time + inst.travelTime(place, nextPlace) - oldArrivalAtNext > allowedDelay + EPS) return false;

    double gain = 0;
    for (int m = i; m <= j; ++m) {
        const PlannedVisit& visit = route[m == i ? j : m == j ? i : m];
        double start = starts[m - i];
        if (start == visit.start) continue;  // same rally at the same time: its town's reach doesn't change
        Booking rally = {start, start + inst.rallyHours(visit.town), politician};
        gain += reachIfChanged(inst, bookings, visit.town, politician, &rally) - reachByTown[visit.town];
    }
    return gain > MIN_GAIN;
}

// Move 4: swap two rallies in one route. The polling-day weight makes a big town worth more later
// in the campaign, so only swaps that move the bigger town later are tried. A swap whose extra
// travel is more than all the waiting it could soak up (plus the slack after it) can't fit, and
// the rest are checked with swapLooksBetter before being made for real.
int IlsPlanner::trySwappingWithinRoutes() {
    int kept = 0;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        vector<PlannedVisit>& route = routes[p];
        int length = route.size();
        int home = inst.homeIndex(p);
        auto placeAt = [&](int i) { return i < 0 || i >= length ? home : route[i].town; };

        for (int i = 0; i < length; ++i) {
            for (int j = i + 1; j < length; ++j) {
                double waiting = 0;
                for (int m = i; m <= j; ++m) waiting += route[m].waitTime();
                int townI = route[i].town;
                int townJ = route[j].town;
                if (inst.towns[townI].population <= inst.towns[townJ].population) continue;

                double oldTravel, newTravel;
                if (j == i + 1) {
                    oldTravel = inst.travelTime(placeAt(i - 1), townI) + inst.travelTime(townI, townJ) +
                                inst.travelTime(townJ, placeAt(j + 1));
                    newTravel = inst.travelTime(placeAt(i - 1), townJ) + inst.travelTime(townJ, townI) +
                                inst.travelTime(townI, placeAt(j + 1));
                } else {
                    oldTravel = inst.travelTime(placeAt(i - 1), townI) + inst.travelTime(townI, placeAt(i + 1)) +
                                inst.travelTime(placeAt(j - 1), townJ) + inst.travelTime(townJ, placeAt(j + 1));
                    newTravel = inst.travelTime(placeAt(i - 1), townJ) + inst.travelTime(townJ, placeAt(i + 1)) +
                                inst.travelTime(placeAt(j - 1), townI) + inst.travelTime(townI, placeAt(j + 1));
                }
                double slackAfter = j + 1 < length ? route[j + 1].waitTime() + route[j + 1].maxShift
                                                   : inst.campaignHours() - timeBackHome(p);
                if (newTravel - oldTravel > waiting + slackAfter + EPS) continue;
                if (!swapLooksBetter(p, i, j)) continue;

                bool clashI = route[i].clash;
                bool clashJ = route[j].clash;
                startMove(p);
                removeVisit(p, j);
                removeVisit(p, i);
                addVisit(p, i, townJ, clashJ);
                addVisit(p, j, townI, clashI);
                if (finishMove(retimeFrom(p, i, j + 1))) kept++;
            }
        }
    }
    return kept;
}

// One pass of the variable neighbourhood descent: each kind of move in turn, keeping every move
// that raises total reach. Returns how many moves were kept.
//
// The full descent goes back to the first kind after every improvement until no move helps. It
// reached about 0.5% more, but at k = 10 it ran up to 21 times as long as B without it (#13);
// one pass stays under 5 times. fill() repeats the pass anyway whenever new rallies were inserted.
int IlsPlanner::localSearch() {
    for (int town = 0; town < inst.numTowns(); ++town) {
        reachByTown[town] = townReach(inst, town, bookings.bookingsIn(town));
    }
    int kept = tryReplacing();
    kept += tryMovingToOthers();
    kept += trySwappingBetweenRoutes();
    kept += trySwappingWithinRoutes();
    return kept;
}

// Fills the plan with insertions. With --ils-vnd, then runs the local search, and fills again
// whenever its moves made room, until neither changes anything.
static void fill(IlsPlanner& planner, const IlsParams& params, IlsStats& stats) {
    planner.insertUntilFull();
    if (!params.localSearch) return;
    while (true) {
        int moves = planner.localSearch();
        stats.localMoves += moves;
        if (moves == 0 || planner.insertUntilFull() == 0) return;
    }
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
    fill(planner, params, stats);
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
        planner.shake(removeFrom, removeCount, params.annealing);
        fill(planner, params, stats);

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
            default: planner.shake(removeFrom, removeCount, false); break;
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

    IlsPlanner fromEmpty(inst, params);
    Solution planFromEmpty = params.alns
                                 ? runAlns(inst, fromEmpty, params, params.alnsSeed, counts.reachFromEmpty, counts)
                                 : search(inst, fromEmpty, params, counts.reachFromEmpty, counts);

    // Starting from A's plan means B can never end up below A.
    IlsPlanner fromA(inst, params);
    fromA.load(solveSequential(inst));
    Solution planFromA = params.alns ? runAlns(inst, fromA, params, params.alnsSeed + 1, counts.reachFromA, counts)
                                     : search(inst, fromA, params, counts.reachFromA, counts);

    if (stats != nullptr) *stats = counts;
    return counts.reachFromA > counts.reachFromEmpty + EPS ? planFromA : planFromEmpty;
}
