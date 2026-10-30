#pragma once

#include <cmath>
#include <string>
#include <vector>

// Small tolerance for comparing times, so rounding errors don't count as clashes.
const double EPS = 1e-9;

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

// The campaign clock. Planners work in *active hours*: only the daily campaigning window counts,
// so nights don't appear on the clock at all. Active hour t is in day floor(t / dayHours).
// Travel can run across a night (stop at the end of one day, carry on next morning), but a rally
// has to fit inside one day.
struct Clock {
    int days = 14;
    double dayHours = 12;     // length of the daily window
    double dayStartHour = 8;  // wall-clock hour the window opens (08:00)

    double end() const { return days * dayHours; }

    // The day an activity starting at t belongs to. A start exactly at the boundary is next morning.
    int dayOf(double t) const { return (int)std::floor((t + EPS) / dayHours); }

    // When the window that a rally starting at t falls in closes.
    double dayEnd(double t) const { return (dayOf(t) + 1) * dayHours; }

    // Earliest start at or after t at which a rally of `duration` hours fits inside one day.
    double fitRally(double t, double duration) const {
        if (t + duration <= dayEnd(t) + EPS) return t;
        return dayEnd(t);
    }

    // Real hours since the campaign opened (08:00 on day 1), nights included. `atEnd` says t is
    // a finish time, so a rally ending exactly at 20:00 stays on that day.
    double wallHours(double t, bool atEnd = false) const {
        int day = atEnd ? (int)std::floor((t - EPS) / dayHours) : dayOf(t);
        if (day < 0) day = 0;
        return day * 24.0 + (t - day * dayHours);
    }
};

// Settings for which towns to use, where politicians start, and how reach is worked out.
struct Config {
    std::string dataPath = "CS201_2026_project/MY.txt";
    std::string excludePath = "data/excluded_places.csv";  // places that double-count a city; "none" to keep all
    int numPoliticians = 2;
    double alpha = 0.5;          // an immediate repeat rally in a town reaches alpha times as many people
    long minPopulation = 50000;
    int maxTowns = 0;            // 0 means no limit
    double mergeKm = 5;          // a place this close to a bigger town counts as part of it
    bool includeBorneo = false;

    // Home bases: "fixed" (the list below), "random" (seeded), or a comma-separated list of town names.
    std::string starts = "fixed";
    unsigned seed = 1;

    Clock clock;
    double tauRepeatHours = 48;  // how fast the repeat penalty wears off (wall-clock hours)
    double tauPollHours = 336;   // how fast early rallies are forgotten before polling day (wall-clock hours)
    double clashPenalty = 0.5;   // beta: a fully overlapped rally loses this share of its reach

    double roadFactor = 1.3;     // roads are roughly 30% longer than a straight line
    double driveSpeedKmh = 70;
    double flightOverheadHours = 2.5;  // getting to the airport, check-in and boarding
    double flightSpeedKmh = 700;
};

// The default home bases, used in order: politician p starts at the p-th one.
const std::vector<std::string> DEFAULT_HOMES = {
    "Kuala Lumpur", "George Town", "Johor Bahru", "Kota Bharu", "Ipoh", "Kuantan", "Alor Setar",
};

// The problem the planners solve.
// Towns are numbered 0 to numTowns() - 1. Politician p's home base uses the extra index homeIndex(p).
struct Instance {
    std::vector<Town> towns;
    std::vector<Town> homes;  // homes[p] is where politician p starts and must end
    int numPoliticians = 2;
    double alpha = 0.5;
    Clock clock;
    double tauRepeatHours = 48;
    double tauPollHours = 336;
    double clashPenalty = 0.5;
    std::vector<std::vector<double>> travelHours;  // travelHours[from][to], homes included

    int numTowns() const { return (int)towns.size(); }
    int homeIndex(int politician) const { return numTowns() + politician; }
    double campaignHours() const { return clock.end(); }
    double travelTime(int from, int to) const { return travelHours[from][to]; }
    double rallyHours(int town) const { return towns[town].rallyHours; }

    // Every politician rallying once in every town, each reaching the whole population.
    // No real plan can do better.
    double maxPossibleReach() const;
};

Instance buildInstance(const Config& config);
