# Design: Campaign routing with clash-free rallies

This document explains what the project solves and how: the problem, the data structures, the algorithms, their complexity, and how the results are checked. It follows the order the presentation needs (motivation → variant → redesign → complexity → trade-offs).

Related documents:
- [ASSUMPTIONS.md](ASSUMPTIONS.md): every modelling assumption, with reasons.
- [RESULTS.md](RESULTS.md): experiment results and how to reproduce them.
- [AI_USAGE.md](AI_USAGE.md): AI acknowledgement required by the brief.
- [papers/README.md](papers/README.md): reading list.

---

## 1. The use case

During a Malaysian general election, the official campaign period is short (about two weeks). A party's leader and deputy, and possibly more senior figures, all want to hold rallies in as many big towns as possible before polling day.

There is one hard rule: **two of them must never be holding a rally in the same town at the same time**. There are two reasons:
- **Security and continuity:** the party's top figures shouldn't all be in one place at once.
- **Coverage:** two headline speakers in one town at the same time wastes one of them.

### How we got to this problem
1. We started from the brief's classic TSP and looked for an assumption that breaks in real life.
2. First idea: two travellers on the same itinerary who never meet. This turned out to be trivial if they visit every city: one simply follows the other a step behind.
3. To make it non-trivial we added a **time limit**. With a time limit, not every town can be visited, so the goal became **maximising population reached**.
4. With a reach objective, a town that has been visited once is worth nothing the second time, so nobody would ever revisit and the clash rule would never matter. **Diminishing returns** fixed this: a second rally in a town still reaches a fraction `alpha` of the population, so politicians do compete for the big towns.

## 2. Why classic TSP doesn't fit

| Classic TSP assumes | Here |
|---|---|
| One traveller | k politicians whose plans affect each other |
| Only the visiting order matters, with no clock | The clash rule depends on *when* each politician is in each town |
| Any number of travellers can be in a city at once | Each town can host only one rally at a time |
| Every city must be visited | There isn't time, so towns are *chosen* |
| Minimise distance | Maximise population reached within a time budget |

The baseline shows this directly. Two politicians who each plan as if they were alone choose identical routes and clash in **every** town (45 out of 45 in the default run).

## 3. The problem we solve

**Input**
- n towns, each with population `pop(c)` and rally length `rally(c)` hours.
- Party HQ (Kuala Lumpur), where every route starts and ends.
- Travel time `travel(a, b)` hours between any two places (HQ included).
- k politicians, a time budget `T` hours, and a decay factor `alpha` in [0, 1].

**Output**
For each politician, a timed route: an ordered list of towns, each with arrive / start / finish times.

**Constraints**
1. Each route starts at HQ at time 0 and is back at HQ by time `T`.
2. Times are physically possible: `arrive ≥ previous finish + travel`, `start ≥ arrive`, `finish = start + rally(c)`.
3. A politician visits a town at most once.
4. **No clash:** for any two politicians' rallies in the same town, the intervals `[start, finish)` don't overlap.

**Objective**
Maximise total reach. In each town, the rallies are taken in time order: the first reaches `pop(c)`, the second `pop(c) × alpha`, the third `pop(c) × alpha²`, and so on.

Total reach for a town depends only on *how many* rallies it had, not on who came first.

**Name in the literature:** this is a **Team Orienteering Problem** (pick and route towns for a team within a time budget). It adds two things: **repeat visits with decaying reward** (as in the Travelling Politician Problem, which has one politician) and **mutual exclusion in time at each town** (an idea from multi-agent path finding). We did not find a paper that combines all three. See [papers/README.md](papers/README.md).

It is NP-hard (it contains the orienteering problem as the special case k = 1), so we use heuristics.

## 4. Data pipeline

Details and reasons are in [ASSUMPTIONS.md](ASSUMPTIONS.md) (D1–D5, T1–T4). In short, `buildInstance` in [src/instance.cpp](../src/instance.cpp) does the following:
1. Reads `MY.txt` and keeps populated places (`P`) with population ≥ 50,000, Peninsular Malaysia only by default.
2. Uses the national capital (`PPLC`, Kuala Lumpur) as HQ.
3. Merges places within 5 km of a larger one, because GeoNames lists districts inside cities separately.
4. Works out a rally length per town and a full travel-time matrix (driving, or flying between the peninsula and Borneo).

The default instance has 92 towns.

## 5. Data structures

### 5.1 Travel-time matrix (`Instance::travelHours`)
An `(n+1) × (n+1)` table of hours. HQ is the extra index `n`.
- **Why:** both planners ask "how long from a to b?" constantly, so an O(1) lookup matters.
- **Cost:** O(n²) memory, which is fine for n in the low hundreds (255 towns ≈ 0.5 MB).

### 5.2 `BookingTable`: when is each town taken?
[src/booking.hpp](../src/booking.hpp). For every town, a list of `Booking {start, end, politician}` sorted by start time.

| Operation | What it answers | Cost |
|---|---|---|
| `earliestFreeTime(town, t, duration, p)` | Earliest start ≥ t at which p's rally wouldn't overlap anyone else's | O(k) |
| `nextOtherBooking(town, t, p)` | When the next rally by someone else starts there | O(k) |
| `book`, `cancel`, `rebook` | Keep the list sorted | O(k) |

