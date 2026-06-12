#include "solver.hpp"
#include "code.hpp"
#include <algorithm>

Solver::Solver(const GameConfig& cfg, const FeedbackTable& fb_table, Strategy& strategy)
    : cfg_(cfg), fb_table_(fb_table), strategy_(strategy),
      all_codes_(enumerate_all_codes(cfg)) {}

void Solver::filter_candidates(std::vector<Code>& candidates,
                                Code guess, Feedback fb,
                                const FeedbackTable& fb_table) {
    candidates.erase(
        std::remove_if(candidates.begin(), candidates.end(),
            [&](Code c) { return fb_table.get(guess, c) != fb; }),
        candidates.end());
}

SolveResult Solver::run(FeedbackOracle oracle) const {
    static constexpr int MAX_TURNS = 12;

    SolveResult result{};
    result.solved = false;

    std::vector<Code> candidates = all_codes_;
    Feedback win_fb = winning_feedback(cfg_);

    for (int turn = 0; turn < MAX_TURNS; ++turn) {
        Code guess = strategy_.choose_guess(all_codes_, candidates, fb_table_, turn);
        Feedback fb = oracle(guess);

        result.guesses.push_back(guess);
        result.feedbacks.push_back(fb);
        result.turns = turn + 1;

        if (fb == win_fb) {
            result.solved = true;
            return result;
        }

        filter_candidates(candidates, guess, fb, fb_table_);
        if (candidates.empty()) return result;
    }

    return result;
}

SolveResult Solver::solve(Code secret) const {
    auto result = run([&](Code guess) { return fb_table_.get(guess, secret); });
    result.secret = secret;
    return result;
}

SolveResult Solver::solve_interactive(FeedbackOracle oracle) const {
    auto result = run(oracle);
    if (result.solved && !result.guesses.empty())
        result.secret = result.guesses.back();
    return result;
}
