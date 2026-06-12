#include "render.hpp"
#include "colors.hpp"
#include "code.hpp"
#include "feedback.hpp"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

// ── Helpers ───────────────────────────────────────────────────────────────────

static void draw_peg(ImDrawList* dl, ImVec2 center, float radius, ImVec4 color) {
    ImU32 fill  = to_u32(color);
    ImU32 rim   = IM_COL32(255, 255, 255, 45);
    dl->AddCircleFilled(center, radius, fill, 32);
    dl->AddCircle(center, radius, rim, 32, 1.5f);
}

static void draw_feedback_dots(ImDrawList* dl, ImVec2 top_left,
                                float dot_r, float gap,
                                int blacks, int whites, int positions) {
    // Layout: dots in a small grid (ceil(sqrt(positions)) columns)
    int cols = (positions <= 4) ? 2 : (int)std::ceil(std::sqrt((double)positions));
    int idx  = 0;
    for (int p = 0; p < positions; ++p) {
        int row = idx / cols;
        int col = idx % cols;
        ImVec2 c = {top_left.x + col * (dot_r * 2 + gap) + dot_r,
                    top_left.y + row * (dot_r * 2 + gap) + dot_r};
        ImVec4 color;
        if (p < blacks)          color = DOT_BLACK;
        else if (p < blacks + whites) color = DOT_WHITE;
        else                     color = DOT_EMPTY;
        dl->AddCircleFilled(c, dot_r, to_u32(color), 16);
        ++idx;
    }
}

// ── Top bar ───────────────────────────────────────────────────────────────────

static void render_top_bar(AppState& state) {
    if (!ImGui::BeginMainMenuBar()) return;

    bool game_active = (state.phase != SolvePhase::Idle &&
                        state.phase != SolvePhase::Solved &&
                        state.phase != SolvePhase::Failed);

    ImGui::PushItemWidth(110.f);

    ImGui::BeginDisabled(game_active);
    ImGui::SliderInt("Colors##cfg",    &state.colors_ui,    2, 8);
    ImGui::SliderInt("Positions##cfg", &state.positions_ui, 2, 6);

    const char* strat_items[] = {"Entropy", "Minimax", "Random"};
    ImGui::Combo("Strategy##cfg", &state.strategy_idx, strat_items, 3);
    ImGui::EndDisabled();

    ImGui::PopItemWidth();

    ImGui::Separator();

    // Mode toggle
    bool guided    = (state.mode == AppMode::Guided);
    bool autosolve = (state.mode == AppMode::AutoSolve);
    if (ImGui::RadioButton("Guided##mode",    guided))    { if (!guided)    state.set_mode(AppMode::Guided); }
    if (ImGui::RadioButton("Auto-Solve##mode", autosolve)) { if (!autosolve) state.set_mode(AppMode::AutoSolve); }

    ImGui::Separator();

    if (ImGui::Button("New Game")) state.reset();

    ImGui::Separator();

    // Fullscreen toggle button — F11 also works
    const char* fs_label = state.is_fullscreen ? "[x] Fullscreen  F11" : "[ ] Fullscreen  F11";
    if (ImGui::Button(fs_label)) state.request_fullscreen_toggle = true;

    // Status indicator
    ImGui::Separator();
    if (state.computing) {
        float t = (float)ImGui::GetTime();
        const char* spin = "|/-\\";
        ImGui::TextColored({0.9f, 0.7f, 0.2f, 1.f}, "Computing... %c", spin[(int)(t * 6) % 4]);
    } else if (state.phase == SolvePhase::Solved) {
        ImGui::TextColored({0.2f, 0.9f, 0.3f, 1.f}, "Solved in %d turns!", (int)state.history.size());
    } else if (state.phase == SolvePhase::Failed) {
        ImGui::TextColored({0.9f, 0.2f, 0.2f, 1.f}, "Failed after %d turns", (int)state.history.size());
    } else if (state.phase != SolvePhase::Idle) {
        ImGui::TextDisabled("Turn %d  |  %d candidates",
            (int)state.history.size() + 1,
            (int)state.candidates.size());
    }

    ImGui::EndMainMenuBar();
}

