#include "strategy.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

class StrategyEntropy : public Strategy {
    const GameConfig& cfg_;

public:
    explicit StrategyEntropy(const GameConfig& cfg) : cfg_(cfg) {}

    std::string name() const override { return "entropy"; }

    bool lower_score_is_better() const override { return false; }

    std::vector<ScoredGuess> score_candidates(
        const std::vector<Code>& all_codes,
        const std::vector<Code>& candidates,
        const FeedbackTable& fb_table,
        int top_n) const override {
        if (candidates.size() <= 1) return {};
        const auto& search_set = fb_table.is_precomputed() ? all_codes : candidates;
        auto N_cand = static_cast<uint32_t>(candidates.size());
        uint32_t counts[64];
        std::vector<ScoredGuess> scores;
        scores.reserve(search_set.size());
        for (Code guess : search_set) {
            std::fill(counts, counts + 64, 0u);
            for (Code cand : candidates)
                ++counts[fb_table.get(guess, cand)];
            double H = 0.0;
            for (int i = 0; i < 64; ++i) {
                if (counts[i] > 0) {
                    double p = static_cast<double>(counts[i]) / N_cand;
                    H -= p * std::log2(p);
                }
            }
            scores.emplace_back(guess, H);
        }
        int n = std::min(top_n, static_cast<int>(scores.size()));
        std::partial_sort(scores.begin(), scores.begin() + n, scores.end(),
            [](const ScoredGuess& a, const ScoredGuess& b) {
                return a.second > b.second;
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
        auto N_cand = static_cast<uint32_t>(candidates.size());

        // O(1) candidate lookup for tiebreaking.
        std::vector<bool> is_cand(cfg_.total_codes, false);
        for (Code c : candidates) is_cand[c] = true;

        Code best_guess = candidates[0];
        double best_entropy = -1.0;
        bool best_is_cand = true;

        // Max feedback value = P*(P+2) <= 48 for P<=6; 64 slots is safe.
        uint32_t counts[64];

        for (Code guess : search_set) {
            std::fill(counts, counts + 64, 0u);

            for (Code cand : candidates)
                ++counts[fb_table.get(guess, cand)];

            double H = 0.0;
            for (int i = 0; i < 64; ++i) {
                if (counts[i] > 0) {
                    double p = static_cast<double>(counts[i]) / N_cand;
                    H -= p * std::log2(p);
                }
            }

            bool this_is_cand = is_cand[guess];
            if (H > best_entropy ||
                (H == best_entropy && this_is_cand && !best_is_cand)) {
                best_entropy = H;
                best_guess = guess;
                best_is_cand = this_is_cand;
            }
        }

        return best_guess;
    }
};

} // namespace

std::unique_ptr<Strategy> make_entropy_strategy(const GameConfig& cfg) {
    return std::make_unique<StrategyEntropy>(cfg);
}
