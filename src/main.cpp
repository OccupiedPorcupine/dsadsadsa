#include "instance.hpp"
#include "planners.hpp"
#include "solution.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct Options {
    Config cfg;
    IlsParams ils;
    std::string approach = "all";
    std::string routesDir;    // write <approach>_routes.csv here
    std::string summaryFile;  // append one CSV row per approach
    bool listTowns = false;
};

void usage() {
    std::cerr <<
        "usage: campaign [options]\n"
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

bool parse(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) throw std::runtime_error("missing value for " + a);
            return argv[++i];
        };
        if (a == "--data") o.cfg.dataPath = next();
        else if (a == "--k") o.cfg.politicians = std::stoi(next());
        else if (a == "--alpha") o.cfg.alpha = std::stod(next());
        else if (a == "--hours") o.cfg.horizonHours = std::stod(next());
        else if (a == "--min-pop") o.cfg.minPopulation = std::stol(next());
        else if (a == "--max-towns") o.cfg.maxTowns = std::stoi(next());
        else if (a == "--merge-km") o.cfg.mergeKm = std::stod(next());
        else if (a == "--borneo") o.cfg.includeBorneo = true;
        else if (a == "--approach") o.approach = next();
        else if (a == "--ils-iters") o.ils.maxIterations = std::stoi(next());
        else if (a == "--ils-no-improve") o.ils.maxNoImprove = std::stoi(next());
        else if (a == "--routes-dir") o.routesDir = next();
        else if (a == "--summary") o.summaryFile = next();
        else if (a == "--list-towns") o.listTowns = true;
        else if (a == "--help" || a == "-h") return false;
        else throw std::runtime_error("unknown option " + a);
    }
    return true;
}

void writeRoutes(const std::string& path, const Instance& inst, const Solution& sol) {
    std::ofstream out(path);
    out << "politician,seq,geonameid,town,lat,lon,population,arrive_h,start_h,finish_h\n";
    for (std::size_t p = 0; p < sol.routes.size(); ++p)
        for (std::size_t i = 0; i < sol.routes[p].size(); ++i) {
            const Visit& v = sol.routes[p][i];
            const Town& t = inst.towns[v.town];
            out << p << ',' << i << ',' << t.geonameId << ",\"" << t.name << "\"," << t.lat << ',' << t.lon << ','
                << t.population << ',' << v.arrive << ',' << v.start << ',' << v.finish << '\n';
        }
}

void appendSummary(const std::string& path, const Options& o, const Instance& inst, const std::string& name,
                   const Metrics& m, double ms) {
    bool fresh = !std::ifstream(path).good();
    std::ofstream out(path, std::ios::app);
    if (fresh)
        out << "approach,towns,k,alpha,hours,reach,max_reach,visits,clashes,travel_h,wait_h,fairness,valid,runtime_ms\n";
    out << name << ',' << inst.n() << ',' << inst.k << ',' << inst.alpha << ',' << inst.horizon << ','
        << m.reach << ',' << inst.maxReach() << ',' << m.visits << ',' << m.clashes << ',' << m.travelHours << ','
        << m.waitHours << ',' << m.fairness() << ',' << (m.valid() ? 1 : 0) << ',' << ms << '\n';
    (void)o;
}

}  // namespace

int main(int argc, char** argv) {
    Options o;
    try {
        if (!parse(argc, argv, o)) {
            usage();
            return 0;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        usage();
        return 1;
    }

    Instance inst;
    try {
        inst = buildInstance(o.cfg);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    std::printf("Instance: %d towns, %d politicians, alpha=%.2f, %.0f campaign hours, HQ=%s\n",
                inst.n(), inst.k, inst.alpha, inst.horizon, inst.depot.name.c_str());
    std::printf("Upper bound on reach (everyone visits everywhere): %.0f\n\n", inst.maxReach());
    if (o.listTowns) {
        for (int c = 0; c < inst.n(); ++c)
            std::printf("  %3d  %-30s pop %8ld  rally %.1fh  %.1fh from HQ\n", c, inst.towns[c].name.c_str(),
                        inst.towns[c].population, inst.stay(c), inst.travelTime(inst.depotIndex(), c));
        std::printf("\n");
    }

    struct Planner { std::string name; std::function<Solution()> run; };
    IlsStats ilsStats;
    std::vector<Planner> planners = {
        {"uncoordinated", [&] { return solveUncoordinated(inst); }},
        {"sequential", [&] { return solveSequential(inst); }},
        {"ils", [&] { return solveIls(inst, o.ils, &ilsStats); }},
    };

    std::printf("%-14s %12s %7s %7s %8s %9s %8s %9s %10s %6s\n", "approach", "reach", "%max", "visits",
                "clashes", "travel_h", "wait_h", "fairness", "runtime_ms", "valid");
    for (const Planner& planner : planners) {
        if (o.approach != "all" && o.approach != planner.name) continue;
        auto t0 = std::chrono::steady_clock::now();
        Solution sol = planner.run();
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        Metrics m = evaluate(inst, sol);

        std::printf("%-14s %12.0f %6.1f%% %7d %8d %9.1f %8.1f %9.2f %10.1f %6s\n", planner.name.c_str(), m.reach,
                    100 * m.reach / inst.maxReach(), m.visits, m.clashes, m.travelHours, m.waitHours, m.fairness(),
                    ms, m.valid() ? "yes" : "NO");
        for (const std::string& err : m.errors) std::printf("    ! %s\n", err.c_str());

        if (!o.routesDir.empty()) writeRoutes(o.routesDir + "/" + planner.name + "_routes.csv", inst, sol);
        if (!o.summaryFile.empty()) appendSummary(o.summaryFile, o, inst, planner.name, m, ms);
    }
    if (o.approach == "all" || o.approach == "ils")
        std::printf("\nILS: %d iterations, %d improvements\n", ilsStats.iterations, ilsStats.improvements);
    return 0;
}