// ── Board panel ───────────────────────────────────────────────────────────────

static void render_board(AppState& state) {
    if (!state.cfg) return;

    int positions = state.cfg->positions;
    float peg_r   = 14.0f;
    float peg_gap  = 5.0f;
    float row_h    = peg_r * 2.0f + peg_gap + 4.0f;
    float dot_r    = 5.0f;
    float dot_gap  = 2.0f;

    // Estimate dot area width
    int dot_cols  = (positions <= 4) ? 2 : (int)std::ceil(std::sqrt((double)positions));
    float dot_area_w = dot_cols * (dot_r * 2 + dot_gap) + 4.0f;
    float row_w = positions * (peg_r * 2 + peg_gap) + dot_area_w + peg_gap * 2;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.15f, 1.f));
    ImGui::BeginChild("##board_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Render history rows
    for (int i = 0; i < (int)state.history.size(); ++i) {
        const TurnRecord& rec = state.history[i];
        ImVec2 cursor = ImGui::GetCursorScreenPos();

        // Row background
        ImVec2 row_tl = cursor;
        ImVec2 row_br = {row_tl.x + row_w, row_tl.y + row_h};
        bool is_last = (i == (int)state.history.size() - 1);
        ImU32 bg = is_last ? to_u32({0.25f, 0.25f, 0.15f, 0.5f}) : IM_COL32(0, 0, 0, 0);
        dl->AddRectFilled(row_tl, row_br, bg, 4.0f);

        // Peg circles
        for (int p = 0; p < positions; ++p) {
            uint8_t digit = static_cast<uint8_t>((rec.guess / state.cfg->pow_c[p]) % state.cfg->colors);
            ImVec2 center = {
                cursor.x + peg_gap + p * (peg_r * 2 + peg_gap) + peg_r,
                cursor.y + row_h * 0.5f
            };
            draw_peg(dl, center, peg_r, peg_color(digit));
        }

        // Feedback dots
        float dots_x = cursor.x + peg_gap + positions * (peg_r * 2 + peg_gap) + peg_gap;
        float dots_y = cursor.y + (row_h - ((int)std::ceil((double)positions / dot_cols)) * (dot_r * 2 + dot_gap)) * 0.5f;
        draw_feedback_dots(dl, {dots_x, dots_y}, dot_r, dot_gap,
                           rec.blacks, rec.whites, positions);

        // Turn number label
        ImGui::SetCursorScreenPos({cursor.x + row_w + 4.f, cursor.y + row_h * 0.35f});
        ImGui::TextDisabled("%d", i + 1);

        ImGui::Dummy(ImVec2(row_w + 24.f, row_h));
    }

    // ── Current guess (if computed) ──────────────────────────────────────────
    if (state.phase == SolvePhase::AwaitingFeedback ||
        state.phase == SolvePhase::AutoPaused ||
        state.phase == SolvePhase::AutoPlaying) {

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImVec2 row_tl = cursor;
        ImVec2 row_br = {row_tl.x + row_w, row_tl.y + row_h};

        // Highlighted background for current row
        dl->AddRectFilled(row_tl, row_br, to_u32(HIGHLIGHT), 4.0f);
        dl->AddRect(row_tl, row_br, IM_COL32(240, 200, 50, 80), 4.0f, 0, 1.5f);

        for (int p = 0; p < positions; ++p) {
            uint8_t digit = static_cast<uint8_t>(
                (state.pending_guess / state.cfg->pow_c[p]) % state.cfg->colors);
            ImVec2 center = {
                cursor.x + peg_gap + p * (peg_r * 2 + peg_gap) + peg_r,
                cursor.y + row_h * 0.5f
            };
            draw_peg(dl, center, peg_r, peg_color(digit));
        }

        // Dots area: show empty placeholders
        float dots_x = cursor.x + peg_gap + positions * (peg_r * 2 + peg_gap) + peg_gap;
        float dots_y = cursor.y + (row_h - ((int)std::ceil((double)positions / dot_cols)) * (dot_r * 2 + dot_gap)) * 0.5f;
        draw_feedback_dots(dl, {dots_x, dots_y}, dot_r, dot_gap, 0, 0, positions);

        ImGui::Dummy(ImVec2(row_w + 24.f, row_h));
    }

    // ── Feedback input (Guided mode) ─────────────────────────────────────────
    if (state.phase == SolvePhase::AwaitingFeedback && !state.computing) {
        ImGui::Separator();
        ImGui::TextUnformatted("Enter feedback for the guess above:");
        ImGui::PushItemWidth(120.f);
        ImGui::SliderInt("Black pegs##fb",  &state.input_blacks, 0, positions);
        int max_whites = std::max(0, positions - state.input_blacks);
        state.input_whites = std::min(state.input_whites, max_whites);
        ImGui::SliderInt("White pegs##fb",  &state.input_whites, 0, max_whites);
        ImGui::PopItemWidth();

        if (ImGui::Button("Submit Feedback", {-1, 0})) {
            state.apply_feedback(
                static_cast<uint8_t>(state.input_blacks),
                static_cast<uint8_t>(state.input_whites));
            state.input_blacks = 0;
            state.input_whites = 0;
        }

        if (!state.feedback_error.empty()) {
            ImGui::TextColored({0.9f, 0.2f, 0.2f, 1.f}, "%s", state.feedback_error.c_str());
        }
    }

    // ── AutoSolve: secret input + controls ───────────────────────────────────
    if (state.mode == AppMode::AutoSolve && state.phase == SolvePhase::Idle) {
        ImGui::Separator();
        ImGui::TextUnformatted("Enter the secret code:");
        ImGui::PushItemWidth(55.f);
        for (int p = 0; p < positions; ++p) {
            if (p > 0) ImGui::SameLine();
            char lbl[16];
            snprintf(lbl, sizeof(lbl), "##s%d", p);
            ImGui::InputInt(lbl, &state.secret_input[p], 0, 0);
            state.secret_input[p] = std::clamp(state.secret_input[p], 1, static_cast<int>(state.cfg->colors));
        }
        ImGui::PopItemWidth();

        if (ImGui::Button("Start Solving", {-1, 0})) {
            state.secret = state.encode_secret_input();
            state.launch_scoring();
        }
    }

    if (state.mode == AppMode::AutoSolve &&
        (state.phase == SolvePhase::AutoPaused || state.phase == SolvePhase::AutoPlaying)) {
        ImGui::Separator();

        bool playing = (state.phase == SolvePhase::AutoPlaying);
        if (ImGui::Button(playing ? "Pause" : "Play", {80, 0})) {
            state.phase = playing ? SolvePhase::AutoPaused : SolvePhase::AutoPlaying;
        }
        ImGui::SameLine();
        if (ImGui::Button("Step", {60, 0})) {
            state.step_auto_solve(ImGui::GetTime());
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.f);
        ImGui::SliderFloat("Speed##auto", &state.auto_step_delay_s, 0.2f, 3.0f, "%.1f s/step");
    }

    // ── Solved / Failed ───────────────────────────────────────────────────────
    if (state.phase == SolvePhase::Solved) {
        ImGui::Separator();
        ImGui::TextColored({0.2f, 0.9f, 0.3f, 1.f},
            "Solved in %d turns!", (int)state.history.size());
        if (ImGui::Button("Play Again", {-1, 0})) state.reset();
    } else if (state.phase == SolvePhase::Failed) {
        ImGui::Separator();
        ImGui::TextColored({0.9f, 0.2f, 0.2f, 1.f}, "Failed to solve.");
        if (ImGui::Button("Try Again", {-1, 0})) state.reset();
    }

    ImGui::PopStyleColor();
    ImGui::EndChild();
}

