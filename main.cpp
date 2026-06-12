#include <getopt.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "code.hpp"
#include "config.hpp"
#include "feedback.hpp"
#include "solver.hpp"
#include "strategy.hpp"

// ── Interactive mode ──────────────────────────────────────────────────────────

static void run_interactive(const GameConfig& cfg, const FeedbackTable& fb_table,
                            const std::string& strategy_name) {
    auto strategy = make_strategy(strategy_name, cfg);
    Solver solver(cfg, fb_table, *strategy);

    std::cout
        << "\nInteractive mode — " << strategy->name() << " strategy\n"
        << "After each guess, enter feedback as: <blacks> <whites>\n"
        << "(blacks = right color + right position, whites = right color + wrong position)\n\n";

    int turn = 0;
    auto oracle = [&](Code guess) -> Feedback {
        ++turn;
        std::cout << "Guess " << turn << ": " << code_to_string(guess, cfg) << "\n";
        std::cout << "Feedback (blacks whites): ";

        int blacks, whites;
        while (!(std::cin >> blacks >> whites) || blacks < 0 || whites < 0 ||
               blacks + whites > static_cast<int>(cfg.positions) ||
               blacks > static_cast<int>(cfg.positions)) {
            std::cin.clear();
            std::cin.ignore(1024, '\n');
            std::cout << "Invalid — re-enter (blacks whites): ";
        }
        return pack_feedback(static_cast<uint8_t>(blacks), static_cast<uint8_t>(whites), cfg);
    };

    auto result = solver.solve_interactive(oracle);

    if (result.solved)
        std::cout << "\nSolved in " << result.turns << " guess" << (result.turns == 1 ? "" : "es")
                  << "!\n";
    else
        std::cout << "\nFailed to solve within " << result.turns << " guesses.\n";
}

// ── Benchmark mode ─────────────────────────────────────────────────────────────

struct BenchStats {
    double mean, stddev;
    int min_t, max_t;
    std::map<int, int> dist;
    double elapsed_ms;
};

