#include "code.hpp"
#include "config.hpp"
#include "feedback.hpp"
#include "solver.hpp"
#include "strategy.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

struct BenchmarkResult {
    std::string strategy_name;
    double mean_turns;
    double stddev_turns;
    int min_turns;
    int max_turns;
    std::map<int, int> distribution;
    double elapsed_ms;
};

BenchmarkResult run_benchmark(const GameConfig& cfg,
                               const FeedbackTable& fb_table,
                               Strategy& strategy,
                               bool verbose) {
    auto all_codes = enumerate_all_codes(cfg);
    Solver solver(cfg, fb_table, strategy);

    std::vector<int> turn_counts;
    turn_counts.reserve(cfg.total_codes);

    auto t0 = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < cfg.total_codes; ++i) {
        auto result = solver.solve(i);
        turn_counts.push_back(result.turns);

        if (verbose && (i + 1) % 1000 == 0)
            std::cout << "  " << strategy.name() << ": " << (i + 1)
                      << "/" << cfg.total_codes << " secrets done\n";
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double, std::milli>(t1 - t0).count();

    double mean = static_cast<double>(
                      std::accumulate(turn_counts.begin(), turn_counts.end(), 0)) /
                  turn_counts.size();

    double variance = 0.0;
    for (int t : turn_counts) {
        double d = t - mean;
        variance += d * d;
    }
    variance /= turn_counts.size();

    BenchmarkResult res;
    res.strategy_name = strategy.name();
    res.mean_turns = mean;
    res.stddev_turns = std::sqrt(variance);
    res.min_turns = *std::min_element(turn_counts.begin(), turn_counts.end());
    res.max_turns = *std::max_element(turn_counts.begin(), turn_counts.end());
    res.elapsed_ms = elapsed;

    for (int t : turn_counts)
        ++res.distribution[t];

    return res;
}

void print_results(const std::vector<BenchmarkResult>& results, const GameConfig& cfg) {
    std::cout << "\n=== Benchmark Results: " << cfg.description() << " ===\n\n";

    // Summary table
    std::cout << std::left
              << std::setw(10) << "Strategy"
              << std::right
              << std::setw(8) << "Mean"
              << std::setw(8) << "StdDev"
              << std::setw(6) << "Min"
              << std::setw(6) << "Max"
              << std::setw(10) << "Time(ms)"
              << "\n";
    std::cout << std::string(48, '-') << "\n";

    for (const auto& r : results) {
        std::cout << std::left  << std::setw(10) << r.strategy_name
                  << std::right << std::fixed << std::setprecision(3)
                  << std::setw(8) << r.mean_turns
                  << std::setw(8) << r.stddev_turns
                  << std::setw(6) << r.min_turns
                  << std::setw(6) << r.max_turns
                  << std::setw(10) << static_cast<int>(r.elapsed_ms)
                  << "\n";
    }

    // Distribution table
    std::cout << "\n--- Turn distribution ---\n";
    std::cout << std::left << std::setw(10) << "Turns";
    for (const auto& r : results)
        std::cout << std::right << std::setw(10) << r.strategy_name;
    std::cout << "\n" << std::string(10 + 10 * results.size(), '-') << "\n";

    int max_turns = 0;
    for (const auto& r : results)
        if (r.max_turns > max_turns) max_turns = r.max_turns;

    for (int t = 1; t <= max_turns; ++t) {
        std::cout << std::left << std::setw(10) << t;
        for (const auto& r : results) {
            auto it = r.distribution.find(t);
            int count = (it != r.distribution.end()) ? it->second : 0;
            std::cout << std::right << std::setw(10) << count;
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

static void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -c, --colors N      Number of colors (default: 6)\n"
              << "  -p, --positions N   Number of positions (default: 4)\n"
              << "  -s, --strategy S    Strategies to benchmark: all|random|entropy|minimax"
                 " (default: all)\n"
              << "  -v, --verbose       Print progress during benchmark\n"
              << "  -h, --help          Show this help\n";
}

int main(int argc, char* argv[]) {
    uint32_t colors = 6, positions = 4;
    std::string which_strategy = "all";
    bool verbose = false;

    static const option long_opts[] = {
        {"colors",    required_argument, nullptr, 'c'},
        {"positions", required_argument, nullptr, 'p'},
        {"strategy",  required_argument, nullptr, 's'},
        {"verbose",   no_argument,       nullptr, 'v'},
        {"help",      no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "c:p:s:vh", long_opts, nullptr)) != -1) {
        switch (opt) {
        case 'c': colors    = static_cast<uint32_t>(std::stoul(optarg)); break;
        case 'p': positions = static_cast<uint32_t>(std::stoul(optarg)); break;
        case 's': which_strategy = optarg; break;
        case 'v': verbose = true; break;
        case 'h': print_usage(argv[0]); return 0;
        default:  print_usage(argv[0]); return 1;
        }
    }

    GameConfig cfg(colors, positions);
    std::cout << "Mastermind Benchmark\n"
              << cfg.description() << "\n";

    if (cfg.use_precomputed_table())
        std::cout << "Building feedback table (" << cfg.total_codes << "x"
                  << cfg.total_codes << ")...\n";
    else
        std::cout << "Large search space — using on-the-fly feedback computation.\n";

    FeedbackTable fb_table(cfg);

    std::vector<std::string> strategies_to_run;
    if (which_strategy == "all")
        strategies_to_run = {"random", "entropy", "minimax"};
    else
        strategies_to_run = {which_strategy};

    std::vector<BenchmarkResult> results;
    for (const auto& sname : strategies_to_run) {
        auto strategy = make_strategy(sname, cfg);
        std::cout << "\nRunning " << sname << " strategy...\n";
        results.push_back(run_benchmark(cfg, fb_table, *strategy, verbose));
        std::cout << "  Done in " << static_cast<int>(results.back().elapsed_ms)
                  << " ms\n";
    }

    print_results(results, cfg);
    return 0;
}
