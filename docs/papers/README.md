# Reading list: Campaign routing with k politicians

Problem: k politicians from one party, a limited campaign time, maximise population reach with diminishing returns for repeat visits, and a penalty (not a ban) when two or more politicians rally in the same town at the same time.

Literature name: **Team Orienteering Problem (TOP)** + **repeat visits with decaying reward** + **soft (priced) time-based mutual exclusion at vertices**.
We found no paper that combines all three. That gap is the novelty claim.

## Downloaded (in this folder)

| File | Citation | Why it matters | Used in |
|---|---|---|---|
| `Shahmanzari2022_MultiPeriod_Travelling_Politician.pdf` | Shahmanzari, Aksen, Salhi. *Multi-Period Travelling Politician Problem: A Hybrid Metaheuristic Solution Method.* J. Oper. Res. Soc. 73(6), 2022. [arXiv:2012.15320](https://arxiv.org/abs/2012.15320) | **Precedent for the story.** Party leader campaigning across Turkish cities. Rewards depreciate by meeting day and recency of the last meeting in that city. Solved with multi-start ILS + VND. **Only one politician.** | Motivation, related work |
| `Buijs2015_ILS_TOPTW_Replication_Thesis.pdf` | Buijs. *Implementation of an iterated local search heuristic for the TOPTW.* Erasmus BSc thesis, 2015. | Step-by-step re-implementation of Vansteenwegen et al. (2009), with worked examples. Insertion ratio is `Ratio = S² / TimeInsertion` (eq. 14). Feasibility uses `Arrival, Start, Wait, MaxDelay` per visit. | **Approach B** |
| `Silver2005_Cooperative_Pathfinding.pdf` | Silver. *Cooperative Pathfinding.* AIIDE 2005. | Introduces the **reservation table**. Agents planned one at a time mark (place, time) entries, and later agents treat them as blocked. | **Approach A** |
| `Jia2024_Repeatable_Visits_Extended_TOP.pdf` | Jia et al. *Route planning of mobile sensing fleets for repeatable visits.* [arXiv:2307.02397](https://arxiv.org/abs/2307.02397) | Extended TOP. Points can be visited multiple times to accumulate score. Large neighbourhood search + matheuristic. | Diminishing-returns model |
| `TOP2024_Mandatory_Incompatible_Nodes.pdf` | Guastalla, Aringhieri, Hosteins. *The TOP with Service Times and Mandatory & Incompatible Nodes.* [arXiv:2410.12368](https://arxiv.org/abs/2410.12368) | Closest existing "conflict" constraint in TOP: incompatible nodes. Contrast with ours, which is a *time*-based conflict at the *same* node. | Related work / novelty |
| `Survey2025_Orienteering_Model_Evolution.pdf` | Shen, Zhou, Lei, Wu. *A survey of the orienteering problem: model evolution, algorithmic advances, and future directions.* [arXiv:2512.16865](https://arxiv.org/abs/2512.16865) | Recent overview of OP variants. Use it to check the novelty claim. | Related work |
| `Wichita_Political_Campaign_Resource_Allocation_Routing.pdf` | *Optimization of resource allocation and routing in political campaign problems.* Wichita State Univ. thesis. | Campaign budget allocation + mass-meeting routing via dynamic programming. A secondary motivation source. | Motivation |

## Not downloaded: get these manually

**SMU repository (ink.library.smu.edu.sg) blocks automated downloads. Open these in a browser while on the SMU network or logged in:**

| Paper | Link | Why |
|---|---|---|
| Gunawan, Lau, Vansteenwegen. *Orienteering Problem: A survey of recent variants, solution approaches and applications.* EJOR 255(2):315–332, 2016 | [SMU INK 3271](https://ink.library.smu.edu.sg/sis_research/3271) (PDF: `cgi/viewcontent.cgi?article=4273&context=sis_research`) | The standard OP survey. Covers multi-agent OP variants. |
| Gunawan, Lau, Vansteenwegen, Lu. *Well-tuned algorithms for the TOPTW.* J. Oper. Res. Soc., 2017 | [SMU INK 3685](https://ink.library.smu.edu.sg/sis_research/3685) (PDF: `cgi/viewcontent.cgi?article=4687&context=sis_research`) | ILS and SAILS (simulated annealing + ILS) for TOPTW. Possible upgrade path for Approach B. |

**Paywalled. Try SMU library access through the DOI or the publisher page:**

| Paper | Link | Why |
|---|---|---|
| Vansteenwegen, Souffriau, Vanden Berghe, Van Oudheusden. *Iterated local search for the team orienteering problem with time windows.* Computers & Operations Research 36(12):3281–3290, 2009 | Search the title on Google Scholar or the SMU library | **The original Approach B algorithm.** The Buijs thesis above covers it in detail if this is unavailable. |
| Aksen, Shahmanzari. *A periodic traveling politician problem with time-dependent rewards.* Operations Research Proceedings 2016, Springer, 2018 | [doi:10.1007/978-3-319-55702-1_37](https://doi.org/10.1007/978-3-319-55702-1_37) | The first Travelling Politician paper (MILP model). |
| *Team orienteering with possible multiple visits: Mathematical model and solution algorithms.* Computers & Industrial Engineering, 2024 | [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S0360835224002183) | Repeat visits with decreasing scores. Closest match to our α decay. |
| Chao, Golden, Wasil. *The team orienteering problem.* EJOR 88(3), 1996 | Search the title on Google Scholar | Original TOP definition. |

## Suggested reading order
1. Shahmanzari 2022, sections 1–3: the story and how they model reward decay.
2. Buijs 2015: the insertion, shake, and `MaxDelay` mechanics behind Approach B.
3. Silver 2005: about 2 pages, the reservation table behind Approach A.
4. Jia 2024 and TOP2024: skim for the related-work slide.
