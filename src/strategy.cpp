#include "strategy.hpp"

#include <stdexcept>

std::vector<ScoredGuess> Strategy::score_candidates(const std::vector<Code>&,
                                                    const std::vector<Code>&, const FeedbackTable&,
                                                    int) const {
    return {};
}

std::unique_ptr<Strategy> make_strategy(const std::string& name, const GameConfig& cfg) {
    if (name == "random")
        return make_random_strategy(cfg);
    if (name == "entropy")
        return make_entropy_strategy(cfg);
    if (name == "minimax")
        return make_minimax_strategy(cfg);
    throw std::invalid_argument("Unknown strategy '" + name +
                                "'. Choose: random, entropy, minimax");
}
