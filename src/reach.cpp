#include "reach.hpp"

#include <algorithm>
#include <cmath>

using namespace std;

// Did rally a start before rally b? Equal starts are broken by politician number.
static bool startedBefore(const Booking& a, const Booking& b) {
    if (a.start < b.start - EPS) return true;
    if (a.start > b.start + EPS) return false;
    return a.politician < b.politician;
}

vector<double> rallyReaches(const Instance& inst, int town, const vector<Booking>& rallies) {
    const Clock& clock = inst.clock;
    double population = inst.towns[town].population;
    double length = inst.rallyHours(town);
    double pollingDay = clock.wallHours(clock.end(), true);

    vector<double> reaches(rallies.size(), 0.0);
    for (int r = 0; r < (int)rallies.size(); ++r) {
        const Booking& rally = rallies[r];
        double startWall = clock.wallHours(rally.start);

        double repeats = 0;  // E(r): earlier rallies here, each fading with time since it ended
        double overlap = 0;
        for (int j = 0; j < (int)rallies.size(); ++j) {
            if (j == r) continue;
            const Booking& other = rallies[j];
            double together = max(0.0, min(rally.end, other.end) - max(rally.start, other.start));
            if (startedBefore(other, rally)) {
                // While both run, the crowd is split (beta) rather than already reached, so an
                // earlier rally only counts as a repeat for the part of r that doesn't overlap it.
                double notTogether = 1.0 - min(together, length) / length;
                double gap = max(0.0, startWall - clock.wallHours(other.end, true));
                repeats += notTogether * exp(-gap / inst.tauRepeatHours);
            }
            if (other.politician != rally.politician) overlap += together;
        }
        overlap = min(overlap, length);

        double repeatFactor = repeats > 0 ? pow(inst.alpha, repeats) : 1.0;
        // Capped at 1: a rally after polling day (only the benchmark does this) is worth no more than one on the last day.
        double pollingWeight = exp(-max(0.0, pollingDay - startWall) / inst.tauPollHours);
        double clashFactor = max(0.0, 1.0 - inst.clashPenalty * overlap / length);
        reaches[r] = population * repeatFactor * pollingWeight * clashFactor;
    }
    return reaches;
}

double townReach(const Instance& inst, int town, const vector<Booking>& rallies) {
    double total = 0;
    for (double reach : rallyReaches(inst, town, rallies)) total += reach;
    return total;
}

double reachGain(const Instance& inst, int town, const vector<Booking>& rallies, const Booking& added) {
    vector<Booking> withAdded = rallies;
    withAdded.push_back(added);
    return townReach(inst, town, withAdded) - townReach(inst, town, rallies);
}
