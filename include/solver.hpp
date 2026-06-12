#pragma once
#include "code.hpp"
#include "config.hpp"
#include "feedback.hpp"
#include "strategy.hpp"
#include <functional>
#include <vector>

struct SolveResult {
    Code secret;
    std::vector<Code> guesses;
    std::vector<Feedback> feedbacks;
    int turns;
    bool solved;
};

// Called with each guess; returns the feedback from the oracle (human or known secret).
using FeedbackOracle = std::function<Feedback(Code guess)>;

class Solver {
public:
    Solver(const GameConfig& cfg, const FeedbackTable& fb_table, Strategy& strategy);

    // Solve with a known secret (benchmarking).
    SolveResult solve(Code secret) const;

    // Solve interactively — oracle provides feedback.
    SolveResult solve_interactive(FeedbackOracle oracle) const;

private:
    const GameConfig& cfg_;
    const FeedbackTable& fb_table_;
    Strategy& strategy_;
    std::vector<Code> all_codes_;

    SolveResult run(FeedbackOracle oracle) const;

    static void filter_candidates(std::vector<Code>& candidates,
                                  Code guess,
                                  Feedback fb,
                                  const FeedbackTable& fb_table);
};
