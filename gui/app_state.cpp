#include "app_state.hpp"

#include <algorithm>
#include <stdexcept>

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::vector<Code> filter_candidates(const std::vector<Code>& candidates, Code guess,
                                           Feedback fb, const FeedbackTable& table) {
    std::vector<Code> out;
    out.reserve(candidates.size());
    for (Code c : candidates)
        if (table.get(guess, c) == fb)
            out.push_back(c);
    return out;
}

// ── AppState::reset ───────────────────────────────────────────────────────────

void AppState::reset() {
    // Must synchronize in-flight async task before destroying objects it references
    if (score_future.valid())
        score_future.get();
    computing = false;

    cfg = std::make_unique<GameConfig>(colors_ui, positions_ui);
    fb_table = std::make_unique<FeedbackTable>(*cfg);
    strategy = make_strategy(kStrategyNames[strategy_idx], *cfg);
    all_codes = enumerate_all_codes(*cfg);
    candidates = all_codes;

    history.clear();
    chart_entropy.clear();
    chart_minimax.clear();
    candidates_per_turn.clear();

    input_blacks = 0;
    input_whites = 0;
    feedback_error.clear();
    pending_guess = 0;
    last_auto_step_time = 0.0;
    fail_reason = FailReason::None;

    if (mode == AppMode::Guided) {
        // Start computing immediately in guided mode
        launch_scoring();
    } else {
        // In AutoSolve, wait until user sets a secret
        secret = std::nullopt;
        phase = SolvePhase::Idle;
    }
}

// ── AppState::launch_scoring ──────────────────────────────────────────────────

void AppState::launch_scoring() {
    phase = SolvePhase::Computing;
    computing = true;

    // Copy by value so the async lambda doesn't dangle if reset() is called
    auto all = all_codes;
    auto cand = candidates;
    GameConfig cfg_copy = *cfg;
    int turn = static_cast<int>(history.size());

    // fb_table is captured by reference — safe because reset() calls
    // score_future.get() before destroying fb_table.
    score_future = std::async(
        std::launch::async,
        [all, cand, cfg_copy, &fb = *fb_table, &strat = *strategy, turn]() -> ScoringResult {
            ScoringResult r;
            // Always compute both entropy and minimax scores for visualization
            r.entropy_scores = make_entropy_strategy(cfg_copy)->score_candidates(all, cand, fb, 10);
            r.minimax_scores = make_minimax_strategy(cfg_copy)->score_candidates(all, cand, fb, 10);
            r.chosen_guess = strat.choose_guess(all, cand, fb, turn);
            return r;
        });
}

// ── AppState::poll_scoring ────────────────────────────────────────────────────

void AppState::poll_scoring(double current_time_s) {
    if (!computing || !score_future.valid())
        return;

    if (score_future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        return;

    ScoringResult r = score_future.get();
    computing = false;
    chart_entropy = std::move(r.entropy_scores);
    chart_minimax = std::move(r.minimax_scores);
    pending_guess = r.chosen_guess;

    if (mode == AppMode::Guided) {
        phase = SolvePhase::AwaitingFeedback;
    } else {
        // AutoSolve: begin paused so user can step through
        phase = SolvePhase::AutoPaused;
        last_auto_step_time = current_time_s;
    }
}

// ── AppState::apply_feedback ──────────────────────────────────────────────────

bool AppState::apply_feedback(uint8_t blacks, uint8_t whites) {
    feedback_error.clear();
    int pos = static_cast<int>(cfg->positions);

    if (static_cast<int>(blacks) > pos) {
        feedback_error = "Blacks > positions";
        return false;
    }
    if (static_cast<int>(whites) > pos) {
        feedback_error = "Whites > positions";
        return false;
    }
    if (static_cast<int>(blacks + whites) > pos) {
        feedback_error = "Blacks + whites > positions";
        return false;
    }
    // In standard Mastermind, (P-1) blacks + 1 white is impossible: with only one slot remaining,
    // that peg is either correct (black) or absent — it cannot be right-color-wrong-position.
    if (pos >= 2 && static_cast<int>(blacks) == pos - 1 && static_cast<int>(whites) == 1) {
        feedback_error = std::to_string(pos - 1) + " black + 1 white is impossible in standard "
                         "Mastermind (the last peg can't be right-color-wrong-position).";
        return false;
    }

    Feedback fb = pack_feedback(blacks, whites, *cfg);

    // Save snapshot of candidates BEFORE filtering so this turn can be undone.
    std::vector<Code> snapshot = candidates;

    candidates_per_turn.push_back(static_cast<int>(candidates.size()));
    history.push_back({pending_guess, blacks, whites, static_cast<int>(candidates.size()),
                       std::move(snapshot)});

    Feedback win = winning_feedback(*cfg);
    if (fb == win) {
        candidates.clear();
        phase = SolvePhase::Solved;
        return true;
    }

    candidates = filter_candidates(candidates, pending_guess, fb, *fb_table);

    if (candidates.empty()) {
        fail_reason = FailReason::EmptyCandidates;
        phase = SolvePhase::Failed;
        return true;
    }
    if (static_cast<int>(history.size()) >= 12) {
        fail_reason = FailReason::MaxTurns;
        phase = SolvePhase::Failed;
        return true;
    }

    launch_scoring();
    return true;
}

// ── AppState::undo_last_turn ──────────────────────────────────────────────────

bool AppState::undo_last_turn() {
    if (history.empty() || computing)
        return false;
    // Sync any in-flight future first (shouldn't be running if history is non-empty
    // and we're in AwaitingFeedback/Failed/Solved, but be safe).
    if (score_future.valid())
        score_future.get();
    computing = false;

    TurnRecord& last = history.back();
    candidates = std::move(last.candidates_snapshot);
    history.pop_back();
    if (!candidates_per_turn.empty())
        candidates_per_turn.pop_back();

    feedback_error.clear();
    fail_reason = FailReason::None;
    chart_entropy.clear();
    chart_minimax.clear();

    // Re-launch scoring so the solver picks the same guess again.
    launch_scoring();
    return true;
}

// ── AppState::step_auto_solve ─────────────────────────────────────────────────

void AppState::step_auto_solve(double current_time_s) {
    if (!secret.has_value())
        return;
    if (phase != SolvePhase::AutoPaused && phase != SolvePhase::AutoPlaying)
        return;

    Feedback fb = fb_table->get(pending_guess, *secret);
    uint8_t blacks, whites;
    unpack_feedback(fb, *cfg, blacks, whites);
    last_auto_step_time = current_time_s;
    apply_feedback(blacks, whites);
}

// ── AppState::set_mode ────────────────────────────────────────────────────────

void AppState::set_mode(AppMode m) {
    mode = m;
    reset();
}

// ── AppState::encode_secret_input ─────────────────────────────────────────────

Code AppState::encode_secret_input() const {
    Code c = 0;
    uint32_t base = 1;
    for (int p = 0; p < static_cast<int>(cfg->positions); ++p) {
        int digit = std::clamp(secret_input[p] - 1, 0, static_cast<int>(cfg->colors) - 1);
        c += static_cast<Code>(digit) * base;
        base *= cfg->colors;
    }
    return c;
}
