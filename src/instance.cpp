// Turns the GeoNames file into a problem instance: picks the rally towns, finds HQ,
// and works out the travel time between every pair of places.

#include "instance.hpp"

#include "geo.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

using namespace std;

// Column positions in MY.txt (see CS201_2026_project/Metadata.txt).
const int COL_ID = 0;
const int COL_ASCII_NAME = 2;
const int COL_LAT = 4;
const int COL_LON = 5;
const int COL_FEATURE_CLASS = 6;
const int COL_FEATURE_CODE = 7;
const int COL_POPULATION = 14;

// Sabah, Sarawak and Labuan are east of this longitude; the peninsula is west of it.
const double BORNEO_MIN_LONGITUDE = 109.0;

static vector<string> splitByTab(const string& line) {
    vector<string> fields;
    string field;
    for (char ch : line) {
        if (ch == '\t') {
            fields.push_back(field);
            field.clear();
        } else {
            field += ch;
        }
    }
    fields.push_back(field);
    return fields;
}

// Bigger towns get longer rallies: 1 hour at 10k people, 2 hours at 100k, 3 hours at 1M or more.
static double rallyHoursFor(long population) {
    if (population < 10000) return 1.0;
    double hours = 1.0 + log10(population / 10000.0);
    return min(hours, 3.0);
}

static Town parseTown(const vector<string>& fields) {
    Town town;
    town.geonameId = stol(fields[COL_ID]);
    town.name = fields[COL_ASCII_NAME];
    town.lat = stod(fields[COL_LAT]);
    town.lon = stod(fields[COL_LON]);
    town.population = fields[COL_POPULATION].empty() ? 0 : stol(fields[COL_POPULATION]);
    town.inBorneo = town.lon > BORNEO_MIN_LONGITUDE;
    town.rallyHours = rallyHoursFor(town.population);
    return town;
}

// Reads every populated place that passes the filters. Also finds the capital, which is used as HQ.
static vector<Town> readCandidateTowns(const Config& config, Town& hq) {
    ifstream file(config.dataPath);
    if (!file) throw runtime_error("cannot open data file: " + config.dataPath);

    vector<Town> candidates;
    bool foundHq = false;
    string line;
    while (getline(file, line)) {
        vector<string> fields = splitByTab(line);
        if ((int)fields.size() <= COL_POPULATION) continue;
        if (fields[COL_FEATURE_CLASS] != "P") continue;  // P = city, town or village

        Town town = parseTown(fields);
        if (fields[COL_FEATURE_CODE] == "PPLC") {  // PPLC = national capital
            hq = town;
            foundHq = true;
        }
        if (town.population < config.minPopulation) continue;
        if (town.inBorneo && !config.includeBorneo) continue;
        candidates.push_back(town);
    }
    if (!foundHq) throw runtime_error("no capital (PPLC) found to use as HQ");
    return candidates;
}

// GeoNames lists a city and the districts inside it as separate places, each with its own
// population, so adding them up would count people twice. We go from the biggest place down
// and skip anything within mergeKm of a town we already kept.
static vector<Town> mergeNearbyPlaces(vector<Town> candidates, const Config& config) {
    sort(candidates.begin(), candidates.end(), [](const Town& a, const Town& b) {
        if (a.population != b.population) return a.population > b.population;
        return a.geonameId < b.geonameId;
    });

    vector<Town> kept;
    for (const Town& candidate : candidates) {
        bool tooClose = false;
        for (const Town& town : kept) {
            if (haversineKm(candidate.lat, candidate.lon, town.lat, town.lon) < config.mergeKm) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) kept.push_back(candidate);
        if (config.maxTowns > 0 && (int)kept.size() == config.maxTowns) break;
    }
    return kept;
}

static double travelHoursBetween(const Town& a, const Town& b, const Config& config) {
    double km = haversineKm(a.lat, a.lon, b.lat, b.lon);
    bool needsFlight = a.inBorneo != b.inBorneo;
    if (needsFlight) {
        return config.flightOverheadHours + km / config.flightSpeedKmh;
    }
    return km * config.roadFactor / config.driveSpeedKmh;
}

double Instance::maxPossibleReach() const {
    double total = 0;
    for (int town = 0; town < numTowns(); ++town) {
        for (int rally = 0; rally < numPoliticians; ++rally) {
            total += rallyReach(town, rally);
        }
    }
    return total;
}

Instance buildInstance(const Config& config) {
    Town hq;
    vector<Town> candidates = readCandidateTowns(config, hq);

    Instance inst;
    inst.towns = mergeNearbyPlaces(candidates, config);
    inst.hq = hq;
    inst.hq.name = "HQ " + hq.name;
    inst.hq.rallyHours = 0;
    inst.numPoliticians = config.numPoliticians;
    inst.alpha = config.alpha;
    inst.campaignHours = config.campaignHours;

    // All towns plus HQ at the end.
    vector<Town> places = inst.towns;
    places.push_back(inst.hq);
    int numPlaces = places.size();

    inst.travelHours.assign(numPlaces, vector<double>(numPlaces, 0.0));
    for (int from = 0; from < numPlaces; ++from) {
        for (int to = 0; to < numPlaces; ++to) {
            if (from != to) inst.travelHours[from][to] = travelHoursBetween(places[from], places[to], config);
        }
    }
    return inst;
}
