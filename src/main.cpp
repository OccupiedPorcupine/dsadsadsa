// Command-line program: builds the instance, runs the planners, checks each plan
// and prints a results table. Run with --help to see the options.

#include "instance.hpp"
#include "planners.hpp"
#include "solution.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

struct Options {
    Config config;
    IlsParams ilsParams;
    string approach = "all";  // which planner(s) to run
    string routesDir;         // if set, write <approach>_routes.csv into this folder
    string summaryFile;       // if set, add one CSV row per approach to this file
    bool listTowns = false;
};

static void printUsage() {
    cerr << "usage: campaign [options]\n"
            "  --data PATH          GeoNames file (default CS201_2026_project/MY.txt)\n"
            "  --exclude PATH       places to drop because they double-count a city (default\n"
            "                       data/excluded_places.csv; none to keep every place)\n"
            "  --k N                number of politicians (default 2)\n"
            "  --alpha A            reach of an immediate repeat rally relative to the one before (default 0.5)\n"
            "  --days D             length of the campaign in days (default 14)\n"
            "  --day-hours H        campaigning hours per day; rallies and travel only happen then (default 16)\n"
            "  --day-start H        wall-clock hour the daily window opens (default 6, i.e. 06:00)\n"
            "  --starts S           home bases: fixed (default list), random, weighted (random, with\n"
            "                       chances proportional to population), or \"Town A,Town B,...\"\n"
            "  --seed N             random seed for --starts random or weighted (default 1)\n"
            "  --tau-repeat H       hours for the repeat penalty to fade (default 48)\n"
            "  --tau-poll H         hours for early rallies to be forgotten by polling day (default 336)\n"
            "  --clash-penalty B    share of reach a fully overlapped rally loses (default 0.5)\n"
            "  --min-pop P          smallest town population considered (default 50000)\n"
            "  --max-towns N        keep only the N most populous towns (default: all)\n"
            "  --merge-km D         merge places within D km of a bigger one (default 5)\n"
            "  --borneo             include Sabah, Sarawak and Labuan (reached by flight)\n"
            "  --approach NAME      nearest | nearest-budget | uncoordinated | sequential | ils | all (default all)\n"
            "  --ils-iters N        ILS iteration cap (default 5000)\n"
            "  --ils-no-improve N   ILS stops after N rounds without improvement (default 150)\n"
            "  --ils-sa             ILS keeps a worse plan only with probability exp(change / T), as in\n"
            "                       SAILS; otherwise it goes back to the plan before the shake (default off)\n"
            "  --ils-sa-temp F      starting T, as a share of the average rally's reach (default 0.5)\n"
            "  --ils-sa-cooling C   T is multiplied by C every round (default 0.99)\n"
            "  --ils-sa-restart N   go back to the best plan after N rounds without improvement (default 50)\n"
            "  --ils-sa-seed N      random seed for the acceptance rule (default 1)\n"
            "  --routes-dir DIR     write the towns, settings and each approach's timed routes as CSV into DIR\n"
            "  --summary FILE       append results as CSV rows to FILE\n"
            "  --list-towns         print the towns in the instance\n";
}

// Returns the value that follows option argv[i], and moves i past it.
static string nextValue(int& i, int argc, char** argv) {
    if (i + 1 >= argc) throw runtime_error(string("missing value for ") + argv[i]);
    i++;
    return argv[i];
}

