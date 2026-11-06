# AI Usage Acknowledgement

## 1. Tools used

| Tool | Model | Used for |
|---|---|---|
| Claude Code (VS Code extension) | Claude Opus 5.5 | Brainstorming, literature search, algorithm design, C++ implementation, code review, experiments, visualisation, documentation |
| Claude (desktop app) | Claude Opus 5.5 | Presentation preparation |
| ChatGPT | GPT-5 | Explaining concepts and checking our understanding of the papers |

## 2. How and where AI was used

| Area | What the AI did | What the team did |
|---|---|---|
| Choosing the problem | Suggested candidate use cases from the dataset. Pointed out that the "two travellers never meet" idea is trivial without a time limit, and that a reach objective alone makes the clash rule irrelevant. | Proposed the first idea (two travellers who never meet) and then the election campaign scenario. Chose the time limit and diminishing returns to make the problem non-trivial. |
| Literature | Searched for related work (Team Orienteering, the Travelling Politician Problem, cooperative pathfinding) and summarised the open-access papers. ChatGPT explained concepts from the papers. | Read the key papers and chose which ideas to adapt. |
| Algorithm design | Proposed Approach A (greedy with a shared booking table) and Approach B (iterated local search with a clash-aware `maxShift`), adapted from the papers. | Reviewed and approved both designs. Decided to use Randomised Sequential Greedy as Approach A. |
| Model decisions | Laid out options for each modelling choice and implemented the agreed ones. | Chose: teammates rather than rival parties, separate home bases (fixed by default, random as an option), both kinds of time decay combined, the overlap-based clash penalty, no night travel, a 06:00–22:00 daily window, and the default parameter values. |
| Code | Wrote the C++ solver in `src/` and the validator, ran stress tests through the validator, then rewrote the code for readability on request. | Reviewed the code, asked for changes, and tested it on our own inputs. |
| Experiments | Ran the parameter sweeps and the five algorithm experiments (cache, go back if worse, ALNS, local-search moves, Randomised Sequential Greedy). All results come from real runs and can be reproduced with the commands in `docs/RESULTS.md`. | Chose which experiments to run, kept each version as its own approach, and interpreted the results. |
| Review and fixes | Reviewed the model and code, drafted GitHub issues for the problems found, and fixed the agreed ones in separate commits. | Approved which issues to fix and how. |
| Visualisation | Wrote the route map (`viz/make_map.py`, `viz/map_template.html`). | Checked the map against the results. |
| Documentation | Drafted the README and the documents on the design, assumptions, results and changes. | Reviewed and edited the documents. |
| Presentation | Drafted text for some slides and one script section, and checked slide figures against the results. | Planned and built most of the slides, wrote most of the scripts, and reviewed the AI's drafts. |

## 3. Parts that are the team's own

- The election campaign use case and the motivation for it
- The idea of a party splitting its members across the country instead of moving as one group
- The decision to plan the politicians as teammates rather than rival parties
- Which classic TSP assumptions to break: one traveller, equal cities, and no clock
- The time limit, diminishing returns, and the modelling choices and default values listed in section 2
- Choosing Randomised Sequential Greedy as Approach A
- Deciding which experiments to run and which versions of B to compare
- Interpreting the results and choosing the findings to present
- The presentation's structure, slides and delivery

## 4. Three representative prompts

1. **Design: turning the idea into a non-trivial problem**
   > "ok how about this idea. two people want to travel the same itenary but never want to be in the same city at the same time."

2. **Design and literature: grounding the algorithm in published work**
   > "for now lets do the diminishing returns variant instead of the other two. How would we design an algo that could solve this problem? (find papers online that have solved this, im pretty sure there's examples)"

3. **Implementation**
   > "alright, can we now start with the implementation of the solution? i want it C++"
