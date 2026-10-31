# Design: Campaign routing with soft-clash rallies

This document explains what the project solves and how: the problem, the data structures, the algorithms, their complexity, and how the results are checked. It follows the order the presentation needs (motivation → variant → redesign → complexity → trade-offs).

It describes **v2** of the model, including the fixes made since (GitHub issues #2–#9). What changed from v1, and why, is in [CHANGES_V2.md](CHANGES_V2.md) and summarised in [section 11](#11-v2-changes).

Related documents:
- [ASSUMPTIONS.md](ASSUMPTIONS.md): every modelling assumption, with reasons.
- [RESULTS.md](RESULTS.md): experiment results and how to reproduce them.
- [CHANGES_V2.md](CHANGES_V2.md): everything that changed from v1 to v2.
- [AI_USAGE.md](AI_USAGE.md): AI acknowledgement required by the brief.
- [papers/README.md](papers/README.md): reading list.

---

## 1. The use case

During a Malaysian general election, the official campaign period is short (about two weeks). A party's leader and deputy, and possibly more senior figures, all want to hold rallies in as many big towns as possible before polling day. Each campaigns from their own base (constituency or regional office), and each must be back there for polling day.

The politicians are **teammates, planned together** (ASSUMPTIONS S8). Under a time crunch the party can't send its whole team on a tour of every town, so it has to split its people up and decide centrally who goes where. We don't model rival parties: nobody would be trying to maximise the combined reach of both, so a single plan for everyone wouldn't make sense.

Campaigning only happens in the daytime: rallies and driving run from 06:00 to 22:00, and nights are spent wherever the day ended.

Two of them holding a rally **in the same town at the same time** is allowed, but it is costly:
- **Security and continuity:** the party's top figures shouldn't all be in one place at once.
- **Coverage:** two headline speakers in one town at the same time split the crowd.

So each time a politician heads for a town that is already busy, they have three choices:
- **Wait** until the other rally ends.
- **Clash:** rally now and accept a smaller crowd for both.
- **Go elsewhere.**

The planner has to pick whichever is worth most.

### How we got to this problem
1. We started from the brief's classic TSP and looked for an assumption that breaks in real life.
2. First idea: two travellers on the same itinerary who never meet. This turned out to be trivial if they visit every city: one simply follows the other a step behind.
3. To make it non-trivial we added a **time limit**. With a time limit, not every town can be visited, so the goal became **maximising population reached**.
4. With a reach objective, a town that has been visited once is worth nothing the second time, so nobody would ever revisit and the clash rule would never matter. **Diminishing returns** fixed this: a second rally in a town still reaches a fraction `alpha` of the population, so politicians do compete for the big towns.
5. **v2 made it realistic:**
   - **Daily window:** in v1 there was always a free slot at 3 a.m., so the clash rule never bit. The window takes those slots away.
   - **Separate home bases:** politicians start from, and return to, their own base.
   - **Time decay:** voters forget, so *when* a rally happens matters, not just whether it happens.
   - **Soft clash rule:** a clash becomes a cost to weigh rather than a ban.

## 2. Why classic TSP doesn't fit

| Classic TSP assumes | Here |
|---|---|
| One traveller | k politicians whose plans affect each other |
| Only the visiting order matters, with no clock | Clashes, the daily window and reach all depend on *when* each rally happens |
| Any number of travellers can be in a city at once | Rallies that overlap in one town both lose reach |
| A city's value is fixed | A rally is worth more near polling day and less soon after another rally in the same town |
| Travel can happen at any time | Only 16 hours a day are usable, and a rally can't run past 22:00 |
| Every city must be visited, starting and ending at one depot | There isn't time, so towns are *chosen*; each politician has their own start and end |
| Minimise distance | Maximise population reached within a time budget |

The baseline shows this directly. Two politicians who each plan as if they were alone pick largely the same towns at the same times. They clash 13 times in the default run and reach 18% less than A's coordinated plan, and 29% less than the budgeted nearest-neighbour benchmark, which picks towns by distance alone.

## 3. The problem we solve

**Input**
- n towns, each with population `pop(c)` and rally length `rally(c)` hours.
- k politicians, each with a home base `home(p)` where their route starts and ends.
- Travel time `travel(a, b)` hours between any two places (homes included).
- A campaign clock: D days of H campaigning hours (default 14 × 16, 06:00–22:00).
- Reach parameters:
  - `alpha` ∈ [0, 1]: repeat-rally discount.
  - `tauRepeat`: how fast that discount fades.
  - `tauPoll`: how fast early rallies are forgotten.
  - `beta` ∈ [0, 1]: clash penalty.

**The clock.** Times are in *active hours* `t ∈ [0, D·H]`, which count only campaigning time, so nights don't appear at all.
- Active hour t falls on day `floor(t / H)`.
- Travel can run across a night: a 5 h drive that starts at 18:00 is 2 h today and 3 h tomorrow morning.
- A rally must fit inside one day.

**Output**
For each politician, a timed route: an ordered list of towns, each with arrive / start / finish times.

**Constraints**
1. Politician p leaves `home(p)` at time 0 and is back there by `D·H`. They don't have to go home each night.
2. Times are physically possible: `arrive ≥ previous finish + travel`, `start ≥ arrive`, `finish = start + rally(c)`.
3. Every rally fits inside one day's window: `finish ≤ (day(start) + 1) · H`.
4. A politician visits a town at most once.

Clashes are **not** a constraint. They are allowed, and they cost reach.

**Objective**
Maximise total reach. A rally r in town c reaches

```
reach(r) = pop(c) × P(E(r)) × w(start_r) × share(r)

P(E)       = alpha^n × (1 − (1 − alpha) × (E − n)),  n = floor(E)                                 repeat penalty
E(r)       = Σ over rallies j that started earlier in c of
             (1 − overlap(r, j) / rally(c)) × exp(−(start_r − finish_j) / tauRepeat)             fading repeat count
w(s)       = exp(−(polling − s) / tauPoll)                                                       polling-day weight
share(r)   = average over r of (1 − beta) / (1 − beta + m × beta),  m = others in c then         crowd share
```

- **Fading repeat penalty (E).** Back-to-back rallies count as full repeats, so a second rally straight after the first reaches `alpha × pop`, as in v1. A rally days later is worth almost the full population again. `P` equals `alpha^E` at whole E and is a straight line in between. Plain `alpha^E` never fades at alpha = 0 (`0^E = 0` for any E > 0), so results jumped between alpha = 0 and any alpha > 0 (#5, ASSUMPTIONS V13).
- **Polling-day weight (w).** Voters partly forget early rallies. With the defaults (tauPoll = 336 h, polling = end of day 14), a day-1 rally counts for about 0.38 of a final-day one. A rally that runs past the end of the campaign is after polling day and reaches nobody (only the benchmark makes these).
- **Clash penalty.** A 10-minute overlap costs little. A full overlap with one other rally cuts both by `beta`. With m others at once, a rally keeps `(1 − beta) / (1 − beta + m × beta)` of its crowd while they overlap, so at the default beta = 0.5 any number of teammates speaking at once reach exactly one crowd between them (#2). Adding up the overlaps instead, as the first v2 rule did, let three at once reach 1.5 crowds. While two rallies overlap, the crowd is split rather than already reached, so the earlier one only counts towards the later one's repeat penalty for the part that doesn't overlap (ASSUMPTIONS V12). A full overlap pays only `beta`; a rally that starts just as the other ends pays the full repeat penalty, and everything in between is in proportion. On reach alone, a full overlap beats waiting when `beta < (1 − alpha) / 2`.
- **Time scale for decay.** Gaps in `E` and `w` are measured in **wall-clock hours** (nights included), because people forget overnight too. Everything else uses the active clock.

A town's total reach therefore depends on *when* its rallies happen, not only on how many there are.

**Name in the literature:** this is a **Team Orienteering Problem with Time Windows** (pick and route towns for a team within a time budget, with a daily window). It adds three things:
- **Time-dependent rewards that depreciate with recency**, as in the Travelling Politician Problem (Shahmanzari et al. 2022), which has only one politician.
- **Separate start and end depots** for each team member.
- **Soft mutual exclusion in time at each town**, an idea from multi-agent path finding, where a conflict is priced rather than forbidden.

We did not find a paper that combines these. See [papers/README.md](papers/README.md).

It is NP-hard (it contains the orienteering problem as the special case k = 1, alpha = 1, no decay, one long day), so we use heuristics.

## 4. Data pipeline

Details and reasons are in [ASSUMPTIONS.md](ASSUMPTIONS.md) (D1–D5, T1–T4, V1, V9). In short, `buildInstance` in [src/instance.cpp](../src/instance.cpp) does the following:
1. Reads `MY.txt` and keeps populated places (`P`) with population ≥ 50,000, Peninsular Malaysia only by default. It also keeps every populated place with a known population, so that home bases can be looked up.
2. Drops the places in `data/excluded_places.csv` (ASSUMPTIONS D6), which carry a district's population on top of the city they sit in, then merges places within 5 km of a larger one, because GeoNames lists districts inside cities separately.
3. Picks the home bases:
   - the fixed list (`--starts fixed`, the default);
   - k distinct random rally towns (`--starts random --seed N`);
   - k distinct rally towns drawn with probability proportional to population (`--starts weighted --seed N`);
   - or the names given on the command line.

   A name is matched to a rally town first, and otherwise to the most populous place in the file with that name.
4. Works out a rally length per town and a full travel-time matrix (driving, or flying between the peninsula and Borneo).

The default instance has 90 towns. The default homes are Kuala Lumpur and George Town.

## 5. Data structures

### 5.1 Travel-time matrix (`Instance::travelHours`)
An `(n + k) × (n + k)` table of hours. Politician p's home is the extra index `homeIndex(p) = n + p`.
- **Why:** both planners ask "how long from a to b?" constantly, so an O(1) lookup matters.
- **Cost:** O((n + k)²) memory, which is fine for n in the low hundreds (255 towns ≈ 0.5 MB).

### 5.2 `Clock`: the daily window
[src/instance.hpp](../src/instance.hpp). A tiny value type (`days`, `dayHours`, `dayStartHour`) with O(1) helpers:

| Function | What it answers |
|---|---|
| `dayOf(t)` | Which day an activity starting at t is on. A start exactly at the boundary counts as the next morning |
| `dayEnd(t)` | When that day's window closes |
| `fitRally(t, d)` | Earliest start ≥ t at which a d-hour rally fits inside one day (t itself, or next morning) |
| `wallHours(t)` | Real hours since the campaign opened, nights included (used for decay) |

**Why an active clock:** the timeline stays one number line, so intervals, bookings and `maxShift` work as before. The only non-linear step is a rally that jumps to the next morning, and that happens in one place, `fitRally`.

### 5.3 `BookingTable`: when is each town taken?
[src/booking.hpp](../src/booking.hpp). For every town, a list of `Booking {start, end, politician}` sorted by start time. Under the soft clash rule, bookings may overlap.

| Operation | What it answers | Cost |
|---|---|---|
| `earliestFreeTime(town, t, duration, p)` | Earliest start ≥ t, inside one day's window, at which p's rally overlaps nobody else's | O(k²) worst case, O(k) typically |
| `nextOtherBooking(town, t, p)` | When the next rally by someone else starts there | O(k) |
| `overlapHours(town, start, end, p)` | How much of [start, end) overlaps others' rallies | O(k) |
| `bookingsIn(town)` | The town's bookings, for the reach model | O(1) |
| `book`, `cancel`, `rebook` | Keep the list sorted | O(k) |

- **Why `earliestFreeTime` is O(k²).** Because bookings can overlap one another, the list is no longer sorted by end time, so one pass isn't enough. After waiting for one rally to end, a longer one that started earlier may still be running. The scan repeats until nothing overlaps. Each repeat moves the start past at least one booking for good, so there are at most k + 1 passes.
- **Why a sorted vector.** A town has at most k bookings, because each politician visits a town at most once, so a sorted vector is the simplest structure that is fast enough.

Alternatives considered:
- **Interval tree per town:** O(log k) queries, but k is 2 to 5, so it would be slower in practice and much more code.
- **Time grid** (town × hour → taken?): simple, but needs O(n × T) memory and forces times onto whole hours.
- **No table, check every other route each time:** O(total visits) per check instead of O(k).

### 5.4 The reach model (`reach.hpp`)
[src/reach.cpp](../src/reach.cpp). Three functions over one town's bookings:
- `rallyReaches(town, rallies)`: each rally's reach under the §3 formula. O(m³) for m rallies in the town (m ≤ k): for each rally, the repeat count is O(m), and the crowd share cuts the rally at up to 2m start and end times and counts who is present in each piece, O(m²).
- `townReach(town, rallies)`: their sum.
- `reachGain(town, rallies, added)`: how much the town's total changes if `added` joins. This is the new rally's own reach **minus** what it costs the others: overlap for anyone it clashes with, and a higher repeat count for any rally that comes after it.

The validator, Approach A and Approach B all call these functions, so they can't disagree about what a plan is worth.

### 5.5 `PlannedVisit.maxShift` (Approach B's key idea)
[src/ils.cpp](../src/ils.cpp). Each planned rally stores `{town, arrive, start, finish, mode, maxShift}`.
- `mode` is `wait` or `clash`: how the rally was timed.
- `maxShift` is how much later this rally could start without breaking anything further along the route.

It is computed backwards from the end of each route:

```
delayNextCanTake = D·H − (time back home)                  # slack before the campaign ends
for each visit i from last to first:
    slackBeforeClash = (next other-politician booking in visit.town) − visit.finish
    slackInDay       = dayEnd(visit.start) − visit.finish      # don't run past 22:00
    visit.maxShift   = max(0, min(slackBeforeClash, slackInDay, delayNextCanTake))
    delayNextCanTake = visit.waitTime + visit.maxShift
```

**Why it is safe.** If visit i is pushed later by d ≤ maxShift(i), then:
- It doesn't run into the next rally by someone else in its town, because d ≤ slackBeforeClash. **No shift ever creates a new clash.** Clashes only happen where a planner chose one on purpose.
- It still finishes before the window closes, because d ≤ slackInDay.
- The next visit's arrival moves by d. Travel can run across nights on the active clock, so that is a plain shift. The next visit's start moves by max(0, d − wait), which is ≤ its own maxShift. By the same argument applied down the route (induction from the end), every later visit stays valid and the trip home still ends by D·H.
- A visit that already overlaps a rally starting after it gets a negative `slackBeforeClash`, which is clamped to 0, so it never moves.

So to check whether inserting a town is allowed, we only compare the delay it causes against `wait + maxShift` of the next visit. That is **O(1)** instead of re-simulating the rest of the route in O(L).

This is the same idea as Vansteenwegen et al. (2009), whose time windows are fixed opening hours, like our daily window. Ours also covers *other politicians' bookings*, which change as the plan changes, so after every insertion we recompute `maxShift` for all routes.

### 5.6 `Solution` and `Metrics`
- `Solution` is just the timed routes (`Visit {town, arrive, start, finish, clash}`), which is what the planners output and the validator checks.
- `Metrics` holds:
  - total reach, reach per politician, and reach per rally;
  - clashes and clash hours;
  - travel and wait hours;
  - return times;
  - fairness;
  - any errors.

## 6. Algorithms

### 6.0 Benchmarks: classic nearest neighbour (`solveNearestNeighbour`, `solveBudgetedNearestNeighbour`)
[src/nearest.cpp](../src/nearest.cpp). The brief's nearest-neighbour TSP, unchanged except for respecting the daily window:

```
for each politician p:
    here = home(p);  t = 0;  tour = []
    repeat n times:
        c = the unvisited town closest to here (by travel time)
        arrive = t + travel(here, c);  start = fitRally(arrive, rally(c));  finish = start + rally(c)
        append c;  t = finish;  here = c
    return home
```

It visits every town and has no idea of population, the time budget or the other politicians. It is here to show the starting point the redesigns improve on.
- **Invalid:** the validator marks it invalid, because 22 of its 180 rallies run past the end of the campaign. It needs 260.5 active hours, 16.3 days of 16 h, to get everyone home, against 224.
- **Reach counts only the rallies before polling day.** The later ones reach nobody (ASSUMPTIONS V5).
- **How long it really takes:** the output prints the active hours the full tour needs to get everyone home, and the equivalent number of days.

**Budgeted version** (`nearest-budget`, #12). The same loop, with one extra test on each candidate:

```
        skip c if fitRally(arrive, rally(c)) + rally(c) + travel(c, home(p)) > D·H     # no time to get home after it
    stop when no unvisited town passes the test
```

It is the smallest change that makes the classic heuristic valid. It still ignores population and the other politicians, so it measures what picking towns by distance alone achieves within the budget, and it is the fair benchmark for A and B.

### 6.1 Baseline: uncoordinated greedy
Each politician runs the greedy builder (6.2) with an **empty** booking table, as if campaigning alone. It never sees the others, so it clashes freely. The validator then charges every clash at the beta penalty. The plan is valid, but it shows what the lack of coordination costs.

### 6.2 Approach A: plan one politician at a time (`solveSequential`)
[src/sequential.cpp](../src/sequential.cpp).

```
bookings = empty BookingTable
for politician p = 1..k:
    t = 0;  here = home(p);  route = []
    repeat:
        for each town c not yet in route:
            arrive    = t + travel(here, c)
            waitStart = bookings.earliestFreeTime(c, arrive, rally(c), p)     # wait for it to be free
            clashStart = fitRally(arrive, rally(c))                           # or start now and overlap
            for start in {waitStart, clashStart (only if earlier)}:
                finish = start + rally(c)
                skip if finish + travel(c, home(p)) > D·H                     # couldn't get home
                gain  = reachGain(c, bookings in c, {start, finish, p})       # includes cost to others
                score = gain / (finish − t)                                   # reach per hour, waiting included
        if no option has a positive score: stop                               # "go elsewhere" = another town wins
        append the best (town, timing);  t = its finish;  here = it
    book all of p's rallies in the table
```

- **Redesign compared with Nearest Neighbour:** the choice is "most reach gained per hour", not "closest".
  - The hour count includes **waiting**, both for another politician and for the next morning.
  - The gain includes the **clash penalty**, both p's own and what p would cost the other politician.
  - Wait, clash and go elsewhere are compared in the same score.
- **Weaknesses:** politician 1 picks first and gets the best towns. It only ever appends, so early choices are never revisited. It can't plan to save a big town for late in the campaign, where the polling-day weight is highest.

### 6.3 Approach B: iterated local search (`solveIls`)
[src/ils.cpp](../src/ils.cpp), based on Vansteenwegen, Souffriau, Vanden Berghe and Van Oudheusden (2009).

```
solveIls:
    best1 = search(empty routes for every politician)
    best2 = search(A's plan)
    return whichever reaches more

search(plan):
    best = plan;  insertUntilFull(plan)
    if reach(plan) > reach(best): best = plan
    removeFrom = 0;  removeCount = 1
    while rounds < 5000 and roundsWithoutImprovement < 150:
        shake(plan, removeFrom, removeCount)       # drop removeCount visits in a row from every route
        insertUntilFull(plan)
        if reach(plan) > reach(best): best = plan;  removeCount = 1    # exact reach, from reach.cpp
        removeFrom += removeCount;  removeCount += 1
        removeFrom %= length of the shortest route
        if removeCount > n / (3k): removeCount = 1
    return best

insertUntilFull(plan):
    repeat:
        for every politician p, town c not in p's route, position i in p's route:
            arrive = when p would reach c from stop i−1
            for start in {earliestFreeTime(c, arrive, ...), fitRally(arrive, ...) if earlier}:
                delay = how much later the next stop (rally or home) is now reached
                if delay <= wait + maxShift of the next stop:                  # O(1) feasibility check
                    if pop(c)^2 / delay <= best score so far: skip             # can't win, skip the exact reach
                    score = reachGain(c, ...)^2 / delay
        if nothing fits: stop
        insert the best (p, c, i, timing);  push later rallies back;  update bookings
        recompute maxShift for all routes
```

- **Insertion anywhere, in any route:** unlike A, a town can go between two existing rallies, and any politician can take it, so nobody gets first pick.
- **`gain² / delay`:** the scoring rule from the paper. Squaring favours big towns.
- **Pruning:** no rally reaches more than its town's population, so `pop² / delay` is an upper bound on a candidate's score. The exact `reachGain` (O(k³)) is only computed for candidates that could beat the best so far.
- **What the ranking leaves out:** pushing later rallies back changes their reach a little, through `w` and `E`. The ranking ignores this to keep each candidate cheap. Each round's total reach is always computed exactly, so the best plan is chosen correctly, but a move can be ranked slightly wrong. Counting the knock-on change exactly was tried (#7) and didn't help: B still lost to A in the same settings, did worse on the default run, and took about twice as long.
- **Two starts:** the paper starts from empty routes. On its own, that start lost to A in 4 of the 27 settings in RESULTS §2 and §5, by up to 6.1% (#7). Starting from A's plan never ends below A, but it is worse than the empty start in 10 of the same 27 settings (17.83M from empty vs 16.78M from A's plan at tauRepeat = 12 h, tauPoll = 168 h). B therefore runs the whole search from both and keeps the better plan. The starting plan itself is the first candidate for best, because filling it up can lower its reach (insertions are ranked on the inserted town alone). This doubles the runtime. `ILS:` in the output gives both runs' reach.
- **Shake and re-timing:** removing visits lets the next insertion round try other choices. After a shake, every route is re-timed from home:
  - a `clash` rally starts as early as the window allows;
  - a `wait` rally starts when the town is free.

  Neither can start later than before, because its old slot is still feasible. One exception: another politician's `clash` rally may have moved into a `wait` rally's old slot. That rally then keeps its old start and becomes a clash itself. Rallies therefore never move later, and every route still gets home in time.
- **Weaknesses:**
  - It is slower than A, and running it twice doubles that.
  - The objective has no fairness term; at k = 5 with a 6-day campaign, one politician can end up with under half of another's reach.
  - From empty routes, the search is weakest with k = 5 or with the polling-day weight off. With the weight off, B ends up only 0.6–5.1% above A, and at tauRepeat = 12 h that comes from the run seeded with A's plan (see [RESULTS.md](RESULTS.md) §2 and §5).

### 6.4 The two approaches side by side

| | A: one at a time | B: iterated local search |
|---|---|---|
| Who gets priority | Politician 1 first | Nobody; best move overall |
| Where a town can go | End of the route only | Any position, any route |
| Timing choices | Wait or clash | Wait or clash |
| Revisits earlier choices | Never | Yes, via shake + re-insert |
| Starting point | Empty routes | Two runs: empty routes, and A's plan |
| Data structures | Booking table, `Clock`, reach model | Booking table, `Clock`, reach model, per-visit `maxShift` |
| Feasibility check | Simulate one step forward | O(1) comparison against `maxShift` |
| Literature origin | Prioritised planning (Silver 2005) | ILS for team orienteering with time windows (Vansteenwegen 2009) |

### 6.5 Improving B: versions from the literature (#13–#17)

B above is the algorithm as published. Five ideas from the later literature were each tried as an experiment, and each is kept as its own approach so the results show what it adds (RESULTS §8). All of them except `ils` use the cache.

**Cached insertion scores (`ils-cache`, #16).** Profiling at 253 towns showed that B spends its time listing candidates, not scoring them: 471M `tryInsertion` calls, of which 5.5% fit the delay check and 0.14% reach `reachGain`. A candidate's timing, delay and gain depend only on its *gap* (the stop before, when it is left, the stop after, when it is reached) and on its town's bookings. So `insertUntilFull` keeps every candidate between insertion steps:
- After an insertion, a candidate is worked out again only if its gap changed or its town's bookings changed.
- Each step still compares the cached delay with the gap's current `wait + maxShift`, and skips candidates whose bound `pop² / delay` can't beat the best so far.
- The scan order and arithmetic are those of `tryInsertion`, so the same move wins, ties included: the plans are identical to B's (checked on all 117 benchmark runs and 36 stress settings), about 3× faster.

Granular candidate lists (only try a town between stops it is close to, Labadie et al. 2012; Shahmanzari et al. 2022 §4.7) were also tried in #16. They changed single runs by ±4% and added no reliable speed on top of the cache, so they are not used.

**Go back if worse (`ils-revert`, #14).** B carries on from the shaken plan whether it is better or worse. SAILS (Gunawan et al. 2017) adds simulated-annealing acceptance; at temperature 0 that is simply:

```
plan before = copy of plan
shake(plan);  fill(plan)
if reach(plan) < reach(plan before): plan = plan before
```

A random acceptance of worse plans (temperature > 0) added nothing measurable over temperature 0, which is also deterministic, so temperature 0 is used. This version's shake also keeps any run of visits whose removal would make a later stop late. That can happen only with Borneo flights, which break the triangle inequality (Kuching → a peninsula town → Kota Kinabalu is two short flights, but Kuching → Kota Kinabalu is a 15 h drive), and `recalculateTimes` assumes it never does.

**Local-search moves (`ils-moves`, #13).** After each `insertUntilFull`, one pass of four moves, as in the variable neighbourhood descent of Shahmanzari et al. (2022):

```
fill(plan):
    insertUntilFull(plan)
    repeat:
        kept = 0
        for each move type in [replace, move to another politician, swap between routes, swap within a route]:
            for each possible move of that type:
                if a quick maxShift-style check and a reach estimate say it may help:
                    make it for real; re-time the changed routes
                    keep it if everyone still gets home in time and the exact reach of the touched towns rises
                    otherwise undo it
                    kept += 1 if kept
        if kept == 0 or insertUntilFull(plan) adds nothing: stop
```

- **Replace:** swap a rally for one in a town that politician doesn't visit yet.
- **Move:** give a rally to another politician, at any position in their route.
- **Swap between routes:** exchange two rallies between two politicians.
- **Swap within a route:** exchange two rallies in one route, only when the bigger town moves later. This is where most of the gain comes from: the polling-day weight rewards big towns late, which insertion alone rarely achieves.

Going back to the first move type after every improvement (a full descent) added only about 0.5% more reach but took up to 21× as long at k = 9–10, so one pass per call is used.

**Final version (`ils-full`).** `ils-moves` and `ils-revert` together.

**Not adopted:**
- **ALNS (`ils-alns`, #15),** after Ropke & Pisinger (2006) and Hammami et al. (2020): destroy operators (random, worst reach per hour, a cluster of nearby towns, one day's rallies, B's shake) and repair operators (B's insertion, regret-2, noisy greedy), picked by adaptive weights. It is about even with B. The set-packing step of Hammami et al. was left out, because it assumes route values add up, and ours don't (politicians share towns and clash).
- **Multi-start A (`multistart`, #17),** after Panadero et al. (2021): A run 200 times with a shuffled politician order, taking the best option with probability 0.3, the next with 0.3 × 0.7, and so on, keeping the best plan. It beats A but stays well below B: A only appends, so randomising its choices can't move big towns later.

## 7. Complexity

Notation:
- n towns, k politicians;
- L = total rallies across all routes (L ≤ k·n);
- I = number of ILS rounds.

At most k rallies share a town, so one reach calculation for a town is O(k³) (§5.4). k is 2 to 5, so this is a small constant in practice.

| Part | Time | Space |
|---|---|---|
| Read and filter `MY.txt` (N ≈ 71,709 lines) | O(N) | O(candidates + populated places) |
| Merge nearby places (c candidates, m kept) | O(c · m) | O(c) |
| Choose homes | O(k · N_places) by name, O(n) random, O(k · n) weighted | O(n) |
| Travel matrix | O((n + k)²) | O((n + k)²) |
| `Clock` helpers | O(1) | O(1) |
| Booking table: `earliestFreeTime` | O(k²) worst case | O(L) in total |
| Booking table: other queries and updates | O(k) | |
| `reachGain` for one town | O(k³) | O(k) |
| **Benchmarks (nearest neighbour, full tour and budgeted)** | Up to n steps, each scanning up to n towns, for each of k politicians → **O(k · n²)** | O(k · n) |
| **Baseline** | Same loop as A, but the booking table is empty, so each lookup is O(1) → **O(n · L)** | O(n) |
| **Approach A** | Each step scans n towns × 2 timings at O(k³) each; L steps in total → **O(k³ · n · L)** | O(n + L) |
| **Approach B**, one insertion | Try n towns × (L + k) positions × 2 timings at O(k³) each, then O(L · k) to update → **O(k³ · n · (L + k))** worst case. Pruning skips most `reachGain` calls, so in practice it is close to O(k · n · L) | O(n + L) |
| **Approach B**, one round | A shake (O(L · k)), a few insertions, and an exact total reach (O(n · k³)) | |
| **Cached insertion (`ils-cache`)**, one insertion | Only candidates whose gap or town changed are worked out again: roughly the new route's gaps × n towns, plus the towns whose bookings moved. The skip check is O(1) per candidate. Same worst case as B, but at 253 towns `earliestFreeTime` calls fall from 471M to 264K | O(k · n · L) for the cache |
| **Local-search pass (`ils-moves`)** | Replace: L × n candidates; move and swap between routes: O(L²); swap within a route: O(L²/k). Each candidate that passes the quick check costs a re-time of up to two routes, O(L), plus exact reach for the touched towns, O(k³) | O(L) to undo a move |
| **Approach B**, total | 2 runs (from empty, from A's plan) × O(I × insertions per round × cost of one insertion), plus one run of A. Each run is capped by `--ils-iters`; in practice it stops after 150 rounds without improvement | |
| Validator | O(L + n · k³) | O(n + L) |

Measured (see [RESULTS.md](RESULTS.md) §7):
- **Both nearest-neighbour benchmarks** take under 0.1 ms.
- **A** takes about 0.7 ms at 90 towns and 1.8 ms at 253.
- **B** takes 4 ms at 15 towns, about 150 ms at 90, and about 2.6 s at 253.
- **Versions of B** (RESULTS §8): with the cache, B takes 55 ms at 90 towns and 0.5 s at 253. The final version, `ils-full`, takes about 0.7 s at 90 towns and 2.7 s at 253, and up to 15.5 s at k = 10.
- **v1 comparison:** B is about 1.2× slower than in v1 at 90 towns, and 2.5× slower at 253. That comes from the two timings per candidate, the full reach model, (since #7) running the search twice, and the longer routes that 16-hour days allow. Pruning keeps it far below the worst case.

## 8. How results are checked

[src/solution.cpp](../src/solution.cpp) re-checks every plan **independently of the planner that produced it**:
- **Timing:** each arrival is reachable from the previous finish, each start is after its arrival, and each rally has the right length.
- **Daily window:** every rally starts and finishes inside one day's window.
- **Campaign end:** no rally runs past the end of the campaign. Any that do are listed as errors, and they reach nobody (#4).
- **Rules:** nobody visits a town twice, and every politician is back at **their own** home by the end of the campaign.
- **Clashes:** every pair of overlapping rallies in the same town is counted, with its overlap in hours. They are reported but don't make a plan invalid.
- **Reach:** recomputed from scratch with `reach.cpp`, per rally, per politician, and in total. The per-rally figures are written to the routes CSV, so the map shows exactly what the validator computed.

A plan is reported as `valid` only if there are no errors.

Checks run on v2:
- **Stress test:** after the versions of B were added (§6.5), every plan from budgeted nearest neighbour, the baseline, A, multi-start A and all six versions of B was valid in 360 settings (3,960 plans): k ∈ {1, 2, 3, 5} × alpha ∈ {0, 0.5, 1} × beta ∈ {0, 0.5, 1} × {3, 14} days × {default, Borneo, 15 towns, 8 h days, random homes}. The same holds for every run in [RESULTS.md](RESULTS.md). The full-tour nearest-neighbour benchmark is invalid by design, except in the 36 settings with 15 towns over 14 days, where the whole tour fits.
- **Reach consistency:** B's internal running total of reach matched the validator's figure in all 27 settings of RESULTS §2 and §5 after the window was widened, and for every version of B at the defaults.
- **Cache:** `ils-cache` gives exactly the same plans as `ils` in all 117 benchmark runs (RESULTS §8), and the routes CSVs were byte-identical in 153 runs checked in #16. With 12-hour days it matched in all 144 settings checked: k ∈ {1, 2, 3, 5} × alpha ∈ {0, 0.5, 1} × beta ∈ {0, 0.5, 1} × {default, Borneo, random homes, 15 towns}.
- **Clash option:** with both politicians in KL and alpha = 0.5, A chooses to clash 64 times at beta = 0 and 7 times at beta = 0.25 (where clashing and waiting tie), and never above the threshold `(1 − alpha) / 2` (RESULTS §3).
- **Reach model:** the overlap rules (#2, #3) were checked against hand-worked examples, for instance any number of fully overlapping rallies reaching exactly one crowd at beta = 0.5. #2 was also compared against a build with the old rule over 72 settings: every k = 2 result was unchanged.
- **Options:** `--alpha` and `--clash-penalty` outside [0, 1], or an unknown `--approach`, are rejected (#6), because they would break the reach bound that B's pruning relies on.

## 9. Trade-offs and limitations

- **A vs B:** A is more than 100× faster and simple to explain. B reaches at least as many people as A in every setting, because one of its two runs starts from A's plan (#7). It is usually fairer, but not always (see [RESULTS.md](RESULTS.md)).
- **B vs its final version:** `ils-full` reaches 14–16% more than B and beats it in every benchmark run, but takes about 3–4× as long (up to 15.5 s at k = 10). `ils-moves` gets most of the gain (11–14%) for 1.3–1.8× B's time. In a few settings `ils-moves` beats `ils-full` by up to 1.1% (RESULTS §8).
- **Neither is optimal.** We don't yet know the gap to the true best plan. The next step is an exact solver for about 10 towns.
- **The soft clash rule only matters when beta is small.** Planners clash when `beta < (1 − alpha) / 2` (0.25 at the default alpha); at the default beta = 0.5 they always wait or go elsewhere (RESULTS §3).
- **A loses to a distance-only plan at the defaults.** Budgeted nearest neighbour reaches 15% more than A at the defaults and beats it in 16 of 27 settings, mostly 14-day campaigns (RESULTS §2, §5). A's reach-per-hour choice doesn't look ahead to the rallies that are worth more later, and it drives 1.8 times as far. With more politicians from random homes, A pulls ahead from k = 4 (RESULTS §4.1). B beats both everywhere.
- **B's search from empty routes can still lose to A** (§6.3), by up to 6.1% in 6-day campaigns and 1.9% when the polling-day weight is off. The run from A's plan covers this, but with the weight off B's gain over A is small (0.6–5.1%). Ranking on the exact knock-on reach was tried and was not the fix.
- **The decay parameters are guesses**, and they move reach more than the choice of algorithm does (RESULTS §5).
- **Straight-line travel × 1.3.** No real road network, traffic or terrain. Night stops along a drive are assumed to be possible anywhere.
- **The objective ignores fairness.** At k = 5, alpha = 0 and 6 days, fairness is 0.44 for A and 0.41 for B.
- **Population ≠ voters.** Five places that double-count a city are excluded by hand (ASSUMPTIONS D6); a few round-number figures (Bukit Rahman Putra 607,000) are kept on purpose, though dropping them would cut B's lead over the budgeted nearest-neighbour benchmark from 12% to 3% at the defaults.

## 10. Code map

| File | What's in it | Main functions |
|---|---|---|
| [src/instance.hpp/.cpp](../src/instance.cpp) | `Town`, `Clock`, `Config`, `Instance`; loading, preprocessing and home bases | `buildInstance`, `readCandidateTowns`, `mergeNearbyPlaces`, `chooseHomes`, `findPlace`, `randomHomes`, `weightedHomes`, `travelHoursBetween`, `Clock::fitRally` |
| [src/geo.hpp](../src/geo.hpp) | Great-circle distance | `haversineKm` |
| [src/booking.hpp/.cpp](../src/booking.cpp) | `BookingTable` | `earliestFreeTime`, `nextOtherBooking`, `overlapHours`, `book`, `cancel`, `rebook` |
| [src/reach.hpp/.cpp](../src/reach.cpp) | The reach model | `rallyReaches`, `townReach`, `reachGain` |
| [src/planners.hpp](../src/planners.hpp) | Planner interface | `solveNearestNeighbour`, `solveBudgetedNearestNeighbour`, `solveUncoordinated`, `solveSequential`, `solveMultiStart`, `solveIls` |
| [src/nearest.cpp](../src/nearest.cpp) | Classic nearest-neighbour benchmarks | `solveNearestNeighbour`, `solveBudgetedNearestNeighbour`, `buildNearestNeighbourTour` |
| [src/sequential.cpp](../src/sequential.cpp) | Baseline, Approach A, and the multi-start experiment (#17) | `buildGreedyRoute`, `Chooser`, `solveMultiStart` |
| [src/ils.cpp](../src/ils.cpp) | Approach B and its versions (§6.5) | `IlsPlanner::insertUntilFull`, `tryInsertion`, `applyInsertion`, `updateMaxShifts`, `shake`, `recalculateTimes`; cache: `insertUntilFullCached`, `scoreCachedTry`; go back if worse: `search`, `keepRunsThatDelay`; moves: `localSearch`, `tryReplacing`, `tryMovingToOthers`, `trySwappingBetweenRoutes`, `trySwappingWithinRoutes`; ALNS: `runAlns`, `removeRandom`, `removeWorst`, `removeCluster`, `removeDay`, `insertByRegret` |
| [src/solution.hpp/.cpp](../src/solution.cpp) | `Visit`, `Solution`, `Metrics`; the validator | `evaluate`, `checkRoute` |
| [src/main.cpp](../src/main.cpp) | Command line, results table, CSV export; which settings each version of B runs with | `parseOptions`, `ilsParamsFor`, `runPlanner`, `writeRoutesCsv`, `writeInstanceCsv` |
| [viz/](../viz/README.md) | Route map, separate from the solver | `make_map.py`, `map_template.html` |

## 11. v2 changes

v1 (commit `94aa4dd`) had:
- one HQ in Kuala Lumpur;
- 168 continuous hours with no nights;
- `pop × alpha^(v−1)` reach;
- a hard no-clash rule.

On 2026-10-29 the model changed to what this document describes:

| # | Change | Where it is described |
|---|---|---|
| 1 | Each politician has their own home base, either fixed or random with a seed | §3, §4, ASSUMPTIONS V1, V9, V10 |
| 2 | Time decay: the fading repeat penalty and the polling-day weight | §3, §5.4, ASSUMPTIONS V4, V5 |
| 3 | Everyone must be back at their own home by the end of the campaign | §3, §8, ASSUMPTIONS V2 |
| 4 | Soft clash rule: wait, clash at an overlap-weighted penalty, or go elsewhere | §1, §3, §6, ASSUMPTIONS V6, V7, V12 |
| 5 | 14 days × 12 active hours; rallies and travel only in the window | §3, §5.2, §5.5, ASSUMPTIONS V3, V8 |

The full list (options, output files, every file in the code, and v1 vs v2 results) is in [CHANGES_V2.md](CHANGES_V2.md).

### Changes since v2

Made on 2026-10-30 and 2026-10-31 through GitHub issues, after a review of v2:

| Issue | Change | Where it is described |
|---|---|---|
| #3 | A rally still running counts towards a later rally's repeat penalty only for the part that doesn't overlap, so clashing can pay off | §3, ASSUMPTIONS V12, RESULTS §3 |
| #4 | Rallies after polling day reach nobody, and the validator flags them | §3, §6.0, §8, ASSUMPTIONS V5 |
| #5 | The repeat penalty is `alpha^E` at whole E and linear in between, so it fades at alpha = 0 too | §3, ASSUMPTIONS V13 |
| #6 | Out-of-range `--alpha` and `--clash-penalty`, and unknown approaches, are rejected | §8, README |
| #7 | B runs its search from empty routes and from A's plan, and keeps the better plan | §6.3, §7, ASSUMPTIONS A6 |
| #8 | The politicians are teammates planned together, not rival parties | §1, ASSUMPTIONS S8 |
| #2 | The crowd is split by how many politicians overlap at once | §3, §5.4, §7, ASSUMPTIONS V6 |
| #9 | Stray files removed, notebook renamed | README |
| #11 | Five places that double-count a city are dropped from the data; the default instance has 90 towns | §4, ASSUMPTIONS D6 |
| #12 | A budgeted nearest-neighbour benchmark, valid and comparable with A and B; the full tour reports how long it needs | §6.0, ASSUMPTIONS A7, RESULTS §1 |
| — | The daily window is 06:00–22:00 (16 h) instead of 08:00–20:00 (12 h), so the campaign has 224 active hours. Budgeted nearest neighbour now beats A at the defaults | §3, ASSUMPTIONS V3, V8, RESULTS §6 |
| #13 | Local-search moves after each insertion round (`ils-moves`) | §6.5, RESULTS §8 |
| #14 | Go back to the plan before the shake if it got worse (`ils-revert`); the shake skips removals that make a later stop late | §6.5, RESULTS §8 |
| #15 | Adaptive large neighbourhood search (`ils-alns`); not adopted | §6.5, RESULTS §8 |
| #16 | Cached insertion scores (`ils-cache`, used by every version); granular lists tried and not used | §6.5, §7, RESULTS §8 |
| #17 | Multi-start A (`multistart`); not adopted | §6.5, RESULTS §8 |
