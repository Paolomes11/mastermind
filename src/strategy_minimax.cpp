#include "strategy.hpp"
#include <algorithm>
#include <limits>
#include <vector>

namespace {

class StrategyMinimax : public Strategy {
    const GameConfig& cfg_;

public:
    explicit StrategyMinimax(const GameConfig& cfg) : cfg_(cfg) {}

    std::string name() const override { return "minimax"; }

    bool lower_score_is_better() const override { return true; }

    std::vector<ScoredGuess> score_candidates(
        const std::vector<Code>& all_codes,
        const std::vector<Code>& candidates,
        const FeedbackTable& fb_table,
        int top_n) const override {
        if (candidates.size() <= 1) return {};
        const auto& search_set = fb_table.is_precomputed() ? all_codes : candidates;
        uint32_t counts[64];
        std::vector<ScoredGuess> scores;
        scores.reserve(search_set.size());
        for (Code guess : search_set) {
            std::fill(counts, counts + 64, 0u);
            int worst = 0;
            for (Code cand : candidates) {
                uint8_t fb = fb_table.get(guess, cand);
                if (static_cast<int>(++counts[fb]) > worst)
                    worst = counts[fb];
            }
            scores.emplace_back(guess, static_cast<double>(worst));
        }
        int n = std::min(top_n, static_cast<int>(scores.size()));
        std::partial_sort(scores.begin(), scores.begin() + n, scores.end(),
            [](const ScoredGuess& a, const ScoredGuess& b) {
                return a.second < b.second;
            });
        scores.resize(n);
        return scores;
    }

    Code choose_guess(const std::vector<Code>& all_codes,
                      const std::vector<Code>& candidates,
                      const FeedbackTable& fb_table,
                      int) const override {
        if (candidates.size() <= 2) return candidates[0];

        const auto& search_set = fb_table.is_precomputed() ? all_codes : candidates;

        // O(1) candidate lookup for tiebreaking.
        std::vector<bool> is_cand(cfg_.total_codes, false);
        for (Code c : candidates) is_cand[c] = true;

        Code best_guess = candidates[0];
        int best_worst = std::numeric_limits<int>::max();
        bool best_is_cand = true;

        uint32_t counts[64];

        for (Code guess : search_set) {
            std::fill(counts, counts + 64, 0u);
            int worst = 0;

            for (Code cand : candidates) {
                uint8_t fb = fb_table.get(guess, cand);
                if (static_cast<int>(++counts[fb]) > worst)
                    worst = counts[fb];
            }

            bool this_is_cand = is_cand[guess];
            if (worst < best_worst ||
                (worst == best_worst && this_is_cand && !best_is_cand)) {
                best_worst = worst;
                best_guess = guess;
                best_is_cand = this_is_cand;
            }
        }

        return best_guess;
    }
};

} // namespace

std::unique_ptr<Strategy> make_minimax_strategy(const GameConfig& cfg) {
    return std::make_unique<StrategyMinimax>(cfg);
}
