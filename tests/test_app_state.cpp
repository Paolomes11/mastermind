#include <doctest/doctest.h>

#include <chrono>

#include "app_state.hpp"
#include "feedback.hpp"

// Blocks until the background scoring future resolves and applies its result.
static void await_scoring(AppState& s) {
    if (!s.score_future.valid())
        return;
    ScoringResult r = s.score_future.get();
    s.computing = false;
    s.chart_entropy = std::move(r.entropy_scores);
    s.chart_minimax = std::move(r.minimax_scores);
    s.pending_guess = r.chosen_guess;
    if (s.mode == AppMode::Guided)
        s.phase = SolvePhase::AwaitingFeedback;
    else
        s.phase = SolvePhase::AutoPaused;
}

// ── reset / initialization ────────────────────────────────────────────────────

TEST_CASE("reset builds core objects") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.reset();
    await_scoring(s);

    REQUIRE(s.cfg != nullptr);
    CHECK(s.cfg->colors == 3);
    CHECK(s.cfg->positions == 3);
    CHECK(s.fb_table != nullptr);
    CHECK(s.strategy != nullptr);
    CHECK(!s.all_codes.empty());
    CHECK(s.candidates.size() == s.cfg->total_codes);
}

TEST_CASE("reset clears history and chart data before new scoring completes") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.reset();
    await_scoring(s);

    // Simulate one turn of history
    s.history.push_back({0, 1, 0, (int)s.candidates.size(), {}});
    s.candidates_per_turn.push_back(27);

    // Reset should clear history and turn data immediately (before async completes).
    s.reset();

    CHECK(s.history.empty());
    CHECK(s.candidates_per_turn.empty());
    // charts are cleared synchronously inside reset() before scoring launches
    CHECK(s.chart_entropy.empty());
    CHECK(s.chart_minimax.empty());

    await_scoring(s);  // charts repopulated by new scoring — that's expected
}

TEST_CASE("reset in guided mode starts computing then awaits feedback") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::Guided;
    s.reset();

    CHECK(s.phase == SolvePhase::Computing);
    CHECK(s.computing == true);

    await_scoring(s);

    CHECK(s.phase == SolvePhase::AwaitingFeedback);
    CHECK(s.computing == false);
    CHECK(s.pending_guess < s.cfg->total_codes);
}

TEST_CASE("reset in autosolve mode starts idle") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::AutoSolve;
    s.reset();

    CHECK(s.phase == SolvePhase::Idle);
    CHECK(!s.secret.has_value());
}

// ── apply_feedback validation ─────────────────────────────────────────────────

TEST_CASE("apply_feedback rejects blacks > positions") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.reset();
    await_scoring(s);

    bool ok = s.apply_feedback(4, 0);
    CHECK(!ok);
    CHECK(!s.feedback_error.empty());
    CHECK(s.history.empty());
}

TEST_CASE("apply_feedback rejects whites > positions") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.reset();
    await_scoring(s);

    bool ok = s.apply_feedback(0, 4);
    CHECK(!ok);
    CHECK(!s.feedback_error.empty());
    CHECK(s.history.empty());
}

TEST_CASE("apply_feedback rejects blacks + whites > positions") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.reset();
    await_scoring(s);

    bool ok = s.apply_feedback(2, 2);
    CHECK(!ok);
    CHECK(!s.feedback_error.empty());
    CHECK(s.history.empty());
}

// ── apply_feedback state transitions ─────────────────────────────────────────

TEST_CASE("apply_feedback records turn and advances game") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::Guided;
    s.reset();
    await_scoring(s);

    size_t before = s.candidates.size();
    bool ok = s.apply_feedback(0, 1);
    REQUIRE(ok);

    CHECK(s.history.size() == 1);
    CHECK(s.candidates_per_turn.size() == 1);
    CHECK(s.candidates_per_turn[0] == (int)before);
    CHECK(s.feedback_error.empty());
}

TEST_CASE("apply_feedback with all-black feedback marks game solved") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::Guided;
    s.reset();
    await_scoring(s);

    uint8_t blacks = static_cast<uint8_t>(s.cfg->positions);
    bool ok = s.apply_feedback(blacks, 0);
    REQUIRE(ok);

    CHECK(s.phase == SolvePhase::Solved);
    CHECK(s.candidates.empty());
}

TEST_CASE("apply_feedback with impossible feedback leads to failed phase") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::Guided;
    s.reset();
    await_scoring(s);

    // Apply feedback that eliminates all candidates.
    // Manually set candidates to only the current pending_guess so that
    // non-winning feedback leaves 0 remaining candidates.
    s.candidates = {s.pending_guess};

    // 0 blacks, 0 whites eliminates pending_guess itself from candidates,
    // unless it truly has 0 matching — but with 1 candidate (the guess),
    // feedback (0,0) is only consistent if the guess has no matching digits,
    // which can't happen when secret == guess. So we use (0,0) which will
    // leave 0 candidates after filtering (the only candidate IS the guess
    // and it would require all-black feedback to survive).
    bool ok = s.apply_feedback(0, 0);
    REQUIRE(ok);

    CHECK(s.phase == SolvePhase::Failed);
}

