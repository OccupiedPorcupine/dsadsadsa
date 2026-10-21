#include "solution.hpp"

#include "booking.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace {

constexpr double kTolerance = 1e-6;

std::string describe(int p, int seq, const std::string& what) {
    std::ostringstream out;
    out << "politician " << p << " visit " << seq << ": " << what;
    return out.str();
}

}  // namespace

double Metrics::fairness() const {
    if (reachByPolitician.empty()) return 1.0;
    auto [lo, hi] = std::minmax_element(reachByPolitician.begin(), reachByPolitician.end());
    return *hi > 0 ? *lo / *hi : 1.0;
}

Metrics evaluate(const Instance& inst, const Solution& sol) {
    Metrics m;
    int k = static_cast<int>(sol.routes.size());
    m.reachByPolitician.assign(k, 0.0);
    m.returnTime.assign(k, 0.0);

    struct Rally { double start, finish; int politician; };
    std::vector<std::vector<Rally>> ralliesAt(inst.n());

    for (int p = 0; p < k; ++p) {
        std::vector<char> seen(inst.n(), 0);
        int cur = inst.depotIndex();
        double t = 0;
        for (int i = 0; i < static_cast<int>(sol.routes[p].size()); ++i) {
            const Visit& v = sol.routes[p][i];
            if (seen[v.town]) m.errors.push_back(describe(p, i, "repeats town " + inst.towns[v.town].name));
            seen[v.town] = 1;
            double leg = inst.travelTime(cur, v.town);
            if (v.arrive + kTolerance < t + leg) m.errors.push_back(describe(p, i, "arrives before it could"));
            if (v.start + kTolerance < v.arrive) m.errors.push_back(describe(p, i, "starts before arriving"));
            if (std::fabs(v.finish - v.start - inst.stay(v.town)) > kTolerance)
                m.errors.push_back(describe(p, i, "rally length is wrong"));
            m.travelHours += leg;
            m.waitHours += v.start - (t + leg);
            ralliesAt[v.town].push_back({v.start, v.finish, p});
            ++m.visits;
            cur = v.town;
            t = v.finish;
        }
        m.travelHours += inst.travelTime(cur, inst.depotIndex());
        m.returnTime[p] = t + inst.travelTime(cur, inst.depotIndex());
        if (m.returnTime[p] > inst.horizon + kTolerance)
            m.errors.push_back(describe(p, -1, "returns to HQ after the campaign ends"));
    }

    for (int c = 0; c < inst.n(); ++c) {
        auto& rallies = ralliesAt[c];
        std::sort(rallies.begin(), rallies.end(), [](const Rally& a, const Rally& b) { return a.start < b.start; });
        for (int v = 0; v < static_cast<int>(rallies.size()); ++v) {
            double g = inst.gain(c, v);
            m.reach += g;
            m.reachByPolitician[rallies[v].politician] += g;
            for (int w = v + 1; w < static_cast<int>(rallies.size()); ++w)
                if (rallies[w].start < rallies[v].finish - kTolerance) ++m.clashes;
        }
    }
    return m;
}