A town has at most k bookings, because each politician visits a town at most once, so a sorted vector is the simplest structure that is fast enough.

Alternatives considered:
- **Interval tree per town:** O(log k) queries, but k is 2 to 5, so it would be slower in practice and much more code.
- **Time grid** (town × hour → taken?): simple, but needs O(n × T) memory and forces times onto whole hours.
- **No table, check every other route each time:** O(total visits) per check instead of O(k).

### 5.3 `PlannedVisit.maxShift` (Approach B's key idea)
[src/ils.cpp](../src/ils.cpp). Each planned rally stores `{town, arrive, start, finish, maxShift}`. `maxShift` is how much later this rally could start without breaking anything further along the route.

It is computed backwards from the end of each route:

```
delayNextCanTake = T - (time back at HQ)                  # slack before the campaign ends
for each visit i from last to first:
    slackBeforeClash = (next other-politician booking in visit.town) - visit.finish
    visit.maxShift   = min(slackBeforeClash, delayNextCanTake)
    delayNextCanTake = visit.waitTime + visit.maxShift
```

**Why it is safe.** If visit i is pushed later by d ≤ maxShift(i), then:
- It doesn't run into the next rally by someone else in its town, because d ≤ slackBeforeClash.
- The next visit's arrival moves by d. Its start moves by max(0, d − wait), which is ≤ its own maxShift. By the same argument applied down the route (induction from the end), every later visit stays clash-free and the trip home still ends by T.

So to check whether inserting a town is allowed, we only compare the delay it causes against `wait + maxShift` of the next visit. That is **O(1)** instead of re-simulating the rest of the route in O(L).

This is the same idea as Vansteenwegen et al. (2009). Their slack only covers fixed opening hours. Ours also covers *other politicians' bookings*, which change as the plan changes, so after every insertion we recompute `maxShift` for all routes.

### 5.4 `Solution` and `Metrics`
- `Solution` is just the timed routes, which is what the planners output and the validator checks.
- `Metrics` holds reach, clashes, travel, wait, return times, fairness and any errors.

## 6. Algorithms

### 6.1 Baseline: uncoordinated greedy
Each politician runs the greedy builder (6.2) with an **empty** booking table, as if campaigning alone. Because the builder is deterministic, every politician picks the same route, which shows what goes wrong without coordination.

### 6.2 Approach A: plan one politician at a time (`solveSequential`)
[src/sequential.cpp](../src/sequential.cpp).

```
bookings = empty BookingTable;  ralliesInTown[c] = 0 for all towns
for politician p = 1..k:
    t = 0;  here = HQ;  route = []
    repeat:
        for each town c not yet in route:
            arrive = t + travel(here, c)
            start  = bookings.earliestFreeTime(c, arrive, rally(c), p)    # wait if taken
            finish = start + rally(c)
            skip c if finish + travel(c, HQ) > T                          # couldn't get home
            score(c) = pop(c) * alpha^ralliesInTown[c] / (finish - t)     # reach per hour, waiting included
        if no town fits: stop
        append the best town;  t = its finish;  here = it
    book all of p's rallies in the table;  update ralliesInTown
```

- **Redesign compared with Nearest Neighbour:** the choice is "most reach per hour", not "closest", and the hour count includes **waiting** caused by other politicians' bookings.
- **Weaknesses:** politician 1 picks first and gets the best towns. It only ever appends, so early choices are never revisited.

### 6.3 Approach B: iterated local search (`solveIls`)
[src/ils.cpp](../src/ils.cpp), based on Vansteenwegen, Souffriau, Vanden Berghe and Van Oudheusden (2009).

```
plan = empty routes for every politician
insertUntilFull(plan);  best = plan
removeFrom = 0;  removeCount = 1
while rounds < 5000 and roundsWithoutImprovement < 150:
    shake(plan, removeFrom, removeCount)       # drop removeCount visits in a row from every route
    insertUntilFull(plan)
    if reach(plan) > reach(best): best = plan;  removeCount = 1
    removeFrom += removeCount;  removeCount += 1
    removeFrom %= length of the shortest route
    if removeCount > n / (3k): removeCount = 1
return best

insertUntilFull(plan):
    repeat:
        for every politician p, town c not in p's route, position i in p's route:
            work out arrive/start/finish of c at position i (start = earliestFreeTime)
            delay = how much later the next stop (rally or HQ) is now reached
            if delay <= wait + maxShift of the next stop:                  # O(1) feasibility check
                score = reach(c)^2 / delay
        if nothing fits: stop
        insert the best (p, c, i);  push later rallies back;  update bookings
        recompute maxShift for all routes
```

- **Insertion anywhere, in any route:** unlike A, a town can go between two existing rallies, and any politician can take it, so nobody gets first pick.
- **`reach² / delay`:** the scoring rule from the paper. Squaring favours big towns.
- **Shake:** removing visits lets the next insertion round try other choices. After a shake, every route is re-timed from HQ. Rallies only ever move *earlier*, because each one's old start is still free, so re-timing cannot create a clash.
- **Weaknesses:** slower, and the objective has no fairness term. At alpha = 0 it can give every town to one politician (see [RESULTS.md](RESULTS.md)).

