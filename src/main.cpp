// Command-line program: builds the instance, runs the planners, checks each plan
// and prints a results table. Run with --help to see the options.

#include "instance.hpp"
#include "planners.hpp"
#include "solution.hpp"

#include <chrono>
#include <cstdio>
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
            "  --k N                number of politicians (default 2)\n"
            "  --alpha A            reach of a repeat visit relative to the previous one (default 0.5)\n"
            "  --hours H            campaign time budget in hours (default 168)\n"
            "  --min-pop P          smallest town population considered (default 50000)\n"
            "  --max-towns N        keep only the N most populous towns (default: all)\n"
            "  --merge-km D         merge places within D km of a bigger one (default 5)\n"
            "  --borneo             include Sabah, Sarawak and Labuan (reached by flight)\n"
            "  --approach NAME      uncoordinated | sequential | ils | all (default all)\n"
            "  --ils-iters N        ILS iteration cap (default 5000)\n"
            "  --ils-no-improve N   ILS stops after N rounds without improvement (default 150)\n"
            "  --routes-dir DIR     write each approach's timed routes as CSV into DIR\n"
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
        else if (option == "--k") config.numPoliticians = stoi(nextValue(i, argc, argv));
        else if (option == "--alpha") config.alpha = stod(nextValue(i, argc, argv));
        else if (option == "--hours") config.campaignHours = stod(nextValue(i, argc, argv));
        else if (option == "--min-pop") config.minPopulation = stol(nextValue(i, argc, argv));
        else if (option == "--max-towns") config.maxTowns = stoi(nextValue(i, argc, argv));
        else if (option == "--merge-km") config.mergeKm = stod(nextValue(i, argc, argv));
        else if (option == "--borneo") config.includeBorneo = true;
        else if (option == "--approach") options.approach = nextValue(i, argc, argv);
        else if (option == "--ils-iters") options.ilsParams.maxIterations = stoi(nextValue(i, argc, argv));
        else if (option == "--ils-no-improve") options.ilsParams.maxNoImprove = stoi(nextValue(i, argc, argv));
        else if (option == "--routes-dir") options.routesDir = nextValue(i, argc, argv);
        else if (option == "--summary") options.summaryFile = nextValue(i, argc, argv);
        else if (option == "--list-towns") options.listTowns = true;
        else throw runtime_error("unknown option " + option);
    }
    return true;
}

static void printTowns(const Instance& inst) {
    for (int town = 0; town < inst.numTowns(); ++town) {
        printf("  %3d  %-30s pop %8ld  rally %.1fh  %.1fh from HQ\n", town, inst.towns[town].name.c_str(),
               inst.towns[town].population, inst.rallyHours(town), inst.travelTime(inst.hqIndex(), town));
    }
    printf("\n");
}

static void printTableHeader() {
    printf("%-14s %12s %7s %7s %8s %9s %8s %9s %10s %6s\n", "approach", "reach", "%max", "visits", "clashes",
           "travel_h", "wait_h", "fairness", "runtime_ms", "valid");
}

static void printTableRow(const string& approach, const Instance& inst, const Metrics& m, double runtimeMs) {
    double percentOfMax = 100 * m.reach / inst.maxPossibleReach();
    printf("%-14s %12.0f %6.1f%% %7d %8d %9.1f %8.1f %9.2f %10.1f %6s\n", approach.c_str(), m.reach, percentOfMax,
           m.visits, m.clashes, m.travelHours, m.waitHours, m.fairness(), runtimeMs, m.valid() ? "yes" : "NO");
    for (const string& error : m.errors) {
        printf("    ! %s\n", error.c_str());
    }
}

static void writeRoutesCsv(const string& path, const Instance& inst, const Solution& solution) {
    ofstream out(path);
    out << "politician,seq,geonameid,town,lat,lon,population,arrive_h,start_h,finish_h\n";
    for (int p = 0; p < (int)solution.routes.size(); ++p) {
        for (int i = 0; i < (int)solution.routes[p].size(); ++i) {
            const Visit& visit = solution.routes[p][i];
            const Town& town = inst.towns[visit.town];
            out << p << ',' << i << ',' << town.geonameId << ",\"" << town.name << "\"," << town.lat << ','
                << town.lon << ',' << town.population << ',' << visit.arrive << ',' << visit.start << ','
                << visit.finish << '\n';
        }
    }
}

static void appendSummaryCsv(const string& path, const string& approach, const Instance& inst, const Metrics& m,
                             double runtimeMs) {
    bool isNewFile = !ifstream(path).good();
    ofstream out(path, ios::app);
    if (isNewFile) {
        out << "approach,towns,k,alpha,hours,reach,max_reach,visits,clashes,travel_h,wait_h,fairness,valid,"
               "runtime_ms\n";
    }
    out << approach << ',' << inst.numTowns() << ',' << inst.numPoliticians << ',' << inst.alpha << ','
        << inst.campaignHours << ',' << m.reach << ',' << inst.maxPossibleReach() << ',' << m.visits << ','
        << m.clashes << ',' << m.travelHours << ',' << m.waitHours << ',' << m.fairness() << ','
        << (m.valid() ? 1 : 0) << ',' << runtimeMs << '\n';
}

static Solution runPlanner(const string& approach, const Instance& inst, const Options& options,
                           IlsStats& ilsStats) {
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
    } catch (const exception& e) {
        cerr << e.what() << "\n";
        printUsage();
        return 1;
    }

    printf("Instance: %d towns, %d politicians, alpha=%.2f, %.0f campaign hours, HQ=%s\n", inst.numTowns(),
           inst.numPoliticians, inst.alpha, inst.campaignHours, inst.hq.name.c_str());
    printf("Upper bound on reach (everyone visits everywhere): %.0f\n\n", inst.maxPossibleReach());
    if (options.listTowns) printTowns(inst);

    printTableHeader();
    IlsStats ilsStats;
    vector<string> approaches = {"uncoordinated", "sequential", "ils"};
    for (const string& approach : approaches) {
        if (options.approach != "all" && options.approach != approach) continue;

        auto startClock = chrono::steady_clock::now();
        Solution solution = runPlanner(approach, inst, options, ilsStats);
        auto endClock = chrono::steady_clock::now();
        double runtimeMs = chrono::duration<double, milli>(endClock - startClock).count();

        Metrics metrics = evaluate(inst, solution);
        printTableRow(approach, inst, metrics, runtimeMs);

        if (!options.routesDir.empty()) {
            writeRoutesCsv(options.routesDir + "/" + approach + "_routes.csv", inst, solution);
        }
        if (!options.summaryFile.empty()) {
            appendSummaryCsv(options.summaryFile, approach, inst, metrics, runtimeMs);
        }
    }

    if (options.approach == "all" || options.approach == "ils") {
        printf("\nILS: %d iterations, %d improvements\n", ilsStats.iterations, ilsStats.improvements);
    }
    return 0;
}
