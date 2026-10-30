// Turns the GeoNames file into a problem instance: picks the rally towns, the politicians'
// home bases, and works out the travel time between every pair of places.

#include "instance.hpp"

#include "geo.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace std;

// Column positions in MY.txt (see CS201_2026_project/Metadata.txt).
const int COL_ID = 0;
const int COL_ASCII_NAME = 2;
const int COL_LAT = 4;
const int COL_LON = 5;
const int COL_FEATURE_CLASS = 6;
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

// The geonameids in the exclusion list (first column; the header row is skipped).
static set<long> readExcludedIds(const string& path) {
    set<long> ids;
    if (path == "none") return ids;
    ifstream file(path);
    if (!file) throw runtime_error("cannot open exclusion list: " + path + " (use --exclude none to keep every place)");

    string line;
    getline(file, line);
    while (getline(file, line)) {
        string id = line.substr(0, line.find(','));
        if (!id.empty()) ids.insert(stol(id));
    }
    return ids;
}

// Reads every populated place that passes the filters. Also keeps every populated place with a
// known population in `allPlaces`, so a home base can be found even if it isn't a rally town.
// Excluded places are dropped before merging, as if GeoNames didn't list them.
static vector<Town> readCandidateTowns(const Config& config, vector<Town>& allPlaces) {
    ifstream file(config.dataPath);
    if (!file) throw runtime_error("cannot open data file: " + config.dataPath);
    set<long> excluded = readExcludedIds(config.excludePath);

    vector<Town> candidates;
    string line;
    while (getline(file, line)) {
        vector<string> fields = splitByTab(line);
        if ((int)fields.size() <= COL_POPULATION) continue;
        if (fields[COL_FEATURE_CLASS] != "P") continue;  // P = city, town or village

        Town town = parseTown(fields);
        if (town.population > 0) allPlaces.push_back(town);
        if (town.population < config.minPopulation) continue;
        if (town.inBorneo && !config.includeBorneo) continue;
        if (excluded.count(town.geonameId)) continue;
        candidates.push_back(town);
    }
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

static string lowercase(string text) {
    for (char& ch : text) ch = (char)tolower((unsigned char)ch);
    return text;
}

static string trim(const string& text) {
    size_t first = text.find_first_not_of(' ');
    size_t last = text.find_last_not_of(' ');
    return first == string::npos ? "" : text.substr(first, last - first + 1);
}

// The town called `name`: a rally town if there is one, otherwise the most populous place
// in the whole file with that name.
static Town findPlace(const string& name, const vector<Town>& towns, const vector<Town>& allPlaces) {
    string wanted = lowercase(name);
    for (const Town& town : towns) {
        if (lowercase(town.name) == wanted) return town;
    }
    const Town* best = nullptr;
    for (const Town& place : allPlaces) {
        if (lowercase(place.name) == wanted && (best == nullptr || place.population > best->population)) {
            best = &place;
        }
    }
    if (best == nullptr) throw runtime_error("no populated place called \"" + name + "\" for a home base");
    return *best;
}

// k distinct rally towns, picked at random. Uses our own Fisher-Yates shuffle on mt19937's raw
// output, because std::shuffle can give different orders on different compilers.
static vector<Town> randomHomes(const vector<Town>& towns, int k, unsigned seed) {
    vector<int> order(towns.size());
    for (int i = 0; i < (int)order.size(); ++i) order[i] = i;
    mt19937 random(seed);
    for (int i = (int)order.size() - 1; i > 0; --i) {
        int j = (int)(random() % (unsigned)(i + 1));
        swap(order[i], order[j]);
    }
    vector<Town> homes;
    for (int p = 0; p < k; ++p) homes.push_back(towns[order[p % order.size()]]);
    return homes;
}

// Politician p's home is the p-th name in the list (starting again from the top if there are
// more politicians than names).
static vector<Town> homesFromNames(const vector<string>& names, int k, const vector<Town>& towns,
                                   const vector<Town>& allPlaces) {
    vector<Town> homes;
    for (int p = 0; p < k; ++p) homes.push_back(findPlace(names[p % names.size()], towns, allPlaces));
    return homes;
}

static vector<Town> chooseHomes(const Config& config, const vector<Town>& towns, const vector<Town>& allPlaces) {
    if (config.starts == "random") return randomHomes(towns, config.numPoliticians, config.seed);
    if (config.starts == "fixed") return homesFromNames(DEFAULT_HOMES, config.numPoliticians, towns, allPlaces);

    vector<string> names;
    stringstream list(config.starts);
    string name;
    while (getline(list, name, ',')) {
        if (!trim(name).empty()) names.push_back(trim(name));
    }
    if (names.empty()) throw runtime_error("--starts needs fixed, random, or a list of town names");
    return homesFromNames(names, config.numPoliticians, towns, allPlaces);
}

double Instance::maxPossibleReach() const {
    double total = 0;
    for (const Town& town : towns) total += (double)town.population * numPoliticians;
    return total;
}

Instance buildInstance(const Config& config) {
    vector<Town> allPlaces;
    vector<Town> candidates = readCandidateTowns(config, allPlaces);
    if (config.numPoliticians < 1) throw runtime_error("need at least one politician");

    Instance inst;
    inst.towns = mergeNearbyPlaces(candidates, config);
    if (inst.towns.empty()) throw runtime_error("no towns pass the filters");
    inst.homes = chooseHomes(config, inst.towns, allPlaces);
    for (Town& home : inst.homes) {
        home.name = "Home " + home.name;
        home.rallyHours = 0;
    }
    inst.numPoliticians = config.numPoliticians;
    inst.alpha = config.alpha;
    inst.clock = config.clock;
    inst.tauRepeatHours = config.tauRepeatHours;
    inst.tauPollHours = config.tauPollHours;
    inst.clashPenalty = config.clashPenalty;

    // All towns, then one home base per politician.
    vector<Town> places = inst.towns;
    places.insert(places.end(), inst.homes.begin(), inst.homes.end());
    int numPlaces = places.size();

    inst.travelHours.assign(numPlaces, vector<double>(numPlaces, 0.0));
    for (int from = 0; from < numPlaces; ++from) {
        for (int to = 0; to < numPlaces; ++to) {
            if (from != to) inst.travelHours[from][to] = travelHoursBetween(places[from], places[to], config);
        }
    }
    return inst;
}