### 6.4 The two approaches side by side

| | A: one at a time | B: iterated local search |
|---|---|---|
| Who gets priority | Politician 1 first | Nobody; best move overall |
| Where a town can go | End of the route only | Any position, any route |
| Revisits earlier choices | Never | Yes, via shake + re-insert |
| Data structures | Booking table | Booking table + per-visit `maxShift` |
| Feasibility check | Simulate one step forward | O(1) comparison against `maxShift` |
| Literature origin | Prioritised planning (Silver 2005) | ILS for team orienteering (Vansteenwegen 2009) |

## 7. Complexity

Notation: n towns, k politicians, L = total rallies across all routes (L ≤ k·n), I = number of ILS rounds.

| Part | Time | Space |
|---|---|---|
| Read and filter `MY.txt` (N ≈ 71,709 lines) | O(N) | O(candidates) |
| Merge nearby places (c candidates, m kept) | O(c · m) | O(c) |
| Travel matrix | O(n²) | O(n²) |
| Booking table query or update | O(k) | O(L) in total |
| **Baseline** | Same loop as A, but every lookup is O(1) because the booking table is empty → **O(n · L)** | O(n) |
| **Approach A** | each step scans n towns at O(k) each; L steps in total → **O(k · n · L)**, at most O(k² n²) | O(n + L) |
| **Approach B**, one insertion | try n towns × (L + k) positions at O(k) each, then O(L · k) to update → **O(k · n · (L + k))** | O(n + L) |
| **Approach B**, one round | a shake (O(L · k)) plus a few insertions | |
| **Approach B**, total | O(I × insertions per round × k · n · L). Capped by `--ils-iters`; in practice it stops after 150 rounds without improvement | |
| Validator | O(L log L + L · k) | O(n + L) |

Measured (see [RESULTS.md](RESULTS.md)): A takes under 1 ms on every instance. B takes 0.5 ms at 15 towns, 125 ms at 92, and about 1 s at 255. Over that range B grows roughly like n^2.7, because larger instances also mean longer routes and more rounds.

## 8. How results are checked

[src/solution.cpp](../src/solution.cpp) re-checks every plan **independently of the planner that produced it**:
- **Timing:** each arrival is reachable from the previous finish, each start is after its arrival, and each rally has the right length.
- **Rules:** nobody visits a town twice, and everyone is back at HQ by T.
- **Clashes:** every pair of overlapping rallies in the same town is counted.
- **Reach:** recomputed from scratch.

A plan is reported as `valid` only if there are no errors and no clashes. Approaches A and B have been valid in every run so far: 81 stress-test settings across k, alpha, T and instance size, plus every run in [RESULTS.md](RESULTS.md). The baseline is invalid by design.

## 9. Trade-offs and limitations

- **A vs B:** A is more than 1,000× faster and simple to explain. B reaches more people in most settings, travels much less, and is usually fairer, but not always (see [RESULTS.md](RESULTS.md)).
- **Neither is optimal.** We don't yet know the gap to the true best plan. The next step is an exact solver for about 10 towns.
- **No daytime schedule** (assumption T6). Rallies can happen at any hour, so the clash rule almost never forces anyone to wait. This is the most important realism gap.
- **Straight-line travel × 1.3.** No real road network, traffic or terrain.
- **The objective ignores fairness.** B can leave a politician idle when alpha = 0.
- **Population ≠ voters**, and some duplicate towns survive merging.

## 10. Code map

| File | What's in it | Main functions |
|---|---|---|
| [src/instance.hpp/.cpp](../src/instance.cpp) | `Town`, `Config`, `Instance`; loading and preprocessing | `buildInstance`, `readCandidateTowns`, `mergeNearbyPlaces`, `travelHoursBetween` |
| [src/geo.hpp](../src/geo.hpp) | Great-circle distance | `haversineKm` |
| [src/booking.hpp/.cpp](../src/booking.cpp) | `BookingTable` | `earliestFreeTime`, `nextOtherBooking`, `book`, `cancel`, `rebook` |
| [src/planners.hpp](../src/planners.hpp) | Planner interface | `solveUncoordinated`, `solveSequential`, `solveIls` |
| [src/sequential.cpp](../src/sequential.cpp) | Baseline and Approach A | `buildGreedyRoute` |
| [src/ils.cpp](../src/ils.cpp) | Approach B | `IlsPlanner::insertUntilFull`, `tryInsertion`, `applyInsertion`, `updateMaxShifts`, `shake`, `recalculateTimes` |
| [src/solution.hpp/.cpp](../src/solution.cpp) | `Visit`, `Solution`, `Metrics`; the validator | `evaluate`, `checkRoute` |
| [src/main.cpp](../src/main.cpp) | Command line, results table, CSV export | `parseOptions`, `runPlanner`, `writeRoutesCsv`, `writeInstanceCsv` |
| [viz/](../viz/README.md) | Route map, separate from the solver | `make_map.py`, `map_template.html` |
