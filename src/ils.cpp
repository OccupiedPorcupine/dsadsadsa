#include "booking.hpp"
#include "planners.hpp"

#include <algorithm>
#include <limits>

namespace {

struct Slot {
    int town;
    double arrive, start, finish;
    // How much later this rally may start without pushing a later rally into a clash or the
    // politician home after the horizon. Computed backwards along the route.
    double maxShift;

    double wait() const { return start - arrive; }
};

class Search {
public:
    explicit Search(const Instance& inst)
        : inst_(inst), routes_(inst.k), booked_(inst.n()), visits_(inst.n(), 0),
          inRoute_(inst.k, std::vector<char>(inst.n(), 0)) {}

    // Greedily insert the best (town, politician, position) until nothing else fits.
    int insertUntilFull();

    // Remove `count` consecutive visits from every route, starting at position `start`
    // (wrapping around), then pull the remaining visits earlier.
    void shake(int start, int count);

    double reach() const;
    int smallestRouteSize() const;
    Solution snapshot() const;

private:
    struct Candidate {
        int politician = -1, position = 0, town = 0;
        double arrive = 0, start = 0, ratio = -1;
    };

    double returnTime(int p) const;
    void apply(const Candidate& c);
    void retime(int p);
    void refreshMaxShift();

    const Instance& inst_;
    std::vector<std::vector<Slot>> routes_;
    BookingTable booked_;
    std::vector<int> visits_;               // rallies held in each town so far
    std::vector<std::vector<char>> inRoute_;  // inRoute_[p][town]
};

double Search::returnTime(int p) const {
    const auto& r = routes_[p];
    if (r.empty()) return 0;
    return r.back().finish + inst_.travelTime(r.back().town, inst_.depotIndex());
}

void Search::refreshMaxShift() {
    for (int p = 0; p < inst_.k; ++p) {
        auto& r = routes_[p];
        double limit = inst_.horizon - returnTime(p);  // slack the trip home can absorb
        for (int i = static_cast<int>(r.size()) - 1; i >= 0; --i) {
            double clashSlack = booked_.nextOtherStart(r[i].town, r[i].start, p) - r[i].finish;
            r[i].maxShift = std::min(clashSlack, limit);
            limit = r[i].wait() + r[i].maxShift;
        }
    }
}

int Search::insertUntilFull() {
    int inserted = 0;
    while (true) {
        Candidate best;
        for (int p = 0; p < inst_.k; ++p) {
            const auto& r = routes_[p];
            int m = static_cast<int>(r.size());
            for (int c = 0; c < inst_.n(); ++c) {
                if (inRoute_[p][c]) continue;
                double gain = inst_.gain(c, visits_[c]);
                if (gain <= 0) continue;
                for (int pos = 0; pos <= m; ++pos) {
                    int prevTown = pos == 0 ? inst_.depotIndex() : r[pos - 1].town;
                    double prevFinish = pos == 0 ? 0.0 : r[pos - 1].finish;
                    double arrive = prevFinish + inst_.travelTime(prevTown, c);
                    double start = booked_.earliestFree(c, arrive, inst_.stay(c), p);
                    double finish = start + inst_.stay(c);

                    // How much later the next stop (a rally, or HQ) is reached, and how much it can absorb.
                    int nextTown = pos == m ? inst_.depotIndex() : r[pos].town;
                    double nextArrive = pos == m ? returnTime(p) : r[pos].arrive;
                    double absorb = pos == m ? inst_.horizon - returnTime(p) : r[pos].wait() + r[pos].maxShift;
                    double delay = finish + inst_.travelTime(c, nextTown) - nextArrive;
                    if (delay > absorb + kEps) continue;

                    double ratio = gain * gain / std::max(delay, kEps);
                    if (ratio > best.ratio) best = {p, pos, c, arrive, start, ratio};
                }
            }
        }
        if (best.politician < 0) return inserted;
        apply(best);
        ++inserted;
    }
}

void Search::apply(const Candidate& c) {
    auto& r = routes_[c.politician];
    double finish = c.start + inst_.stay(c.town);
    r.insert(r.begin() + c.position, Slot{c.town, c.arrive, c.start, finish, 0});
    booked_.add(c.town, c.start, finish, c.politician);
    ++visits_[c.town];
    inRoute_[c.politician][c.town] = 1;

    // Push later rallies back. The maxShift check guarantees none of them now clashes.
    for (std::size_t i = c.position + 1; i < r.size(); ++i) {
        double arrive = r[i - 1].finish + inst_.travelTime(r[i - 1].town, r[i].town);
        double start = std::max(arrive, r[i].start);
        r[i].arrive = arrive;
        if (start == r[i].start) break;  // absorbed by waiting time; the rest of the route is unchanged
        r[i].start = start;
        r[i].finish = start + inst_.stay(r[i].town);
        booked_.move(r[i].town, c.politician, r[i].start, r[i].finish);
    }
    refreshMaxShift();
}

void Search::retime(int p) {
    int cur = inst_.depotIndex();
    double t = 0;
    for (Slot& s : routes_[p]) {
        s.arrive = t + inst_.travelTime(cur, s.town);
        s.start = booked_.earliestFree(s.town, s.arrive, inst_.stay(s.town), p);
        s.finish = s.start + inst_.stay(s.town);
        booked_.move(s.town, p, s.start, s.finish);
        cur = s.town;
        t = s.finish;
    }
}

void Search::shake(int start, int count) {
    for (int p = 0; p < inst_.k; ++p) {
        auto& r = routes_[p];
        int m = static_cast<int>(r.size());
        if (m == 0) continue;
        std::vector<char> drop(m, 0);
        for (int i = 0; i < std::min(count, m); ++i) drop[(start + i) % m] = 1;

        std::vector<Slot> kept;
        for (int i = 0; i < m; ++i) {
            if (!drop[i]) {
                kept.push_back(r[i]);
                continue;
            }
            booked_.remove(r[i].town, p);
            --visits_[r[i].town];
            inRoute_[p][r[i].town] = 0;
        }
        r = std::move(kept);
    }
    // Each rally can only move earlier: its old start time is still free, so earliestFree never returns later.
    for (int p = 0; p < inst_.k; ++p) retime(p);
    refreshMaxShift();
}

double Search::reach() const {
    double total = 0;
    for (int c = 0; c < inst_.n(); ++c)
        for (int v = 0; v < visits_[c]; ++v) total += inst_.gain(c, v);
    return total;
}

int Search::smallestRouteSize() const {
    std::size_t smallest = std::numeric_limits<std::size_t>::max();
    for (const auto& r : routes_) smallest = std::min(smallest, r.size());
    return static_cast<int>(smallest);
}

Solution Search::snapshot() const {
    Solution sol;
    for (const auto& r : routes_) {
        std::vector<Visit> route;
        for (const Slot& s : r) route.push_back({s.town, s.arrive, s.start, s.finish});
        sol.routes.push_back(std::move(route));
    }
    return sol;
}

}  // namespace

Solution solveIls(const Instance& inst, const IlsParams& params, IlsStats* stats) {
    Search search(inst);
    search.insertUntilFull();
    Solution best = search.snapshot();
    double bestReach = search.reach();

    // Shake schedule from Vansteenwegen et al. (2009): the removal start moves along the route
    // and the removal length grows, resetting to 1 after an improvement or once it gets too long.
    const int maxRemove = std::max(1, inst.n() / (3 * inst.k));
    int removeStart = 0, removeCount = 1, sinceImprove = 0, iterations = 0, improvements = 0;
    while (iterations < params.maxIterations && sinceImprove < params.maxNoImprove) {
        ++iterations;
        search.shake(removeStart, removeCount);
        search.insertUntilFull();

        double reach = search.reach();
        if (reach > bestReach + kEps) {
            best = search.snapshot();
            bestReach = reach;
            removeCount = 1;
            sinceImprove = 0;
            ++improvements;
        } else {
            ++sinceImprove;
        }

        removeStart += removeCount;
        ++removeCount;
        int smallest = std::max(1, search.smallestRouteSize());
        removeStart %= smallest;
        if (removeCount > maxRemove) removeCount = 1;
    }
    if (stats) *stats = {iterations, improvements};
    return best;
}
