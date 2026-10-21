#pragma once

#include <cmath>
#include <string>
#include <vector>

struct Town {
    long geonameId = 0;
    std::string name;
    double lat = 0, lon = 0;
    long population = 0;
    double stayHours = 0;  // length of one rally here
    bool borneo = false;   // Sabah / Sarawak / Labuan: reachable from the peninsula only by air
};

struct Config {
    std::string dataPath = "CS201_2026_project/MY.txt";
    int politicians = 2;
    double alpha = 0.5;           // the v-th visit to a town earns population * alpha^(v-1)
    double horizonHours = 168;    // campaign time budget; every route must be home by then
    long minPopulation = 50000;
    int maxTowns = 0;             // keep only the N most populous towns (0 = no limit)
    double mergeKm = 5;           // places this close to a bigger one are treated as part of it
    bool includeBorneo = false;
    double roadFactor = 1.3;      // road distance / straight-line distance
    double driveSpeedKmh = 70;
    double flightOverheadHours = 2.5;  // airport transfers, check-in, boarding
    double flightSpeedKmh = 700;
};

// Rally towns are indices 0..n-1; the party HQ (start and end of every route) is index n.
struct Instance {
    std::vector<Town> towns;
    Town depot;
    int k = 2;
    double alpha = 0.5;
    double horizon = 168;
    std::vector<std::vector<double>> travel;  // hours, (n+1) x (n+1)

    int n() const { return static_cast<int>(towns.size()); }
    int depotIndex() const { return n(); }
    double travelTime(int from, int to) const { return travel[from][to]; }
    double stay(int town) const { return towns[town].stayHours; }

    // Reach earned by a visit to `town` that already had `priorVisits` visits.
    double gain(int town, int priorVisits) const {
        return towns[town].population * std::pow(alpha, priorVisits);
    }

    // Reach if every politician rallied in every town: an upper bound on any solution.
    double maxReach() const;
};

Instance buildInstance(const Config& cfg);