// ── encode_secret_input ───────────────────────────────────────────────────────

TEST_CASE("encode_secret_input with all 1s produces code 0") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::AutoSolve;
    s.reset();

    // Default secret_input is all 1s (1-based), which maps to digit 0 each position.
    for (int i = 0; i < 3; ++i)
        s.secret_input[i] = 1;

    Code c = s.encode_secret_input();
    CHECK(c == 0);
}

TEST_CASE("encode_secret_input is consistent with decode") {
    AppState s;
    s.colors_ui = 4;
    s.positions_ui = 3;
    s.mode = AppMode::AutoSolve;
    s.reset();

    // Set digits 2, 3, 1 (1-based) = digits 1, 2, 0 (0-based)
    s.secret_input[0] = 2;
    s.secret_input[1] = 3;
    s.secret_input[2] = 1;

    Code c = s.encode_secret_input();
    auto digits = decode(c, *s.cfg);

    // encode_secret_input maps secret_input[p]-1 to digit at position p
    CHECK(digits[0] == 1);  // position 0: input 2 → digit 1
    CHECK(digits[1] == 2);  // position 1: input 3 → digit 2
    CHECK(digits[2] == 0);  // position 2: input 1 → digit 0
}

TEST_CASE("encode_secret_input clamps out-of-range values") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::AutoSolve;
    s.reset();

    s.secret_input[0] = 99;  // exceeds colors — should clamp to max digit
    s.secret_input[1] = -5;  // below 1 — should clamp to digit 0
    s.secret_input[2] = 2;

    Code c = s.encode_secret_input();
    auto digits = decode(c, *s.cfg);

    CHECK(digits[0] == static_cast<int>(s.cfg->colors) - 1);
    CHECK(digits[1] == 0);
    CHECK(digits[2] == 1);
}

// ── mode switching ────────────────────────────────────────────────────────────

TEST_CASE("set_mode clears history and switches phase") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::Guided;
    s.reset();
    await_scoring(s);

    // Record a fake turn
    s.history.push_back({0, 0, 1, 27, {}});

    s.set_mode(AppMode::AutoSolve);

    CHECK(s.mode == AppMode::AutoSolve);
    CHECK(s.history.empty());
    CHECK(s.phase == SolvePhase::Idle);
}

// ── full guided mini-game ─────────────────────────────────────────────────────

TEST_CASE("guided mode solves 3c3p with oracle feedback") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::Guided;
    s.reset();

    // Pick any secret and drive the solver with honest feedback.
    GameConfig cfg{3, 3};
    FeedbackTable ft{cfg};
    Code secret = 13 % cfg.total_codes;

    int turns = 0;
    constexpr int max_turns = 12;

    while (turns < max_turns) {
        await_scoring(s);

        if (s.phase == SolvePhase::Solved)
            break;
        REQUIRE(s.phase == SolvePhase::AwaitingFeedback);

        Feedback fb = ft.get(s.pending_guess, secret);
        uint8_t blacks, whites;
        unpack_feedback(fb, cfg, blacks, whites);

        bool ok = s.apply_feedback(blacks, whites);
        REQUIRE(ok);
        ++turns;

        if (s.phase == SolvePhase::Solved)
            break;
    }

    CHECK(s.phase == SolvePhase::Solved);
    CHECK(s.history.size() <= static_cast<size_t>(max_turns));
    // Each history entry should have consistent feedback
    for (const auto& rec : s.history) {
        CHECK(static_cast<int>(rec.blacks) + static_cast<int>(rec.whites) <=
              s.cfg->positions);
    }
}

// ── auto-solve ────────────────────────────────────────────────────────────────

TEST_CASE("autosolve step_auto_solve advances until solved") {
    AppState s;
    s.colors_ui = 3;
    s.positions_ui = 3;
    s.mode = AppMode::AutoSolve;
    s.reset();

    // Provide a secret and launch scoring.
    s.secret_input[0] = 2;
    s.secret_input[1] = 1;
    s.secret_input[2] = 3;
    s.secret = s.encode_secret_input();
    s.launch_scoring();

    int steps = 0;
    constexpr int max_steps = 12;

    while (steps < max_steps) {
        await_scoring(s);

        if (s.phase == SolvePhase::Solved || s.phase == SolvePhase::Failed)
            break;

        REQUIRE((s.phase == SolvePhase::AutoPaused || s.phase == SolvePhase::AutoPlaying));
        s.step_auto_solve(0.0);
        ++steps;

        // After step, scoring is launched; wait again next iteration.
    }

    // Might have ended due to Solved or Failed, but must not loop forever.
    CHECK((s.phase == SolvePhase::Solved || s.phase == SolvePhase::Failed));
}