// Returns false if the user asked for --help.
static bool parseOptions(int argc, char** argv, Options& options) {
    Config& config = options.config;
    for (int i = 1; i < argc; ++i) {
        string option = argv[i];
        if (option == "--help" || option == "-h") return false;
        else if (option == "--data") config.dataPath = nextValue(i, argc, argv);
        else if (option == "--exclude") config.excludePath = nextValue(i, argc, argv);
        else if (option == "--k") config.numPoliticians = stoi(nextValue(i, argc, argv));
        else if (option == "--alpha") config.alpha = stod(nextValue(i, argc, argv));
        else if (option == "--days") config.clock.days = stoi(nextValue(i, argc, argv));
        else if (option == "--day-hours") config.clock.dayHours = stod(nextValue(i, argc, argv));
        else if (option == "--day-start") config.clock.dayStartHour = stod(nextValue(i, argc, argv));
        else if (option == "--starts") config.starts = nextValue(i, argc, argv);
        else if (option == "--seed") config.seed = (unsigned)stoul(nextValue(i, argc, argv));
        else if (option == "--tau-repeat") config.tauRepeatHours = stod(nextValue(i, argc, argv));
        else if (option == "--tau-poll") config.tauPollHours = stod(nextValue(i, argc, argv));
        else if (option == "--clash-penalty") config.clashPenalty = stod(nextValue(i, argc, argv));
        else if (option == "--min-pop") config.minPopulation = stol(nextValue(i, argc, argv));
        else if (option == "--max-towns") config.maxTowns = stoi(nextValue(i, argc, argv));
        else if (option == "--merge-km") config.mergeKm = stod(nextValue(i, argc, argv));
        else if (option == "--borneo") config.includeBorneo = true;
        else if (option == "--approach") options.approach = nextValue(i, argc, argv);
        else if (option == "--ils-iters") options.ilsParams.maxIterations = stoi(nextValue(i, argc, argv));
        else if (option == "--ils-no-improve") options.ilsParams.maxNoImprove = stoi(nextValue(i, argc, argv));
        else if (option == "--ils-sa") options.ilsParams.annealing = true;
        else if (option == "--ils-sa-temp") options.ilsParams.startTemperature = stod(nextValue(i, argc, argv));
        else if (option == "--ils-sa-cooling") options.ilsParams.cooling = stod(nextValue(i, argc, argv));
        else if (option == "--ils-sa-restart") options.ilsParams.restartAfter = stoi(nextValue(i, argc, argv));
        else if (option == "--ils-sa-seed") options.ilsParams.seed = (unsigned)stoul(nextValue(i, argc, argv));
        else if (option == "--routes-dir") options.routesDir = nextValue(i, argc, argv);
        else if (option == "--summary") options.summaryFile = nextValue(i, argc, argv);
        else if (option == "--list-towns") options.listTowns = true;
        else throw runtime_error("unknown option " + option);
    }
    if (config.clock.days < 1 || config.clock.dayHours < 3 || config.clock.dayHours > 24) {
        throw runtime_error("need at least 1 day, and between 3 and 24 campaigning hours a day");
    }
    if (config.tauRepeatHours <= 0 || config.tauPollHours <= 0) throw runtime_error("tau values must be positive");
    // Above 1, or with a negative beta, one rally could reach more than the town's population,
    // which breaks the % max bound and ILS's pruning.
    if (config.alpha < 0 || config.alpha > 1) throw runtime_error("--alpha must be between 0 and 1");
    if (config.clashPenalty < 0 || config.clashPenalty > 1) throw runtime_error("--clash-penalty must be between 0 and 1");
    const IlsParams& ils = options.ilsParams;
    if (ils.startTemperature < 0 || ils.cooling <= 0 || ils.cooling > 1 || ils.restartAfter < 1) {
        throw runtime_error("need --ils-sa-temp >= 0, 0 < --ils-sa-cooling <= 1 and --ils-sa-restart >= 1");
    }
    const string& a = options.approach;
    if (a != "all" && a != "nearest" && a != "nearest-budget" && a != "uncoordinated" && a != "sequential" &&
        a != "ils") {
        throw runtime_error("unknown approach " + a);
    }
    return true;
}

static void printTowns(const Instance& inst) {
    for (int town = 0; town < inst.numTowns(); ++town) {
        printf("  %3d  %-30s pop %8ld  rally %.1fh  %.1fh from P1's home\n", town, inst.towns[town].name.c_str(),
               inst.towns[town].population, inst.rallyHours(town), inst.travelTime(inst.homeIndex(0), town));
    }
    printf("\n");
}

static void printTableHeader() {
    printf("%-14s %12s %7s %7s %8s %8s %9s %8s %9s %10s %6s\n", "approach", "reach", "%max", "visits", "clashes",
           "clash_h", "travel_h", "wait_h", "fairness", "runtime_ms", "valid");
}

