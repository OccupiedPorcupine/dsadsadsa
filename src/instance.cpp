#include "instance.hpp"

#include "geo.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace {

constexpr double kBorneoMinLongitude = 109.0;  // Sabah, Sarawak and Labuan all lie east of this

// GeoNames columns (see CS201_2026_project/Metadata.txt).
constexpr int kColId = 0, kColAsciiName = 2, kColLat = 4, kColLon = 5;
constexpr int kColFeatureClass = 6, kColFeatureCode = 7, kColPopulation = 14;

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t begin = 0;
    while (true) {
        std::size_t end = line.find('\t', begin);
        fields.push_back(line.substr(begin, end - begin));
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return fields;
}

// 1h for a town of 10k, 2h for 100k, capped at 3h for 1M and above.
double stayHoursFor(long population) {
    double hours = 1.0 + std::log10(std::max(population, 10000L) / 10000.0);
    return std::min(hours, 3.0);
}

double travelHours(const Town& a, const Town& b, const Config& cfg) {
    double km = haversineKm(a.lat, a.lon, b.lat, b.lon);
    if (a.borneo != b.borneo) return cfg.flightOverheadHours + km / cfg.flightSpeedKmh;
    return km * cfg.roadFactor / cfg.driveSpeedKmh;
}

}  // namespace

double Instance::maxReach() const {
    double total = 0;
    for (int c = 0; c < n(); ++c)
        for (int v = 0; v < k; ++v) total += gain(c, v);
    return total;
}

Instance buildInstance(const Config& cfg) {
    std::ifstream in(cfg.dataPath);
    if (!in) throw std::runtime_error("cannot open data file: " + cfg.dataPath);

    std::vector<Town> candidates;
    Town depot;
    bool haveDepot = false;
    std::string line;
    while (std::getline(in, line)) {
        std::vector<std::string> f = splitTabs(line);
        if (f.size() <= kColPopulation || f[kColFeatureClass] != "P") continue;

        Town t;
        t.geonameId = std::stol(f[kColId]);
        t.name = f[kColAsciiName];
        t.lat = std::stod(f[kColLat]);
        t.lon = std::stod(f[kColLon]);
        t.population = f[kColPopulation].empty() ? 0 : std::stol(f[kColPopulation]);
        t.borneo = t.lon > kBorneoMinLongitude;
        t.stayHours = stayHoursFor(t.population);

        if (f[kColFeatureCode] == "PPLC") {  // national capital: party HQ
            depot = t;
            haveDepot = true;
        }
        if (t.population < cfg.minPopulation) continue;
        if (t.borneo && !cfg.includeBorneo) continue;
        candidates.push_back(t);
    }
    if (!haveDepot) throw std::runtime_error("no capital (PPLC) found to use as HQ");

    // GeoNames lists cities and the districts inside them separately, each with its own
    // population. Keep the largest place and drop anything within mergeKm of a kept one.
    std::sort(candidates.begin(), candidates.end(), [](const Town& a, const Town& b) {
        return a.population != b.population ? a.population > b.population : a.geonameId < b.geonameId;
    });
    Instance inst;
    for (const Town& c : candidates) {
        bool absorbed = std::any_of(inst.towns.begin(), inst.towns.end(), [&](const Town& kept) {
            return haversineKm(c.lat, c.lon, kept.lat, kept.lon) < cfg.mergeKm;
        });
        if (!absorbed) inst.towns.push_back(c);
        if (cfg.maxTowns > 0 && inst.n() == cfg.maxTowns) break;
    }

    depot.name = "HQ " + depot.name;
    depot.stayHours = 0;
    inst.depot = depot;
    inst.k = cfg.politicians;
    inst.alpha = cfg.alpha;
    inst.horizon = cfg.horizonHours;

    int size = inst.n() + 1;
    auto place = [&](int i) -> const Town& { return i == inst.depotIndex() ? inst.depot : inst.towns[i]; };
    inst.travel.assign(size, std::vector<double>(size, 0.0));
    for (int i = 0; i < size; ++i)
        for (int j = 0; j < size; ++j)
            if (i != j) inst.travel[i][j] = travelHours(place(i), place(j), cfg);
    return inst;
}
