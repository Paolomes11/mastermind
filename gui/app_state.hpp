#pragma once
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "code.hpp"
#include "config.hpp"
#include "feedback.hpp"
#include "solver.hpp"
#include "strategy.hpp"

enum class AppMode {
    Guided,    // Human thinks of a secret; solver suggests guesses, user provides feedback
    AutoSolve  // Human picks the secret; solver animates the solution step-by-step
};

enum class SolvePhase {
    Idle,              // No game in progress (initial state or after Reset)
    Computing,         // Background thread running strategy scoring
    AwaitingFeedback,  // Guided: guess shown, waiting for user to enter blacks/whites
    AutoPaused,        // AutoSolve: ready to step, paused
    AutoPlaying,       // AutoSolve: auto-advancing on timer
    Solved,            // Game won
    Failed             // Could not solve within max turns
};

enum class FailReason {
    None,
    EmptyCandidates,  // feedback was inconsistent; no valid code remains
    MaxTurns,         // hit the turn limit without solving
};

struct TurnRecord {
    Code guess;
    uint8_t blacks;
    uint8_t whites;
    int candidates_before;          // number of candidates when this guess was chosen
    std::vector<Code> candidates_snapshot;  // candidates list before this turn (for undo)
};

struct ScoringResult {
    std::vector<ScoredGuess> entropy_scores;  // top 10, descending entropy
    std::vector<ScoredGuess> minimax_scores;  // top 10, ascending worst-case size
    Code chosen_guess;
};

struct AppState {
    // ── Configuration (applied on Reset) ─────────────────────────────────────
    int colors_ui = 6;
    int positions_ui = 4;
    int strategy_idx = 0;  // 0=entropy, 1=minimax, 2=random
    static constexpr const char* kStrategyNames[] = {"entropy", "minimax", "random"};

    // Mapping: digit d → PEG_COLORS index (allows rearranging which color appears at each slot)
    int color_perm[8] = {0, 1, 2, 3, 4, 5, 6, 7};

    AppMode mode = AppMode::Guided;
    SolvePhase phase = SolvePhase::Idle;
    FailReason fail_reason = FailReason::None;

    // ── Core objects (rebuilt on Reset) ──────────────────────────────────────
    std::unique_ptr<GameConfig> cfg;
    std::unique_ptr<FeedbackTable> fb_table;
    std::unique_ptr<Strategy> strategy;
    std::vector<Code> all_codes;
    std::vector<Code> candidates;

    // ── Game history ──────────────────────────────────────────────────────────
    std::vector<TurnRecord> history;

    // ── Chart data (populated when scoring future resolves) ───────────────────
    std::vector<ScoredGuess> chart_entropy;
    std::vector<ScoredGuess> chart_minimax;
    std::vector<int> candidates_per_turn;  // candidates_before each turn

    // ── Background scoring ────────────────────────────────────────────────────
    std::future<ScoringResult> score_future;
    bool computing = false;
    Code pending_guess = 0;  // set when future resolves

    // ── AutoSolve state ───────────────────────────────────────────────────────
    std::optional<Code> secret;
    int secret_input[6] = {1, 1, 1, 1, 1, 1};  // 1-based digit per position
    float auto_step_delay_s = 0.8f;
    double last_auto_step_time = 0.0;

    // ── Guided feedback input ─────────────────────────────────────────────────
    int input_blacks = 0;
    int input_whites = 0;
    std::string feedback_error;  // non-empty → show error

    // ── Fullscreen / UI scale (managed by main_gui.cpp) ──────────────────────
    bool request_fullscreen_toggle = false;
    bool is_fullscreen = false;

    // ── Methods ───────────────────────────────────────────────────────────────

    // Rebuild core objects and restart the game.
    // IMPORTANT: always synchronizes any in-flight score_future first.
    void reset();

    // Launch background thread to score candidates and choose next guess.
    void launch_scoring();

    // Called every frame: checks if score_future is ready and applies result.
    void poll_scoring(double current_time_s);

    // Apply feedback (blacks, whites) to the pending_guess.
    // Returns false and sets feedback_error if the input is invalid.
    bool apply_feedback(uint8_t blacks, uint8_t whites);

    // Undo the last submitted turn. Returns false if nothing to undo.
    bool undo_last_turn();

    // Advance one step in AutoSolve mode (compute feedback from secret).
    void step_auto_solve(double current_time_s);

    // Set game mode and reset.
    void set_mode(AppMode m);

    // Encode secret_input[] into a Code for the current config.
    Code encode_secret_input() const;
};