static void printTableRow(const string& approach, const Instance& inst, const Metrics& m, double runtimeMs) {
    double percentOfMax = 100 * m.reach / inst.maxPossibleReach();
    printf("%-14s %12.0f %6.1f%% %7d %8d %8.1f %9.1f %8.1f %9.2f %10.1f %6s\n", approach.c_str(), m.reach,
           percentOfMax, m.visits, m.clashes, m.clashHours, m.travelHours, m.waitHours, m.fairness(), runtimeMs,
           m.valid() ? "yes" : "NO");
    for (const string& error : m.errors) {
        printf("    ! %s\n", error.c_str());
    }
}

// One row per rally. Times are active hours; day is 1-based. return_h (when that politician gets
// back home) repeats on each of their rows. clash is 1 if the rally overlaps someone else's, as
// found by the validator: a planner's own flag misses overlaps it didn't plan, like the baseline's.
static void writeRoutesCsv(const string& path, const Instance& inst, const Solution& solution,
                           const Metrics& metrics) {
    ofstream out(path);
    out << "politician,seq,geonameid,town,lat,lon,population,arrive_h,start_h,finish_h,day,clash,reach,return_h\n";
    for (int p = 0; p < (int)solution.routes.size(); ++p) {
        for (int i = 0; i < (int)solution.routes[p].size(); ++i) {
            const Visit& visit = solution.routes[p][i];
            const Town& town = inst.towns[visit.town];
            out << p << ',' << i << ',' << town.geonameId << ",\"" << town.name << "\"," << town.lat << ','
                << town.lon << ',' << town.population << ',' << visit.arrive << ',' << visit.start << ','
                << visit.finish << ',' << inst.clock.dayOf(visit.start) + 1 << ',' << (metrics.visitClash[p][i] ? 1 : 0) << ','
                << metrics.visitReach[p][i] << ',' << metrics.returnTime[p] << '\n';
        }
    }
}

// Every town in the instance (visited or not) plus the run settings, so other tools
// can make sense of the routes without re-reading the GeoNames file.
static void writeInstanceCsv(const string& dir, const Instance& inst) {
    ofstream towns(dir + "/towns.csv");
    towns << "geonameid,town,lat,lon,population,rally_h\n";
    for (const Town& town : inst.towns) {
        towns << town.geonameId << ",\"" << town.name << "\"," << town.lat << ',' << town.lon << ','
              << town.population << ',' << town.rallyHours << '\n';
    }

    ofstream meta(dir + "/meta.csv");
    meta << "key,value\n";
    meta << "politicians," << inst.numPoliticians << '\n';
    meta << "alpha," << inst.alpha << '\n';
    meta << "campaign_hours," << inst.campaignHours() << '\n';
    meta << "days," << inst.clock.days << '\n';
    meta << "day_hours," << inst.clock.dayHours << '\n';
    meta << "day_start," << inst.clock.dayStartHour << '\n';
    meta << "tau_repeat," << inst.tauRepeatHours << '\n';
    meta << "tau_poll," << inst.tauPollHours << '\n';
    meta << "clash_penalty," << inst.clashPenalty << '\n';
    meta << "max_reach," << inst.maxPossibleReach() << '\n';

    ofstream homes(dir + "/homes.csv");
    homes << "politician,name,lat,lon\n";
    for (int p = 0; p < inst.numPoliticians; ++p) {
        const Town& home = inst.homes[p];
        homes << p << ",\"" << home.name << "\"," << home.lat << ',' << home.lon << '\n';
    }
}

static string homeNames(const Instance& inst) {
    string names;
    for (int p = 0; p < inst.numPoliticians; ++p) {
        if (p > 0) names += "; ";
        names += inst.homes[p].name.substr(5);  // drop the "Home " prefix
    }
    return names;
}

static void appendSummaryCsv(const string& path, const string& approach, const Instance& inst, const Metrics& m,
                             double runtimeMs) {
    bool isNewFile = !ifstream(path).good();
    ofstream out(path, ios::app);
    if (isNewFile) {
        out << "approach,towns,k,alpha,days,day_hours,homes,tau_repeat,tau_poll,clash_penalty,reach,max_reach,"
               "visits,clashes,clash_h,travel_h,wait_h,fairness,valid,runtime_ms\n";
    }
    out << approach << ',' << inst.numTowns() << ',' << inst.numPoliticians << ',' << inst.alpha << ','
        << inst.clock.days << ',' << inst.clock.dayHours << ",\"" << homeNames(inst) << "\","
        << inst.tauRepeatHours << ',' << inst.tauPollHours << ',' << inst.clashPenalty << ',' << m.reach << ','
        << inst.maxPossibleReach() << ',' << m.visits << ',' << m.clashes << ',' << m.clashHours << ','
        << m.travelHours << ',' << m.waitHours << ',' << m.fairness() << ',' << (m.valid() ? 1 : 0) << ','
        << runtimeMs << '\n';
}

