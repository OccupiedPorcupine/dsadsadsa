# Assumptions and decisions

The modelling choices behind the campaign-routing project. Update this whenever an assumption changes, and note the date.
The code reads its defaults from `Config` in `src/instance.hpp`. If you change a number here, change it there too, and the other way round.

## The scenario

| # | Assumption | Why |
|---|---|---|
| S1 | k politicians from the same party campaign during the official campaign period (default k = 2: party leader and deputy). | Malaysian general elections have a short, fixed campaign period. |
| S2 | *(v1, replaced by V6)* No two politicians may hold a rally in the same town at the same time. | Security (top leaders shouldn't be in one place at once) and coverage (two headline speakers in one town at once wastes one of them). |
| S3 | *(v1, replaced by V1, V2)* Every politician starts and ends at party HQ in Kuala Lumpur. | One shared base. HQ is the GeoNames record with feature code `PPLC`. |
| S4 | The goal is to maximise total population reached, not to visit every town. | There isn't enough time to go everywhere, so towns have to be chosen. In the literature this is the Team Orienteering Problem. |
| S5 | *(v1, replaced by V4)* A repeat rally in a town still has value but less: the v-th rally there reaches `population × alpha^(v-1)`. Default alpha = 0.5. | Without this, sending two politicians to one town is pointless and the clash rule never matters. A second speaker draws a partly new crowd and reinforces the message. |
| S6 | Each politician holds at most one rally per town. | A politician returning to the same town is less useful than going somewhere new. It also keeps the model simple. |
| S7 | Each rally's reach is credited to the politician who held it, using the full v2 formula (V4–V6, V12, V13). In a town, a later rally is discounted for the earlier ones, so whoever goes first gets the bigger share. | Only used for the per-politician fairness figure. It doesn't affect total reach. |
| S8 | The k politicians are teammates from one party, planned together by one planner to maximise the party's combined reach. They are not rival parties. | A party under a time crunch can't send its whole team to every town, so it has to split its people across the country and coordinate where each one goes. With rival parties, nobody would be trying to maximise the combined reach of both, so a single plan for everyone wouldn't make sense (#8). |

## Data and preprocessing

| # | Assumption | Why |
|---|---|---|
| D1 | Only records with feature class `P` (populated places) are used. | Rallies happen in towns, not at rivers, hills or hotels. |
| D2 | Only towns with population ≥ 50,000 are considered (`--min-pop`). | Only 791 of 71,709 records have a population at all. Campaigns target large towns. This gives about 92 Peninsular towns after merging, which is enough to be realistic and small enough to run quickly. |
| D3 | Places within 5 km of a larger kept place are dropped (`--merge-km`). | GeoNames lists cities and their districts separately, each with its own population, so summing them would count people twice. |
| D4 | Peninsular Malaysia only by default. Sabah, Sarawak and Labuan (longitude > 109°E) are added with `--borneo`. | Keeps the first version simple. Borneo needs flights (see T2). |
| D5 | Population figures are used as given by GeoNames. They are not real voter numbers and may be out of date. | No better source is in the provided data. |

**Known data issue:** some duplicates survive the 5 km merge. For example, "Kota Kuala Muda" (544,984) and "Sungai Petani" (544,851) are almost certainly the same population counted twice. Not fixed yet.

## Travel and time

| # | Assumption | Why |
|---|---|---|
| T1 | Driving time = straight-line distance × 1.3 ÷ 70 km/h. | The data has no road network. A factor of 1.3 is a common rough ratio of road distance to straight-line distance. 70 km/h averages highways and town roads. |
| T2 | Travel between the peninsula and Borneo is by plane: 2.5 h overhead + distance ÷ 700 km/h. | The sea crossing can't be driven. The overhead covers getting to the airport, check-in and boarding. |
| T3 | Travel times are fixed and the same in both directions. | No traffic or weather data. |
| T4 | Rally length = `1 + log10(population / 10,000)` hours, between 1 and 3 hours. | Bigger towns get bigger events. This gives 1 h at 10k people, 2 h at 100k, and 3 h at 1M or more. |
| T5 | *(v1, replaced by V2, V3, V8)* The campaign is a single budget of continuous hours (default 168), and everyone must be back at HQ by the end. | Simplest first version. |
| T6 | *(v1, replaced by V3)* No daily schedule: rallies can happen at any hour, and there is no sleep or rest. | **Unrealistic, and the next thing to fix.** It also explains why the clash rule rarely forces anyone to wait: with 24-hour days there is always another free time slot. |
| T7 | *(v1, replaced by V6)* A politician who reaches a town while another politician is still holding a rally there waits until it ends. | This is how the clash rule is enforced. Waiting time is reported. |

## Algorithms

| # | Decision | Why |
|---|---|---|
| A0 | Benchmark: the brief's classic nearest-neighbour TSP. One tour through every town, closest (by travel time) first, from each politician's own home. A rally starts on arrival, or the next morning if it wouldn't finish by 20:00. | Shows how the unmodified classic algorithm does on this problem before any redesign. It breaks the time budget, so it is reported but never valid. |
| A1 | Baseline: each politician plans greedily as if alone, which is the classic single-traveller view. | Shows what goes wrong without coordination: largely the same towns at the same times, many clashes, and the reach they cost. |
| A2 | Approach A ranks (town, timing) options by `reach gain / (travel + wait + rally hours)`, where the gain includes what a clash costs the other politician, and timing is wait or clash. | Plain greedy "reach per hour", which compares wait, clash and go elsewhere in one score. |
| A3 | Approach B ranks insertions by `reach gain² / extra time`, trying both timings. The exact gain is only computed when `population² / extra time` could beat the best so far. | Taken from Vansteenwegen et al. (2009). Squaring favours big towns. The bound skips most of the O(k³) reach calculations. |
| A4 | Approach B stops after 150 rounds without improvement or 5,000 rounds in total. | The paper's stopping rule is similar. Each run takes about 0.2 s at 92 towns and about 1 s at 255. |
| A5 | Approach B's shake rule: the removal point moves along the route, and the number removed grows by 1 each round, resetting after an improvement or once it exceeds `towns / (3k)`. | Follows the paper. |
| A6 | Approach B runs its whole search twice, from empty routes (as in the paper) and from Approach A's plan, and keeps the better plan. | From empty routes alone, B lost to A in 3 of 27 settings; from A's plan alone, it did worse than from empty routes in 8. Ranking insertions on the exact knock-on reach was tried first and didn't fix it (#7). Running both doubles B's runtime and guarantees B ≥ A. |

## v2 (agreed and implemented 2026-10-29)

These replace the rows named in the last column. Rows that have been replaced are marked *(v1, replaced by …)* in the tables above. How they are modelled is in [DESIGN.md](DESIGN.md) §3 (problem and reach formula), §5 (data structures) and §6 (algorithms). The full list of changes is in [CHANGES_V2.md](CHANGES_V2.md).

| # | Assumption | Why | Replaces |
|---|---|---|---|
| V1 | Each politician has their own home base. The default is a fixed list (KL, George Town, Johor Bahru, Kota Bharu, Ipoh, Kuantan, Alor Setar), set with `--starts`. `--starts random --seed N` picks k distinct random towns. | Senior figures campaign from their own constituencies and regional bases, not from one HQ. Random starts test whether the results depend on where politicians begin. | S3 |
| V2 | Each politician must be back at their own home by the end of day 14, but not every night. | They need to be home for polling day. Overnight stays elsewhere are normal on a campaign. | S3, T5 |
| V3 | Campaigning happens 08:00–20:00 for 14 days (168 active hours). Rallies **and** travel only happen in that window. A long drive can be split across a night; a rally can't. | The official campaign period is about two weeks, and rallies and driving at night are unrealistic. This also fixes T6. | T5, T6 |
| V4 | The repeat penalty fades with time: reach = `pop × alpha^E`, where `E = Σ exp(−gap_j / tauRepeat)` over earlier rallies in the town (default tauRepeat = 48 h, wall clock). A rally still running counts only for the part that doesn't overlap (V12). | A crowd that just heard a speech is mostly the same people, but a few days later the message needs reinforcing. It makes the *timing* of a revisit matter. *Shape changed after v2 in #5:* the penalty is now `alpha^E` only at whole E, with straight lines in between (V13). | S5 |
| V5 | Rallies closer to polling day are worth more: weight `w = exp(−(polling − start) / tauPoll)`, with default tauPoll = 336 h, so a day-1 rally is worth about 0.38 of a final-day one. A rally that runs past the end of the campaign is after polling day, so it reaches nobody, and the validator flags it as an error (#4). Only the benchmark makes these. | Voters forget early campaigning. Late rallies sway undecided voters. | (new) |
| V6 | Soft clash rule: two politicians *may* rally in the same town at overlapping times. While m other politicians are rallying in the same town, a rally keeps `(1 − beta) / (1 − beta + m × beta)` of its crowd, averaged over the rally (default beta = 0.5). With one other, that is `1 − beta` for the overlapping part. The planner picks whichever is worth most: wait, clash, or go elsewhere. | A full ban is stricter than real campaigns. A partial overlap (one rally finishing as another starts) costs little; a full overlap splits the crowd. `beta = 1` is close to the old hard rule. At beta = 0.5, any number of teammates speaking at once reach exactly one crowd between them. The first v2 rule added up overlaps, so three at once reached 1.5 crowds (#2). | S2, T7 |
| V7 | Clashes are reported but no longer make a plan invalid. | They are now a cost, not a violation. | — |
| V8 | Planners work on an *active clock* that counts only campaigning hours (0 to 168). A rally that wouldn't finish by 20:00 starts at 08:00 the next day, and the time spent waiting counts as waiting. | Keeps the timeline as one number line, so the booking table and `maxShift` work almost unchanged. | T5 |
| V9 | Home bases are matched to rally towns by name. A name that isn't a rally town (for example with `--max-towns 15`) is looked up among every populated place in the file. With more politicians than names, the list starts again from the top, so two politicians can share a home. | Lets any town be a base without changing the rally towns. | S3 |
| V10 | `--starts random` uses its own Fisher-Yates shuffle on `mt19937`. | `std::shuffle` can give different orders with different standard libraries, which would make seeded results depend on the machine. | — |
| V11 | The upper bound for `% max` is every politician reaching every town's full population once: `k × Σ population`. | With fading decay a late repeat rally can be worth almost a full crowd, so the v1 bound (`Σ pop × alpha^i`) is no longer a true upper bound. | — |
| V12 | An earlier rally counts towards a later one's repeat penalty only for the part of the later rally that doesn't overlap it: `E += (1 − overlap / rally length) × exp(−gap / tauRepeat)`. A full overlap pays only beta; no overlap pays the full repeat penalty. | While both rallies run, the crowd is split between them (beta), not already reached. Counting it in full as well (the first v2 rule) made clashing always worse than waiting, so the soft clash rule never fired (#3). Scaling by overlap keeps the score continuous, so a 1-minute overlap can't dodge the repeat penalty. Clashing now wins on reach when beta < (1 − alpha) / 2 (see [RESULTS.md §3](RESULTS.md)). | — |
| V13 | The repeat penalty is `alpha^E` at whole E and linear in between: `P(E) = alpha^n × (1 − (1 − alpha) × (E − n))` with `n = floor(E)`. A back-to-back repeat (E = 1) still reaches `alpha × pop`, a third reaches `alpha² × pop`, and the penalty fades as E falls, for every alpha. | Plain `alpha^E` never fades at alpha = 0, because `0^E = 0` for any E > 0, so one rally made a town worth nothing for the rest of the campaign. Results jumped between alpha = 0 and alpha = 0.001 (#5). With straight lines, a rally 2 days later at alpha = 0 reaches about 63% of the town, and results change smoothly with alpha. At whole E nothing changes, so the clash threshold beta < (1 − alpha) / 2 still holds. | V4 |

## Open questions

- Should a politician be allowed to return to a town later in the campaign? See S6. This becomes more relevant under V4, because a revisit days later is worth almost the full population.
- Default values for tauRepeat, tauPoll and beta (V4–V6) are first guesses. The first sensitivity sweeps are in [RESULTS.md](RESULTS.md) §3 and §5. tauPoll matters most, and needs a justification.
- Exact solver for small instances (about 10 towns), to measure how far A and B are from the best possible plan.
- Fix the duplicate towns that survive merging. See D3 and the known data issue.

## Change log

- 2026-10-21: First version. Model and defaults as above.
- 2026-10-21: Added the classic nearest-neighbour benchmark (A0).
- 2026-10-29: Agreed the v2 plan (V1–V7): separate home bases, return home by the end of the campaign, 14 × 12 h daily window, time decay (fading repeat penalty and polling-day weight), and a soft clash rule.
- 2026-10-29: Implemented v2 (V1–V7), and added V8–V12 for decisions made while building it. Capped the polling-day weight at 1 (V5).
- 2026-10-29: Updated S7 and A0–A3 for v2, and added the V12 open question.
- 2026-10-31: Changed V12 so overlapping rallies only count as repeats for their non-overlapping part (#3). Closed the V12 open question.
- 2026-10-31: Rallies after polling day now reach nobody instead of being capped at a final-day weight, and the validator flags them (V5, #4).
- 2026-10-31: Added S8: the politicians are teammates planned together, not rival parties (#8).
- 2026-10-31: Added V13: the repeat penalty is `alpha^E` at whole E and linear in between, so it fades at alpha = 0 too (#5).
- 2026-10-31: Changed V6 so the clash penalty depends on how many politicians overlap at once. At beta = 0.5 any number of them reach one crowd between them (#2).
- 2026-10-31: Added A6 (B runs from two starts, #7). A3's reach calculation is now O(k³), because of the crowd split in V6. Brought S7, V4 and A4 up to date.
