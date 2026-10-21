#pragma once

#include <cmath>
#include <string>
#include <vector>

// A town where a rally can be held.
struct Town {
    long geonameId = 0;
    std::string name;
    double lat = 0;
    double lon = 0;
    long population = 0;
    double rallyHours = 0;   // how long one rally here takes
    bool inBorneo = false;   // Sabah, Sarawak or Labuan: can only be reached from the peninsula by plane
};

// Settings for which towns to use and how to estimate travel time.
struct Config {
    std::string dataPath = "CS201_2026_project/MY.txt";
    int numPoliticians = 2;
    double alpha = 0.5;          // each repeat rally in a town reaches alpha times as many people as the one before
    double campaignHours = 168;  // everyone must be back at HQ by then
    long minPopulation = 50000;
    int maxTowns = 0;            // 0 means no limit
    double mergeKm = 5;          // a place this close to a bigger town counts as part of it
    bool includeBorneo = false;

    double roadFactor = 1.3;     // roads are roughly 30% longer than a straight line
    double driveSpeedKmh = 70;
    double flightOverheadHours = 2.5;  // getting to the airport, check-in and boarding
    double flightSpeedKmh = 700;
};

// The problem the planners solve.
// Towns are numbered 0 to numTowns() - 1. Party HQ uses one extra index, hqIndex().
struct Instance {
    std::vector<Town> towns;
    Town hq;
    int numPoliticians = 2;
    double alpha = 0.5;
    double campaignHours = 168;
    std::vector<std::vector<double>> travelHours;  // travelHours[from][to], HQ included

    int numTowns() const { return (int)towns.size(); }
    int hqIndex() const { return numTowns(); }
    double travelTime(int from, int to) const { return travelHours[from][to]; }
    double rallyHours(int town) const { return towns[town].rallyHours; }

    // How many people a rally in `town` reaches if `earlierRallies` were already held there.
    double rallyReach(int town, int earlierRallies) const {
        return towns[town].population * std::pow(alpha, earlierRallies);
    }

    // Reach if every politician held a rally in every town. No real plan can do better.
    double maxPossibleReach() const;
};

Instance buildInstance(const Config& config);
