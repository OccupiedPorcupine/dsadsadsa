# Literature summary

This covers every source cited in the code, the docs and the slides, and what each one contributed. The reading list with download links is in [papers/README.md](papers/README.md).

Our problem is a **Team Orienteering Problem with Time Windows** with two additions: repeat rallies that are worth less but recover over time, and a penalty (not a ban) when politicians rally in the same town at once. We found no paper that combines team routing, fading repeats and soft clashes. That claim only reflects the sources below; it is not an exhaustive search.

## At a glance

"Access" says what this summary is based on. **PDF** means the full text is in `docs/papers/` and was checked. **Abstract** means the paper is not in the repo, so its description comes from its published abstract or record, and from how our docs use it.

| Source | Topic | Role in the project | Result for us | Access |
|---|---|---|---|---|
| Vansteenwegen et al. 2009 | ILS for TOPTW | Base algorithm for Approach B | Used | Abstract, via Buijs 2015 |
| Buijs 2015 | Re-implementation of the above | Our working reference for B | Used | PDF |
| Silver 2005 | Reservation tables | Basis of Approach A and `BookingTable` | Used | PDF |
| Shahmanzari et al. 2022 | Travelling Politician Problem | Motivation, reward decay, local-search moves | Used | PDF |
| Gunawan et al. 2017 | SAILS for TOPTW | `ils-revert` (go back if worse) | Used, at temperature 0 | Abstract |
| Labadie et al. 2012 | Granular neighbourhoods | Idea tested in #16 | Tried, not used | Abstract |
| Ropke & Pisinger 2006 | ALNS | `ils-alns` | Tried, not adopted | Abstract |
| Hammami et al. 2020 | Hybrid ALNS for TOP | `ils-alns` | Tried, not adopted | Abstract |
| Panadero et al. 2021 | Biased randomisation | `multistart` | Beats plain A, well below B | Abstract |
| Ji et al. 2024 | TOP with repeat visits | Diminishing-returns model | Related work | PDF |
| Guastalla et al. 2024 | TOP with incompatible nodes | Contrast for the novelty claim | Related work | PDF |
| Shen et al. 2025 | OP survey | Check on the novelty claim | Related work | PDF |
| Akgun 2017 | Campaign budget and routing | Secondary motivation | Related work | PDF |

## Sources behind the algorithms

### Vansteenwegen, Souffriau, Vanden Berghe & Van Oudheusden (2009)

**What it does:** the iterated local search for TOPTW that alternates an insertion step with a shake step.

**What we took:** this is "B as published" (`ils`). The full mechanics below come from Buijs (2015), because the original is paywalled.

### Buijs (2015)

**What it does:** a bachelor's thesis that re-implements Vansteenwegen et al. with worked examples, then extends it to extra constraints such as a money budget. Its scores are on average 1.2% below the original's.

**What we took:**
- Insertion ranked by `Ratio = S² / TimeInsertion`, which became our `gain² / extra time`.
- Per-visit `Arrival, Start, Wait, MaxDelay`, which became our `maxShift` for O(1) feasibility checks.
- The shake schedule: the removal point moves along the route, and the number removed grows each round.
- Stopping after 150 rounds without improvement.

**How ours differs:** their time windows are fixed opening hours. Our `maxShift` also covers other politicians' bookings, which change as the plan changes, so it is recomputed for all routes after every insertion.

### Silver (2005), *Cooperative Pathfinding*

**What it does:** multi-agent pathfinding for games. Agents are planned one at a time and mark the space-time cells they use in a shared **reservation table**. Later agents treat those cells as impassable.

**What we took:** planning politicians in turn against a shared table, which is Approach A and our `BookingTable`.

**How ours differs:** a booking is not a hard block. Overlapping is allowed and priced through the clash penalty.

### Shahmanzari, Aksen & Salhi (2022)

**What it does:** the Multi-Period Travelling Politician Problem, in which a party leader schedules meetings across Turkish cities during a fixed campaign. It uses a multi-start ILS with variable neighbourhood descent. Rewards change linearly with the meeting day and the recency of the last meeting in that city. High-value cities may host several meetings.

**What we took:**
- The real-world framing, and the idea that repeat rallies lose value based on recency.
- The local-search moves in `ils-moves`, adapted from its four neighbourhoods (§4.6.1). Our replace is close to its N3, swap between routes to N1 and swap within a route to N4. Moving a rally to another politician is our own adaptation, closest to N2.
- The granular filter (§4.7), which we tested and did not keep.

**How ours differs:** it has one politician, so there are no clashes. Our decay is exponential rather than linear.

## Ideas tried to improve B

### Gunawan, Lau, Vansteenwegen & Lu (2017)

Proposes SAILS, which adds simulated-annealing acceptance to ILS for TOPTW. `ils-revert` uses this at temperature 0, meaning it returns to the plan from before the shake if the new one is worse. Non-zero temperature added nothing measurable. RESULTS §8 reports a gain of 5–7% from going back to the earlier plan.

### Labadie, Mansini, Melechovský & Wolfler Calvo (2012)

Speeds up variable neighbourhood search on TOPTW by examining granular subsets of moves. Our granular candidate lists changed single runs by ±4% and added no reliable speed on top of the candidate cache, so they are not used.

### Ropke & Pisinger (2006)

Introduces adaptive large neighbourhood search, where competing destroy and repair heuristics are chosen according to past performance. It is the basis of `ils-alns`, which came out about even with B.

### Hammami, Rekik & Coelho (2020)

Applies ALNS to the TOP, with local search and an exact set-packing stage to recombine routes. We left out set packing because it assumes route values add up, and ours do not: politicians share towns and clash.

