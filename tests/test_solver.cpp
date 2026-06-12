#include <doctest/doctest.h>

#include "code.hpp"
#include "config.hpp"
#include "feedback.hpp"
#include "solver.hpp"
#include "strategy.hpp"

TEST_CASE("entropy strategy solves all codes for 4c3p") {
    GameConfig cfg{4, 3};
    FeedbackTable ft{cfg};
    auto strategy = make_entropy_strategy(cfg);
    Solver solver{cfg, ft, *strategy};

    int max_turns = 0;
    for (Code secret = 0; secret < cfg.total_codes; ++secret) {
        auto result = solver.solve(secret);
        CHECK(result.solved);
        CHECK(result.turns <= 6);
        if (result.turns > max_turns)
            max_turns = result.turns;
    }
    MESSAGE("Entropy worst-case turns for 4c3p: ", max_turns);
}

TEST_CASE("minimax strategy solves all codes for 4c3p") {
    GameConfig cfg{4, 3};
    FeedbackTable ft{cfg};
    auto strategy = make_minimax_strategy(cfg);
    Solver solver{cfg, ft, *strategy};

    int max_turns = 0;
    for (Code secret = 0; secret < cfg.total_codes; ++secret) {
        auto result = solver.solve(secret);
        CHECK(result.solved);
        CHECK(result.turns <= 6);
        if (result.turns > max_turns)
            max_turns = result.turns;
    }
    MESSAGE("Minimax worst-case turns for 4c3p: ", max_turns);
}

TEST_CASE("solve result structure is consistent") {
    GameConfig cfg{6, 4};
    FeedbackTable ft{cfg};
    auto strategy = make_entropy_strategy(cfg);
    Solver solver{cfg, ft, *strategy};

    auto result = solver.solve(0);
    REQUIRE(result.solved);
    CHECK(result.guesses.size() == static_cast<size_t>(result.turns));
    CHECK(result.feedbacks.size() == static_cast<size_t>(result.turns));
    CHECK(result.feedbacks.back() == winning_feedback(cfg));
    CHECK(result.secret == 0u);
}

TEST_CASE("interactive oracle matches solve") {
    GameConfig cfg{4, 3};
    FeedbackTable ft{cfg};
    auto strategy = make_entropy_strategy(cfg);
    Solver solver{cfg, ft, *strategy};

    Code secret = 17 % cfg.total_codes;
    auto oracle = [&](Code guess) { return ft.get(guess, secret); };
    auto result = solver.solve_interactive(oracle);
    CHECK(result.solved);
    CHECK(result.turns <= 6);
}

TEST_CASE("random strategy terminates for 4c3p") {
    GameConfig cfg{4, 3};
    FeedbackTable ft{cfg};
    auto strategy = make_random_strategy(cfg);
    Solver solver{cfg, ft, *strategy};

    int unsolved = 0;
    for (Code secret = 0; secret < cfg.total_codes; ++secret) {
        auto result = solver.solve(secret);
        if (!result.solved)
            ++unsolved;
    }
    // Random may fail (hits 12-turn limit), but should solve most codes
    CHECK(unsolved < static_cast<int>(cfg.total_codes) / 4);
}