// ── Charts panel ──────────────────────────────────────────────────────────────

static void render_charts(AppState& state) {
    if (!state.cfg) return;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float chart_h = (avail.y - ImGui::GetStyle().ItemSpacing.y * 2) / 3.0f;

    // Build label arrays for bar charts
    auto make_labels = [&](const std::vector<ScoredGuess>& scores,
                           std::vector<std::string>& strs,
                           std::vector<const char*>& ptrs) {
        strs.clear(); ptrs.clear();
        for (auto& [code, _] : scores) {
            strs.push_back(code_to_string(code, *state.cfg));
            ptrs.push_back(strs.back().c_str());
        }
    };

    // ── 1. Entropy chart ──────────────────────────────────────────────────────
    {
        std::vector<std::string> e_strs;
        std::vector<const char*> e_ptrs;
        make_labels(state.chart_entropy, e_strs, e_ptrs);

        std::vector<double> e_vals, e_pos;
        for (int i = 0; i < (int)state.chart_entropy.size(); ++i) {
            e_vals.push_back(state.chart_entropy[i].second);
            e_pos.push_back((double)i);
        }

        if (ImPlot::BeginPlot("Entropy (bits) — top candidates",
                              ImVec2(-1, chart_h))) {
            ImPlot::SetupAxes("Guess", "H (bits)",
                              ImPlotAxisFlags_NoGridLines,
                              ImPlotAxisFlags_AutoFit);
            if (!e_ptrs.empty()) {
                ImPlot::SetupAxisTicks(ImAxis_X1, e_pos.data(),
                                       (int)e_ptrs.size(), e_ptrs.data(), false);
            }
            ImPlot::SetupAxisLimits(ImAxis_X1, -0.5, std::max(1.0, (double)e_vals.size() - 0.5),
                                    ImGuiCond_Always);

            ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(0.2f, 0.55f, 0.85f, 0.85f));
            if (!e_vals.empty())
                ImPlot::PlotBars("##H", e_vals.data(), (int)e_vals.size(), 0.7);
            ImPlot::PopStyleColor();

            // Highlight chosen guess
            if (!state.chart_entropy.empty()) {
                auto it = std::find_if(state.chart_entropy.begin(), state.chart_entropy.end(),
                    [&](const ScoredGuess& sg) { return sg.first == state.pending_guess; });
                if (it != state.chart_entropy.end()) {
                    int idx = (int)(it - state.chart_entropy.begin());
                    double hv = it->second;
                    ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(0.95f, 0.80f, 0.10f, 1.f));
                    ImPlot::PlotBars("##Hsel", &hv, 1, 0.7, (double)idx);
                    ImPlot::PopStyleColor();
                }
            }

            if (state.computing || state.chart_entropy.empty()) {
                ImPlot::PlotText("Computing...", 0, 0);
            }
            ImPlot::EndPlot();
        }
    }

    // ── 2. Minimax chart ──────────────────────────────────────────────────────
    {
        std::vector<std::string> m_strs;
        std::vector<const char*> m_ptrs;
        make_labels(state.chart_minimax, m_strs, m_ptrs);

        std::vector<double> m_vals, m_pos;
        for (int i = 0; i < (int)state.chart_minimax.size(); ++i) {
            m_vals.push_back(state.chart_minimax[i].second);
            m_pos.push_back((double)i);
        }

        if (ImPlot::BeginPlot("Minimax (worst-case partition) — lower is better",
                              ImVec2(-1, chart_h))) {
            ImPlot::SetupAxes("Guess", "Worst-case size",
                              ImPlotAxisFlags_NoGridLines,
                              ImPlotAxisFlags_AutoFit);
            if (!m_ptrs.empty()) {
                ImPlot::SetupAxisTicks(ImAxis_X1, m_pos.data(),
                                       (int)m_ptrs.size(), m_ptrs.data(), false);
            }
            ImPlot::SetupAxisLimits(ImAxis_X1, -0.5, std::max(1.0, (double)m_vals.size() - 0.5),
                                    ImGuiCond_Always);

            ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(0.85f, 0.40f, 0.10f, 0.85f));
            if (!m_vals.empty())
                ImPlot::PlotBars("##MM", m_vals.data(), (int)m_vals.size(), 0.7);
            ImPlot::PopStyleColor();

            // Highlight chosen guess
            if (!state.chart_minimax.empty()) {
                auto it = std::find_if(state.chart_minimax.begin(), state.chart_minimax.end(),
                    [&](const ScoredGuess& sg) { return sg.first == state.pending_guess; });
                if (it != state.chart_minimax.end()) {
                    int idx = (int)(it - state.chart_minimax.begin());
                    double mv = it->second;
                    ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(0.95f, 0.80f, 0.10f, 1.f));
                    ImPlot::PlotBars("##MMsel", &mv, 1, 0.7, (double)idx);
                    ImPlot::PopStyleColor();
                }
            }

            if (state.computing || state.chart_minimax.empty()) {
                ImPlot::PlotText("Computing...", 0, 0);
            }
            ImPlot::EndPlot();
        }
    }

    // ── 3. Candidates remaining line chart ────────────────────────────────────
    {
        int n = (int)state.candidates_per_turn.size();
        std::vector<double> xs, ys;
        xs.push_back(0.0);
        ys.push_back(state.cfg ? (double)state.cfg->total_codes : 0.0);
        for (int i = 0; i < n; ++i) {
            xs.push_back((double)(i + 1));
            ys.push_back((double)state.candidates_per_turn[i]);
        }
        // Add current candidate count as the next point
        if (state.phase != SolvePhase::Idle && !state.candidates.empty()) {
            xs.push_back((double)(n + 1));
            ys.push_back((double)state.candidates.size());
        }

        if (ImPlot::BeginPlot("Candidates Remaining per Turn",
                              ImVec2(-1, chart_h))) {
            ImPlot::SetupAxes("Turn", "Candidates",
                              ImPlotAxisFlags_None,
                              ImPlotAxisFlags_AutoFit);
            if (ys.size() > 1 && ys[0] > 10)
                ImPlot::SetupAxisScale(ImAxis_Y1, ImPlotScale_Log10);

            ImPlot::PushStyleColor(ImPlotCol_Line, ImVec4(0.10f, 0.80f, 0.45f, 1.f));
            ImPlot::PushStyleColor(ImPlotCol_Fill, ImVec4(0.10f, 0.80f, 0.45f, 0.20f));
            if (xs.size() >= 2)
                ImPlot::PlotShaded("##cand", xs.data(), ys.data(), (int)xs.size(), -INFINITY);
            if (!xs.empty())
                ImPlot::PlotLine("Candidates##line", xs.data(), ys.data(), (int)xs.size());
            ImPlot::PopStyleColor(2);

            if (n == 0) ImPlot::PlotText("Start a game to see data", 0.5, 100);
            ImPlot::EndPlot();
        }
    }
}

