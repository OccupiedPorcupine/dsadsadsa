# Changes from v1 to v2

What changed between v1 (commit `94aa4dd`, "Tidy docs and update map colours") and v2, and why. Implemented on 2026-10-29.

Sections 1–10 record v2 as it was first built. Fixes made since, through GitHub issues #2–#9, are marked *after v2* where they change something, and listed in full in [section 11](#11-fixes-since-v2).

Related documents:
- [DESIGN.md](DESIGN.md): the v2 design in full (problem §3, data structures §5, algorithms §6, complexity §7). §11 maps each change to the section that describes it.
- [ASSUMPTIONS.md](ASSUMPTIONS.md): assumptions V1–V13. The v1 rows they replace are marked.
- [RESULTS.md](RESULTS.md): all experiments re-run on v2.

## 1. The five requested changes

| # | Request | What v2 does | v1 behaviour |
|---|---|---|---|
| 1 | Different starting points, random or fixed | Each politician has their own home base. `--starts fixed` (the default) uses KL, George Town, Johor Bahru, Kota Bharu, Ipoh, Kuantan, Alor Setar in that order. `--starts random --seed N` picks k distinct towns reproducibly. `--starts "A,B,..."` names them. | Everyone started at one HQ in Kuala Lumpur |
| 2 | Time decay function | Both kinds combined. The **repeat penalty fades**: `alpha^E` with `E = Σ exp(−gap / tauRepeat)`. A **polling-day weight** `w = exp(−(polling − start) / tauPoll)` makes rallies closer to polling day worth more. | `alpha^(number of earlier rallies)`, no matter when they happened |
| 3 | Must return to their starting point by the end of the campaign | Each politician must be back at **their own** home by the end of day 14. They don't have to go home each night: overnight they stay wherever the day ended. | Back at KL HQ by hour T |
| 4 | Soft clash rule: wait, clash and share the reach, or go elsewhere | Both planners score every town with two timings, **wait** (until it's free) and **clash** (start now and overlap), and pick whichever adds the most reach per hour. "Go elsewhere" is another town scoring higher. A clashing rally's reach is multiplied by `1 − beta × overlap / rally length` (beta = 0.5). *Changed after v2 in #3 and #2:* see section 2. | Clashes were forbidden; a politician always waited |
| 5 | Rallies only in 12 active hours a day, over 2 weeks | 14 days × 12 h (08:00–20:00) = 168 active hours. **Rallies and travel** only happen in the window. A drive can be split across a night; a rally must fit inside one day. | 168 continuous hours, day and night |

## 2. Reach model

```
v1:  reach(v-th rally in town c) = pop(c) × alpha^(v−1)

v2:  reach(r) = pop(c) × alpha^E(r) × w(start_r) × (1 − beta × overlap(r) / rally(c))
     E(r)       = Σ over rallies j that started earlier in c of exp(−(start_r − finish_j) / tauRepeat)
     w(s)       = exp(−max(0, polling − s) / tauPoll)
     overlap(r) = hours of r overlapping other politicians' rallies in c, at most rally(c)
```

- Gaps in E and w are in **wall-clock hours** (nights count), because people forget overnight too.
- In v2 a town's total reach depends on **when** its rallies happen, not just how many there are.
- The upper bound behind `% max` changed from `Σ pop × alpha^i` to `k × Σ pop`. With fading decay, the old figure was no longer a true bound (V11).
- `w` is capped at 1. Only the nearest-neighbour benchmark rallies after polling day, and those rallies can't be worth more than a full crowd. *Replaced after v2 in #4:* a rally after polling day now reaches nobody, and the validator flags it (ASSUMPTIONS V5).
- *Changed after v2 in #5:* `alpha^E` is now `alpha^E` only at whole E, with straight lines in between. Plain `alpha^E` never fades at alpha = 0, so those results jumped compared with any alpha > 0 (ASSUMPTIONS V13).
- *Changed after v2 in #3:* a rally still running counts towards a later rally's `E` only for the part that doesn't overlap, `(1 − overlap / rally) × exp(−gap / tauRepeat)`. Before, clashing paid both penalties in full and was never worth choosing (ASSUMPTIONS V12).
- *Changed after v2 in #2:* `(1 − beta × overlap / rally)` is replaced by a crowd share. While m others rally in the town at the same time, a rally keeps `(1 − beta) / (1 − beta + m × beta)`. With one other this is the same as before; at beta = 0.5 any number of rallies at once reach one crowd between them (ASSUMPTIONS V6).

## 3. Command-line options

| Option | Status | Default | Meaning |
|---|---|---|---|
| `--hours H` | **removed** | — | Replaced by `--days` × `--day-hours` |
| `--days D` | new | 14 | Length of the campaign |
| `--day-hours H` | new | 12 | Campaigning hours per day (3 to 24) |
| `--day-start H` | new | 8 | Wall-clock hour the window opens (used for display only) |
| `--starts S` | new | `fixed` | `fixed`, `random`, or a comma-separated list of town names |
| `--seed N` | new | 1 | Seed for `--starts random` |
| `--tau-repeat H` | new | 48 | Hours for the repeat penalty to fade |
| `--tau-poll H` | new | 336 | Hours for early rallies to be forgotten by polling day |
| `--clash-penalty B` | new | 0.5 | Share of reach a fully overlapped rally loses (beta) |

The 7 × 24 h setting (`--days 7 --day-hours 24`) gives v1's 168 continuous hours, but uses v2's reach model.

*Added after v2 in #6:* `--alpha` and `--clash-penalty` must be between 0 and 1, and `--approach` must be a known name. Anything else is an error (exit code 1).

## 4. Output

**Console table:** a new `clash_h` column (total overlapping hours). `valid` now means "no rule broken". Clashes alone don't make a plan invalid. The header shows the home bases, the window and the decay settings. *Changed after v2:* rallies after polling day are listed as errors (#4), and the `ILS:` line gives the reach of both of B's runs (#7).

**Files written by `--routes-dir`:**

| File | Change |
|---|---|
| `<approach>_routes.csv` | New columns `day` (1-based), `clash` (0/1, overlaps someone) and `reach` (the validator's reach for that rally). `return_h` is now the time back at the politician's own home. Times are active hours. |
| `meta.csv` | Removed `hq_name`, `hq_lat`, `hq_lon`. Added `days`, `day_hours`, `day_start`, `tau_repeat`, `tau_poll`, `clash_penalty`, `max_reach`. |
| `homes.csv` | **New.** One row per politician: `politician, name, lat, lon`. |
| `towns.csv` | Unchanged. |

**`--summary` CSV:** the header changed. Old summary files can't be appended to.
`approach,towns,k,alpha,days,day_hours,homes,tau_repeat,tau_poll,clash_penalty,reach,max_reach,visits,clashes,clash_h,travel_h,wait_h,fairness,valid,runtime_ms`

## 5. Code changes

| File | Change |
|---|---|
| [src/instance.hpp](../src/instance.hpp) | New `Clock` (`dayOf`, `dayEnd`, `fitRally`, `wallHours`). `Config` gains `starts`, `seed`, `clock`, `tauRepeatHours`, `tauPollHours` and `clashPenalty`, and loses `campaignHours`. `Instance` replaces `hq` / `hqIndex()` with `homes` / `homeIndex(p)`, and `campaignHours` becomes a method. `rallyReach` is removed (see `reach.*`). `EPS` moved here from `booking.hpp`. New `DEFAULT_HOMES`. |
| [src/instance.cpp](../src/instance.cpp) | No longer looks for the `PPLC` capital. New `chooseHomes`, `findPlace`, `randomHomes` (own Fisher-Yates on `mt19937`, so seeds give the same homes on every compiler) and `homesFromNames`. The travel matrix is `(n + k) × (n + k)`. |
| [src/reach.hpp/.cpp](../src/reach.cpp) | **New.** The reach model: `rallyReaches`, `townReach`, and `reachGain` (the change in a town's total reach when one rally is added, including what it costs the others). |
| [src/booking.hpp/.cpp](../src/booking.cpp) | Constructed with a `Clock`. `earliestFreeTime` respects the daily window and repeats its scan until nothing overlaps, because bookings can now overlap each other. New `overlapHours` and `bookingsIn`. |
| [src/solution.hpp/.cpp](../src/solution.cpp) | `Visit` gains `clash`. `Metrics` gains `clashHours` and `visitReach`, and `valid()` no longer requires zero clashes. The validator checks that every rally fits in one day's window and that each politician gets back to their own home in time, and recomputes reach with `reach.cpp`. |
| [src/nearest.cpp](../src/nearest.cpp) | One tour per politician, from their own home. A rally that won't finish by 20:00 starts the next morning. |
| [src/sequential.cpp](../src/sequential.cpp) | `buildGreedyRoute` tries both timings (wait and clash) for every town and scores by `reachGain / hours`. The `ralliesInTown` counts are gone. |
| [src/ils.cpp](../src/ils.cpp) | `PlannedVisit` and `Insertion` gain a timing mode. `maxShift` adds a "slack before the window closes" term and is never negative. `tryInsertion` tries both timings, scores by `reachGain² / delay`, and skips the exact reach calculation when `pop² / delay` can't beat the best so far. `recalculateTimes` keeps a rally's old start (as a clash) instead of moving it later. `totalReach` uses `townReach`. |
| [src/main.cpp](../src/main.cpp) | New options, `homes.csv`, new columns, a new console header. `--hours` removed. |
| [viz/make_map.py](../viz/make_map.py), [viz/map_template.html](../viz/map_template.html) | Read `homes.csv` and the new columns. Routes start and end at each politician's own home, shown as an `H` badge in their colour. Times are shown as "Day 3, 14:30". Stats add up the solver's per-rally reach instead of recomputing it with the old formula. The route table gains a Reach column. |

Code size: about 1,130 lines of C++ (sources and headers) in v1, about 1,460 in v2.

## 6. Changes to the algorithms

- **Approach A** still only appends to the end of a route. It now chooses a timing (wait or clash) as well as a town, and its score is the change in total reach, which includes the penalty a clash puts on the other politician's rally.
- **Approach B** keeps the O(1) feasibility check. `maxShift = max(0, min(slack before next other booking, slack before 20:00, what the next visit can absorb))`. Clashes are only created on purpose, at the visit being inserted. A later visit is never pushed into one.
- **Approach B, ranking:** candidates are ranked using the reach change in the inserted town only. The small change in reach of the later rallies it pushes back (through `w` and `E`) is ignored when ranking. Each round's total reach is always exact. One consequence is in [RESULTS.md §5](RESULTS.md): with the polling-day weight turned off, B can lose to A. *Addressed after v2 in #7:* B now also runs from A's plan and keeps the better result (DESIGN §6.3).
- **The baseline** is now valid in every run. Its clashes cost it reach instead of making the plan invalid.

## 7. Effect on the results (default run)

| | v1 | v2 |
|---|---|---|
| Setting | k = 2, both from KL, 168 continuous h, `alpha^n` reach, hard clash rule | k = 2, KL + George Town, 14 × 12 h, time decay, beta = 0.5 |
| Baseline | 29.24M, 45 clashes, **invalid** | 12.02M, 28 clashes, valid but heavily penalised |
| A | 30.43M, fairness 0.68, 96 h travel | 19.02M, fairness 0.90, 114 h travel |
| B | 31.87M, fairness 0.93, 68 h travel, ~125 ms | 22.06M, fairness 0.93, 122 h travel, ~215 ms |
| B / A | 1.047 | 1.160 |
| Waiting | about 0 h | 4–8 h |

These are the figures when v2 was first built. With the fixes since, the default run (now 90 towns) is: budgeted nearest neighbour 16.89M, baseline 15.03M, A 17.06M (fairness 0.91), B 19.96M (fairness 0.90), B / A 1.170 (RESULTS §1).

Reach figures are lower in v2 mainly because of the polling-day weight (a day-1 rally counts for about 0.38). They measure something different from v1's, so the two columns can't be compared directly. The main new findings are in [RESULTS.md §8](RESULTS.md):
- B's lead over A grew.
- A and B almost never choose to clash. With beta > 0 they always wait or go elsewhere. *Changed after v2 in #3:* they now clash when beta < (1 − alpha) / 2, which is 0.25 at the default alpha.
- The daily window makes waiting matter.
- The decay settings affect reach more than the choice of algorithm does.

## 8. How v2 was checked

- 576 settings were run through the validator, and every baseline, A and B plan was valid. The settings were k ∈ {1, 2, 3, 5} × alpha ∈ {0, 0.5, 1} × beta ∈ {0, 0.5, 1} × {3, 14} days × {fixed, random} homes × {default, `--borneo`, `--max-towns 15`, `--day-hours 8`}.
- B's internal running total of reach matched the independent validator's figure exactly on the runs checked (default, k = 3 with alpha = 1 and beta = 0, k = 5 with random homes, Borneo).
- The clash option was checked by forcing it. With `beta = 0` and `alpha = 1` and both politicians in KL, A clashes 43 times. With any `beta > 0` it never does.
- The map was generated and rendered in headless Chrome.

## 9. Documentation updated

- **README.md:** problem statement, examples (`--hours` is gone), approach table, derived attributes, output columns, file list.
- **docs/DESIGN.md:** rewritten for v2. It now covers the use case, why TSP doesn't fit, the problem statement with the reach formula and the active clock, `Clock`, the soft-clash `BookingTable`, the reach model, `maxShift` with the window term, updated pseudocode for every approach, a new complexity table (O(k²) per town for reach), checks, limitations and the code map. §11 maps each change to the section that describes it.
- **docs/ASSUMPTIONS.md:** V1–V12 marked as implemented, the v1 rows they replace marked, and a change log entry added.
- **docs/RESULTS.md:** every section re-run on v2, with new sections on the clash penalty, starting points, decay parameters, and the daily window vs continuous hours.
- **viz/README.md:** new input files and columns, and the home badges.
- **results/:** `default/` regenerated. `sweep/` now holds `settings`, `clash_penalty`, `starts`, `decay`, `window` and `scaling`.

## 10. Known gaps and next steps

- **Clashes are rarely chosen** (V12). *Addressed after v2 in #3:* an overlapping rally now only counts as a repeat for its non-overlapping part, so a full overlap pays only `beta`. See ASSUMPTIONS V12 and RESULTS §3.
- **B's ranking ignores time-dependent knock-on effects.** A ranking that accounts for the whole route, or a final local-search pass on exact reach, could fix the cases where B loses to A. *Addressed after v2 in #7:* exact ranking was tried and didn't stop B losing to A. Instead, B now runs a second time from A's plan and keeps the better plan, so it never ends below A.
- **Starting-point and decay defaults are first guesses.** RESULTS.md §4–5 shows how sensitive the results are to them.
- **No fairness term yet.** With alpha = 0 and long campaigns, fairness still drops to 0.00 (RESULTS.md §2). *Changed after v2 in #5:* that came from alpha = 0 never fading. It now drops only as low as 0.38 for A and 0.47 for B (k = 5, alpha = 0, 6 days).

## 11. Fixes since v2

After v2 was built, a review raised the problems below as GitHub issues. Each was fixed in its own commit, and RESULTS.md and every CSV in `results/` were re-run after the last one.

| Issue | Problem | Fix | Commit |
|---|---|---|---|
| #3 | Clashing paid the repeat penalty and the overlap penalty in full, so it was never worth choosing | A rally still running counts as a repeat only for the part that doesn't overlap (ASSUMPTIONS V12) | `9637f8c` |
| #6 | Out-of-range `--alpha` or `--clash-penalty` and misspelt approaches were accepted silently | Rejected with an error and exit code 1 | `a60c584` |
| #9 | Stray files in the repo | Empty root `main.cpp` deleted, `.DS_Store` and `.ipynb_checkpoints/` untracked, notebook moved to `notebooks/explore_geonames.ipynb` | `848f9f3` |
| #4 | Rallies after polling day counted at full weight, so the invalid benchmark topped the reach table | They reach nobody, and the validator flags each one (ASSUMPTIONS V5) | `a1e581b` |
| #8 | Were the politicians teammates or rival parties? | Teammates, planned together (ASSUMPTIONS S8, DESIGN §1) | `fb0e66b` |
| #7 | B could lose to A | B runs from empty routes and from A's plan, and keeps the better plan (ASSUMPTIONS A6, DESIGN §6.3) | `47ff952` |
| #2 | Three or more overlapping rallies reached more than one crowd | Crowd share `(1 − beta) / (1 − beta + m × beta)` with m others present (ASSUMPTIONS V6) | `5514bdb` |
| #5 | At alpha = 0 the repeat penalty never faded, so results jumped compared with any alpha > 0 | `alpha^E` at whole E, linear in between (ASSUMPTIONS V13) | `158d1db` |
| — | The `clash` column in the routes CSV missed overlaps the planner didn't choose (the baseline's) | Written from the validator | `7ab845b` |
| — | `--routes-dir` and `--summary` into a missing folder wrote nothing, silently | Folders are created, or the run stops with an error | `8ce405e` |
| #11 | Five places carried a district's population on top of their city, inflating the JB area to 4.0M and the total to 24.0M | Dropped via `data/excluded_places.csv` (ASSUMPTIONS D6); 90 towns | `98dff91` |
| #12 | The nearest-neighbour benchmark visits every town, so it's never valid and can't be compared | Kept as the full tour (which reports how many days it needs: 22 with 12-hour days), plus a budgeted version that stops when it must head home (ASSUMPTIONS A7) | `50e401f` |
| — | 08:00–20:00 left only 12 campaigning hours a day | The window is 06:00–22:00 (16 h, 224 active hours); every result re-run. Budgeted nearest neighbour now beats A at the defaults (ASSUMPTIONS V3, RESULTS §6) | `628dbb6` |
