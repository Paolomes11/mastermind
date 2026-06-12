#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "code.hpp"
#include "config.hpp"
#include "feedback.hpp"

// (code, score) pair for visualization. Score semantics are strategy-defined:
//   entropy  → higher is better
//   minimax  → lower is better (use lower_score_is_better() to distinguish)
using ScoredGuess = std::pair<Code, double>;

class Strategy {
public:
    virtual ~Strategy() = default;
    virtual std::string name() const = 0;

    // Select the next guess.
    // all_codes: full enumeration [0..N-1] (never modified).
    // candidates: current set of possible secrets.
    // turn: 0-indexed turn number.
    virtual Code choose_guess(const std::vector<Code>& all_codes,
                              const std::vector<Code>& candidates, const FeedbackTable& fb_table,
                              int turn) const = 0;

    // Returns up to top_n (code, score) pairs sorted by descending score.
    // Default implementation returns {} (random strategy has no meaningful scores).
    virtual std::vector<ScoredGuess> score_candidates(const std::vector<Code>& all_codes,
                                                      const std::vector<Code>& candidates,
                                                      const FeedbackTable& fb_table,
                                                      int top_n = 10) const;

    // True if a lower score is better (minimax), false if higher is better (entropy).
    virtual bool lower_score_is_better() const { return false; }
};

// Factories — implemented in respective strategy_X.cpp files.
std::unique_ptr<Strategy> make_random_strategy(const GameConfig& cfg);
std::unique_ptr<Strategy> make_entropy_strategy(const GameConfig& cfg);
std::unique_ptr<Strategy> make_minimax_strategy(const GameConfig& cfg);

// Dispatch by name: "random", "entropy", "minimax".
std::unique_ptr<Strategy> make_strategy(const std::string& name, const GameConfig& cfg);
