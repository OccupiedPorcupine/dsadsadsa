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
// With --ils-vnd (#13), each round's insertions are followed by a local search, as in the
// variable neighbourhood descent of Shahmanzari et al. (2022): replace a rally with one in a new
// town, move a rally to another politician, swap rallies between two politicians, and swap two
// rallies in one route. A move is kept only if it raises the exact total reach.

#include "booking.hpp"
#include "planners.hpp"
#include "reach.hpp"

#include <algorithm>
#include <cmath>

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
    int insertUntilFull();
    void shake(int firstPosition, int howMany);
    int localSearch();

    double totalReach() const;
    int shortestRouteLength() const;
    Solution currentPlan() const;

private:
    void tryInsertion(int politician, int town, int position, Insertion& best) const;
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
    vector<vector<PlannedVisit>> routes;
    BookingTable bookings;
    vector<vector<bool>> inRoute;  // inRoute[politician][town]

    // Local search bookkeeping.
    vector<double> reachByTown;              // each town's reach in the current plan
    vector<int> changedTowns;                // towns whose rallies the move being tried changed
    vector<int> changedIn;                   // changedIn[town] == moveNumber if it is in changedTowns
    int moveNumber = 0;
    vector<int> movePoliticians;             // the routes the move changes,
    vector<vector<PlannedVisit>> savedRoutes;  // and how they were before it
};

IlsPlanner::IlsPlanner(const Instance& instance)
    : inst(instance),
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

// Returns how many rallies were added.
int IlsPlanner::insertUntilFull() {
    int added = 0;
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
        if (best.politician == -1) return added;  // nothing else fits anywhere
        applyInsertion(best);
        added++;
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
    fill(planner, params, stats);
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
        fill(planner, params, stats);

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
