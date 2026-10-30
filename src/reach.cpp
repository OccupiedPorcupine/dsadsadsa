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

// Share of the crowd rally r keeps, averaged over its length. While m other politicians are
// rallying in the town at the same time, r keeps (1 - beta) / (1 - beta + m * beta) of it:
// 1 - beta when one other is there (the plain clash penalty), and 1 / (m + 1) at beta = 0.5,
// so any number of teammates speaking at once reach one crowd between them (#2).
static double crowdShare(const Instance& inst, const vector<Booking>& rallies, int r) {
    const Booking& rally = rallies[r];
    double beta = inst.clashPenalty;

    vector<double> cuts = {rally.start, rally.end};
    for (const Booking& other : rallies) {
        if (other.politician == rally.politician) continue;
        if (other.start > rally.start && other.start < rally.end) cuts.push_back(other.start);
        if (other.end > rally.start && other.end < rally.end) cuts.push_back(other.end);
    }
    sort(cuts.begin(), cuts.end());

    double kept = 0;
    for (int i = 0; i + 1 < (int)cuts.size(); ++i) {
        double middle = (cuts[i] + cuts[i + 1]) / 2;
        int others = 0;
        for (const Booking& other : rallies) {
            if (other.politician != rally.politician && other.start < middle && middle < other.end) others++;
        }
        double share = 1.0;
        if (others > 0) share = beta >= 1 ? 0.0 : (1 - beta) / (1 - beta + others * beta);
        kept += (cuts[i + 1] - cuts[i]) * share;
    }
    return kept / (rally.end - rally.start);
}

vector<double> rallyReaches(const Instance& inst, int town, const vector<Booking>& rallies) {
    const Clock& clock = inst.clock;
    double population = inst.towns[town].population;
    double length = inst.rallyHours(town);
    double pollingDay = clock.wallHours(clock.end(), true);

    vector<double> reaches(rallies.size(), 0.0);
    for (int r = 0; r < (int)rallies.size(); ++r) {
        const Booking& rally = rallies[r];
        // A rally that runs past the end of the campaign is after polling day, so it reaches nobody.
        // Only the benchmark makes these; the validator also flags them as errors.
        if (rally.end > clock.end() + EPS) continue;
        double startWall = clock.wallHours(rally.start);

        double repeats = 0;  // E(r): earlier rallies here, each fading with time since it ended
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
        }

        double repeatFactor = repeats > 0 ? pow(inst.alpha, repeats) : 1.0;
        double pollingWeight = exp(-(pollingDay - startWall) / inst.tauPollHours);
        reaches[r] = population * repeatFactor * pollingWeight * crowdShare(inst, rallies, r);
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
