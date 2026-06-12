#include <random>

#include "strategy.hpp"

namespace {

class StrategyRandom : public Strategy {
    mutable std::mt19937 rng_;

public:
    explicit StrategyRandom(const GameConfig&) : rng_(std::random_device{}()) {}

    std::string name() const override { return "random"; }

    Code choose_guess(const std::vector<Code>&, const std::vector<Code>& candidates,
                      const FeedbackTable&, int) const override {
        std::uniform_int_distribution<size_t> dist(0, candidates.size() - 1);
        return candidates[dist(rng_)];
    }
};

}  // namespace

std::unique_ptr<Strategy> make_random_strategy(const GameConfig& cfg) {
    return std::make_unique<StrategyRandom>(cfg);
}