static BenchStats benchmark_strategy(const GameConfig& cfg, const FeedbackTable& fb_table,
                                     Strategy& strategy, bool verbose) {
    Solver solver(cfg, fb_table, strategy);
    std::vector<int> counts;
    counts.reserve(cfg.total_codes);

    auto t0 = std::chrono::high_resolution_clock::now();
    for (uint32_t s = 0; s < cfg.total_codes; ++s) {
        counts.push_back(solver.solve(s).turns);
        if (verbose && (s + 1) % 500 == 0)
            std::cout << "  " << strategy.name() << ": " << (s + 1) << "/" << cfg.total_codes
                      << "\n";
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    BenchStats r;
    r.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double mean =
        static_cast<double>(std::accumulate(counts.begin(), counts.end(), 0)) / counts.size();
    r.mean = mean;
    double var = 0;
    for (int t : counts) {
        double d = t - mean;
        var += d * d;
    }
    r.stddev = std::sqrt(var / counts.size());
    r.min_t = *std::min_element(counts.begin(), counts.end());
    r.max_t = *std::max_element(counts.begin(), counts.end());
    for (int t : counts)
        ++r.dist[t];
    return r;
}

static void run_benchmark(const GameConfig& cfg, const FeedbackTable& fb_table, bool verbose) {
    const std::vector<std::string> names = {"random", "entropy", "minimax"};
    std::vector<std::pair<std::string, BenchStats>> results;

    for (const auto& name : names) {
        auto strategy = make_strategy(name, cfg);
        std::cout << "Running " << name << "...\n";
        results.push_back({name, benchmark_strategy(cfg, fb_table, *strategy, verbose)});
        std::cout << "  done in " << static_cast<int>(results.back().second.elapsed_ms) << " ms\n";
    }

    std::cout << "\n=== Benchmark: " << cfg.description() << " ===\n\n"
              << std::left << std::setw(10) << "Strategy" << std::right << std::setw(8) << "Mean"
              << std::setw(8) << "StdDev" << std::setw(6) << "Min" << std::setw(6) << "Max"
              << std::setw(10) << "Time(ms)" << "\n"
              << std::string(48, '-') << "\n";

    for (const auto& [name, s] : results) {
        std::cout << std::left << std::setw(10) << name << std::right << std::fixed
                  << std::setprecision(3) << std::setw(8) << s.mean << std::setw(8) << s.stddev
                  << std::setw(6) << s.min_t << std::setw(6) << s.max_t << std::setw(10)
                  << static_cast<int>(s.elapsed_ms) << "\n";
    }

    // Distribution
    int max_t = 0;
    for (const auto& [_, s] : results)
        if (s.max_t > max_t)
            max_t = s.max_t;

    std::cout << "\n" << std::left << std::setw(8) << "Turns";
    for (const auto& [name, _] : results)
        std::cout << std::right << std::setw(10) << name;
    std::cout << "\n" << std::string(8 + 10 * results.size(), '-') << "\n";

    for (int t = 1; t <= max_t; ++t) {
        std::cout << std::left << std::setw(8) << t;
        for (const auto& [_, s] : results) {
            auto it = s.dist.find(t);
            std::cout << std::right << std::setw(10) << (it != s.dist.end() ? it->second : 0);
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

// ── Help & CLI ────────────────────────────────────────────────────────────────

static void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  -c, --colors N      Number of colors (default: 6)\n"
              << "  -p, --positions N   Number of positions (default: 4)\n"
              << "  -s, --strategy S    Strategy: random|entropy|minimax (default: entropy)\n"
              << "  -b, --benchmark     Benchmark all three strategies vs all secrets\n"
              << "  -i, --interactive   Interactive play (human provides feedback)\n"
              << "  -v, --verbose       Verbose progress output during benchmark\n"
              << "  -h, --help          Show this help\n\n"
              << "Examples:\n"
              << "  " << prog << " -b                   # benchmark 6 colors, 4 positions\n"
              << "  " << prog << " -c 6 -p 4 -b         # same, explicit\n"
              << "  " << prog << " -c 6 -p 4 -s entropy -i  # interactive play\n";
}

int main(int argc, char* argv[]) {
    uint32_t colors = 6, positions = 4;
    std::string strategy_name = "entropy";
    bool benchmark = false, interactive = false, verbose = false;

    static const option long_opts[] = {{"colors", required_argument, nullptr, 'c'},
                                       {"positions", required_argument, nullptr, 'p'},
                                       {"strategy", required_argument, nullptr, 's'},
                                       {"benchmark", no_argument, nullptr, 'b'},
                                       {"interactive", no_argument, nullptr, 'i'},
                                       {"verbose", no_argument, nullptr, 'v'},
                                       {"help", no_argument, nullptr, 'h'},
                                       {nullptr, 0, nullptr, 0}};

    int opt;
    while ((opt = getopt_long(argc, argv, "c:p:s:bivh", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'c':
                colors = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 'p':
                positions = static_cast<uint32_t>(std::stoul(optarg));
                break;
            case 's':
                strategy_name = optarg;
                break;
            case 'b':
                benchmark = true;
                break;
            case 'i':
                interactive = true;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                print_help(argv[0]);
                return 0;
            default:
                print_help(argv[0]);
                return 1;
        }
    }

    if (!benchmark && !interactive) {
        print_help(argv[0]);
        return 0;
    }

    GameConfig cfg(colors, positions);
    std::cout << "Mastermind Solver\n" << cfg.description() << "\n";

    if (cfg.use_precomputed_table())
        std::cout << "Building feedback table...\n";
    else
        std::cout << "Large search space — using on-the-fly feedback.\n";

    FeedbackTable fb_table(cfg);

    if (benchmark)
        run_benchmark(cfg, fb_table, verbose);
    if (interactive)
        run_interactive(cfg, fb_table, strategy_name);

    return 0;
}