static Solution runPlanner(const string& approach, const Instance& inst, const Options& options,
                           IlsStats& ilsStats) {
    if (approach == "nearest") return solveNearestNeighbour(inst);
    if (approach == "nearest-budget") return solveBudgetedNearestNeighbour(inst);
    if (approach == "uncoordinated") return solveUncoordinated(inst);
    if (approach == "sequential") return solveSequential(inst);
    return solveIls(inst, options.ilsParams, &ilsStats);
}

int main(int argc, char** argv) {
    Options options;
    Instance inst;
    try {
        if (!parseOptions(argc, argv, options)) {
            printUsage();
            return 0;
        }
        inst = buildInstance(options.config);

        // Create the output folders up front. Otherwise the CSV writers fail silently and the
        // run looks fine but leaves no files.
        if (!options.routesDir.empty()) filesystem::create_directories(options.routesDir);
        if (!options.summaryFile.empty()) {
            filesystem::path folder = filesystem::path(options.summaryFile).parent_path();
            if (!folder.empty()) filesystem::create_directories(folder);
        }
    } catch (const exception& e) {
        cerr << e.what() << "\n";
        printUsage();
        return 1;
    }

    printf("Instance: %d towns, %d politicians, alpha=%.2f, %d days x %.0f h (from %02.0f:00) = %.0f active hours\n",
           inst.numTowns(), inst.numPoliticians, inst.alpha, inst.clock.days, inst.clock.dayHours,
           inst.clock.dayStartHour, inst.campaignHours());
    printf("Homes: %s\n", homeNames(inst).c_str());
    printf("Decay: tauRepeat=%.0f h, tauPoll=%.0f h; clash penalty beta=%.2f\n", inst.tauRepeatHours,
           inst.tauPollHours, inst.clashPenalty);
    printf("Upper bound on reach (everyone rallies everywhere, full crowd): %.0f\n\n", inst.maxPossibleReach());
    if (options.listTowns) printTowns(inst);
    if (!options.routesDir.empty()) writeInstanceCsv(options.routesDir, inst);

    printTableHeader();
    IlsStats ilsStats;
    vector<string> approaches = {"nearest", "nearest-budget", "uncoordinated", "sequential", "ils"};
    for (const string& approach : approaches) {
        if (options.approach != "all" && options.approach != approach) continue;

        auto startClock = chrono::steady_clock::now();
        Solution solution = runPlanner(approach, inst, options, ilsStats);
        auto endClock = chrono::steady_clock::now();
        double runtimeMs = chrono::duration<double, milli>(endClock - startClock).count();

        Metrics metrics = evaluate(inst, solution);
        printTableRow(approach, inst, metrics, runtimeMs);
        if (approach == "nearest") {
            // The full tour ignores the campaign length, so say how long it actually needs.
            double longest = *max_element(metrics.returnTime.begin(), metrics.returnTime.end());
            printf("    full tour needs %.1f active hours (%.1f days of %.0f h) to get everyone home; the campaign "
                   "has %.0f\n",
                   longest, longest / inst.clock.dayHours, inst.clock.dayHours, inst.campaignHours());
        }

        if (!options.routesDir.empty()) {
            writeRoutesCsv(options.routesDir + "/" + approach + "_routes.csv", inst, solution, metrics);
        }
        if (!options.summaryFile.empty()) {
            appendSummaryCsv(options.summaryFile, approach, inst, metrics, runtimeMs);
        }
    }

    if (options.approach == "all" || options.approach == "ils") {
        printf("\nILS: %d iterations, %d improvements over two runs. Best reach from empty routes %.0f, "
               "from A's plan %.0f\n",
               ilsStats.iterations, ilsStats.improvements, ilsStats.reachFromEmpty, ilsStats.reachFromA);
    }
    return 0;
}
