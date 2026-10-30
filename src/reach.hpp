#pragma once

#include "booking.hpp"
#include "instance.hpp"

#include <vector>

// The reach model (see docs/DESIGN.md section 11.4). For a rally r in town c:
//
//   reach(r) = pop(c) * P(E(r)) * w(start) * share(r)
//
//   P(E)       repeat penalty: alpha^E at whole E, linear in between,
//              i.e. alpha^n * (1 - (1 - alpha) * (E - n)) with n = floor(E)
//   E(r)       sum over rallies j that started earlier in c of
//              (1 - overlap(r, j) / rally(c)) * exp(-(start_r - finish_j) / tauRepeat)
//   w(s)       exp(-(polling day - s) / tauPoll)
//   share(r)   average over r of (1 - beta) / (1 - beta + m * beta), where m is how many other
//              politicians are rallying in c at that moment (no loss while m = 0)
//
// Gaps in E and w are in wall-clock hours, because people forget overnight too.
// A rally that runs past the end of the campaign (after polling day) reaches 0.

// Reach of each rally in `rallies` (all in `town`), in the same order.
std::vector<double> rallyReaches(const Instance& inst, int town, const std::vector<Booking>& rallies);

// Total reach of all the rallies in `town`.
double townReach(const Instance& inst, int town, const std::vector<Booking>& rallies);

// How much the total reach of `town` changes if `added` joins its current `rallies`. This counts
// what the new rally reaches minus what it costs the others (overlap, and the repeat penalty
// for rallies that come after it).
double reachGain(const Instance& inst, int town, const std::vector<Booking>& rallies, const Booking& added);