### Panadero, Ammouriova, Juan, Agustin, Nogal & Serrat (2021)

Combines a fast biased-randomised heuristic with parallel computing for real-time TOP. `multistart` adapts the biased selection: take the best option with probability 0.3, the next with 0.3 × 0.7, and so on, over 200 runs with shuffled politician orders. It beats A but stays well below B, because A only appends to routes.

## Related work and motivation

### Ji, Han & Ge (2024)

The open team orienteering problem with repeatable visits, solved with ALNS for vehicle-based air-quality sensing. Reward for a location is `w × q^β` with β in (0, 1), so each further visit adds less. This is the closest match to our diminishing returns, but it depends only on the number of visits, not on when they happen.

### Guastalla, Aringhieri & Hosteins (2024)

The TOP with service times, mandatory nodes and incompatibilities, solved with a cutting-plane algorithm. Its incompatibilities are forbidden arcs and node types that cannot share a route. These are the nearest "conflict" rule we found in the TOP, but ours is a time-based overlap at the same node, and it is priced rather than forbidden.

### Shen, Zhou, Lei & Wu (2025)

A PRISMA-based survey of orienteering research from 2017 to 2025, with a taxonomy of model extensions and solution methods. We used it to check the novelty claim.

### Akgun (2017)

A master's thesis on splitting a campaign budget between mass meetings and advertising while routing the meetings, solved with dynamic programming on a Turkish case study. It supports the motivation only.

## On the reading list, not used directly

| Source | Why it is listed |
|---|---|
| Chao, Golden & Wasil (1996) | Original definition of the TOP |
| Gunawan, Lau & Vansteenwegen (2016) | Standard survey of OP variants |
| Aksen & Shahmanzari (2018) | First Travelling Politician paper |
| Jung, Kim & Lee (2024) | TOP with repeat visits whose scores diminish |

## References

Aksen, D., & Shahmanzari, M. (2018). A periodic traveling politician problem with time-dependent rewards. In *Operations Research Proceedings 2016* (pp. 277–283). Springer. https://doi.org/10.1007/978-3-319-55702-1_37

Akgun, G. (2017). *Optimization of resource allocation and routing in political campaign problems* [Master's thesis, Wichita State University]. http://hdl.handle.net/10057/14523

Buijs, R. (2015). *Implementation of an iterated local search heuristic for the team orienteering problem with time windows* [Bachelor's thesis, Erasmus University Rotterdam]. https://thesis.eur.nl/pub/30280

Chao, I.-M., Golden, B. L., & Wasil, E. A. (1996). The team orienteering problem. *European Journal of Operational Research, 88*(3), 464–474.

Guastalla, A., Aringhieri, R., & Hosteins, P. (2024). *The team orienteering problem with service times and mandatory & incompatible nodes*. arXiv. https://arxiv.org/abs/2410.12368

Gunawan, A., Lau, H. C., & Vansteenwegen, P. (2016). Orienteering problem: A survey of recent variants, solution approaches and applications. *European Journal of Operational Research, 255*(2), 315–332.

Gunawan, A., Lau, H. C., Vansteenwegen, P., & Lu, K. (2017). Well-tuned algorithms for the team orienteering problem with time windows. *Journal of the Operational Research Society, 68*(8), 861–876. https://doi.org/10.1057/s41274-017-0244-1

Hammami, F., Rekik, M., & Coelho, L. C. (2020). A hybrid adaptive large neighborhood search heuristic for the team orienteering problem. *Computers & Operations Research, 123*, 105034. https://doi.org/10.1016/j.cor.2020.105034

Ji, W., Han, K., & Ge, Q. (2024). *Route planning of mobile sensing fleets for repeatable visits*. arXiv. https://arxiv.org/abs/2307.02397

Jung, H.-B., Kim, H.-I., & Lee, D.-H. (2024). Team orienteering with possible multiple visits: Mathematical model and solution algorithms. *Computers & Industrial Engineering*. https://doi.org/10.1016/j.cie.2024.110097

Labadie, N., Mansini, R., Melechovský, J., & Wolfler Calvo, R. (2012). The team orienteering problem with time windows: An LP-based granular variable neighborhood search. *European Journal of Operational Research, 220*(1), 15–27. https://doi.org/10.1016/j.ejor.2012.01.030

Panadero, J., Ammouriova, M., Juan, A. A., Agustin, A., Nogal, M., & Serrat, C. (2021). Combining parallel computing and biased randomization for solving the team orienteering problem in real-time. *Applied Sciences, 11*(24), 12092.

Ropke, S., & Pisinger, D. (2006). An adaptive large neighborhood search heuristic for the pickup and delivery problem with time windows. *Transportation Science, 40*(4), 455–472. https://doi.org/10.1287/trsc.1050.0135

Shahmanzari, M., Aksen, D., & Salhi, S. (2022). Multi-period travelling politician problem: A hybrid metaheuristic solution method. *Journal of the Operational Research Society, 73*(6), 1325–1346. https://doi.org/10.1080/01605682.2021.1895684

Shen, S., Zhou, Y., Lei, Q., & Wu, Z. (2025). *A survey of the orienteering problem: Model evolution, algorithmic advances, and future directions*. arXiv. https://arxiv.org/abs/2512.16865

Silver, D. (2005). Cooperative pathfinding. In *Proceedings of the First Artificial Intelligence and Interactive Digital Entertainment Conference (AIIDE 2005)*. AAAI Press.

Vansteenwegen, P., Souffriau, W., Vanden Berghe, G., & Van Oudheusden, D. (2009). Iterated local search for the team orienteering problem with time windows. *Computers & Operations Research, 36*(12), 3281–3290.
