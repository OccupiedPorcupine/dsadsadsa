# AI usage acknowledgement

The brief (section 11.3) requires every project to state the AI tools used, how and where they were used, which parts are the team's own, and three representative prompts.

**This is a draft.** Fill in the parts marked _TEAM_ yourselves. Only the team can say what was originally theirs, and every member must be able to explain all of the submitted work.

## Tools used

| Tool | Version | Used for |
|---|---|---|
| Claude Code (VS Code extension) | Claude Opus 5.5 | Brainstorming, literature search, coding, visualisation, documentation |
| _TEAM: add any others (ChatGPT, Copilot, etc.)_ | | |

## How and where AI was used

| Area | What the AI did | What the team did |
|---|---|---|
| Choosing the problem | Suggested candidate use cases from the dataset profile. Pointed out that the "two travellers never meet" idea is trivial without a time limit, and that a reach objective alone makes the clash rule irrelevant. | _TEAM_ |
| Literature | Searched for related work (Team Orienteering, Travelling Politician Problem, Cooperative Pathfinding), downloaded the open-access papers and wrote [papers/README.md](papers/README.md). | _TEAM_ |
| Algorithm design | Proposed Approach A (booking table) and Approach B (ILS with clash-aware `maxShift`), adapted from the papers. | _TEAM_ |
| Code | Wrote the C++ solver in `src/` and the stress tests, then rewrote it for readability on request. | _TEAM_ |
| Visualisation | Wrote `viz/make_map.py` and `viz/map_template.html`, and chose and validated the colours. | _TEAM_ |
| Documentation | Drafted the README, [ASSUMPTIONS.md](ASSUMPTIONS.md), [DESIGN.md](DESIGN.md), [RESULTS.md](RESULTS.md) and this file. | _TEAM_ |
| v2 model changes | Asked the team to decide on starting points, the time-decay function, how clash reach is shared, and night travel. Then implemented the agreed changes (own home bases, a 14 × 12 h daily window, time decay, the soft clash rule), re-ran every experiment, and wrote [CHANGES_V2.md](CHANGES_V2.md). | _TEAM_ (chose: fixed default + random homes, both decay types combined, the overlap-weighted penalty, no night travel) |
| Experiments | Ran the parameter sweeps in [RESULTS.md](RESULTS.md). All numbers come from real runs and are reproducible with the commands given there. | _TEAM_ |
| Algorithm experiments | Searched the TOPTW literature for faster or better methods than B, drafted experiments #13–#17 as GitHub issues, ran each in a separate subagent and git worktree against a shared benchmark, verified and posted the results, merged them, and kept each as its own approach (RESULTS §8). | _TEAM_ (chose: run the experiments, keep every version as its own approach for the presentation) |
| Review and fixes after v2 | Reviewed the v2 code and model, drafted GitHub issues #2–#10 for the team to approve, implemented the agreed fixes one commit per issue, checked each against the validator and the old behaviour, and documented the results in the issues and in [CHANGES_V2.md](CHANGES_V2.md) §11. | _TEAM_ (chose: teammates rather than rivals, the overlap-scaled repeat penalty, the crowd split for several overlapping rallies, which issues to fix) |

## Parts that are the team's own

_TEAM: list them. For example: choosing the election framing, the diminishing-returns decision, parameter choices, interpreting the results, the presentation._

## Three representative prompts

1. **Design.** Turning the idea into a non-trivial problem:
   > "ok how about this idea. two people want to travel the same itenary but never want to be in the same city at the same time."

2. **Design and literature.** Grounding the algorithm in published work:
   > "for now lets do the diminishing returns variant instead of the other two. How would we design an algo that could solve this problem? (find papers online that have solved this, im pretty sure there's examples)"

3. **Implementation.**
   > "alright, can we now start with the implementation of the solution? i want it C++"

_TEAM: swap in other prompts if these aren't representative of how you used AI._