// ── Main render entry point ───────────────────────────────────────────────────

void render_frame(AppState& state, double current_time_s) {
    state.poll_scoring(current_time_s);

    // Auto-advance in AutoPlaying mode
    if (state.phase == SolvePhase::AutoPlaying && !state.computing) {
        double elapsed = current_time_s - state.last_auto_step_time;
        if (elapsed >= (double)state.auto_step_delay_s) {
            state.step_auto_solve(current_time_s);
        }
    }

    render_top_bar(state);

    // Two-panel layout below the menu bar
    ImGuiIO& io = ImGui::GetIO();
    float menu_h = ImGui::GetFrameHeight();
    float avail_w = io.DisplaySize.x;
    float avail_h = io.DisplaySize.y - menu_h;
    float board_w = avail_w * 0.38f;

    // Left panel — game board
    ImGui::SetNextWindowPos({0.f, menu_h});
    ImGui::SetNextWindowSize({board_w, avail_h});
    ImGui::Begin("Game Board", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoBringToFrontOnFocus);
    render_board(state);
    ImGui::End();

    // Right panel — charts
    ImGui::SetNextWindowPos({board_w, menu_h});
    ImGui::SetNextWindowSize({avail_w - board_w, avail_h});
    ImGui::Begin("Analysis", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove     | ImGuiWindowFlags_NoCollapse);
    render_charts(state);
    ImGui::End();
}
