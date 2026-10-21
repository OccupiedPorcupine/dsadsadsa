# Assumptions and decisions

The modelling choices behind the campaign-routing project. Update this whenever an assumption changes, and note the date.
The code reads its defaults from `Config` in `src/instance.hpp`. If you change a number here, change it there too, and the other way round.

## The scenario

| # | Assumption | Why |
|---|---|---|
| S1 | k politicians from the same party campaign during the official campaign period (default k = 2: party leader and deputy). | Malaysian general elections have a short, fixed campaign period. |
| S2 | No two politicians may hold a rally in the same town at the same time. | Security (top leaders shouldn't be in one place at once) and coverage (two headline speakers in one town at once wastes one of them). |
| S3 | Every politician starts and ends at party HQ in Kuala Lumpur. | One shared base. HQ is the GeoNames record with feature code `PPLC`. |
| S4 | The goal is to maximise total population reached, not to visit every town. | There isn't enough time to go everywhere, so towns have to be chosen. In the literature this is the Team Orienteering Problem. |
| S5 | A repeat rally in a town still has value but less: the v-th rally there reaches `population × alpha^(v-1)`. Default alpha = 0.5. | Without this, sending two politicians to one town is pointless and the clash rule never matters. A second speaker draws a partly new crowd and reinforces the message. |
| S6 | Each politician holds at most one rally per town. | A politician returning to the same town is less useful than going somewhere new. It also keeps the model simple. |
| S7 | Reach is shared fairly when reporting: in a town, the earliest rally gets the full population, the next gets alpha × that, and so on. | Only used for the per-politician fairness figure. It doesn't affect total reach. |

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
| T5 | The campaign is a single budget of continuous hours (default 168), and everyone must be back at HQ by the end. | Simplest first version. |
| T6 | No daily schedule: rallies can happen at any hour, and there is no sleep or rest. | **Unrealistic, and the next thing to fix.** It also explains why the clash rule rarely forces anyone to wait: with 24-hour days there is always another free time slot. |
| T7 | A politician who reaches a town while another politician is still holding a rally there waits until it ends. | This is how the clash rule is enforced. Waiting time is reported. |

## Algorithms

| # | Decision | Why |
|---|---|---|
| A1 | Baseline: each politician plans greedily as if alone, which is the classic single-traveller view. | Shows what goes wrong without coordination: identical routes and a clash in every town. |
| A2 | Approach A ranks towns by `reach / (travel + wait + rally hours)`. | Plain greedy "reach per hour". |
| A3 | Approach B ranks insertions by `reach² / extra time`. | Taken from Vansteenwegen et al. (2009). Squaring favours big towns. |
| A4 | Approach B stops after 150 rounds without improvement or 5,000 rounds in total. | The paper's stopping rule is similar. Runs finish in well under a second. |
| A5 | Approach B's shake rule: the removal point moves along the route, and the number removed grows by 1 each round, resetting after an improvement or once it exceeds `towns / (3k)`. | Follows the paper. |

## Open questions

- Daytime-only rallies (for example 08:00 to 23:00), with nights spent wherever the politician ends the day. See T6.
- Should a politician be allowed to return to a town later in the campaign? See S6.
- Exact solver for small instances (about 10 towns), to measure how far A and B are from the best possible plan.
- Fix the duplicate towns that survive merging. See D3 and the known data issue.

## Change log

- 2026-10-21: First version. Model and defaults as above.
