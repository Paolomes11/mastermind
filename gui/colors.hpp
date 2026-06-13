#pragma once
#include <imgui.h>

#include <array>
#include <cstdint>

// 8 visually distinct peg colors (indices 0–7 correspond to digit values 0–7)
inline constexpr std::array<ImVec4, 8> PEG_COLORS = {{
    {0.85f, 0.15f, 0.15f, 1.0f},  // 0: Red
    {0.20f, 0.55f, 0.85f, 1.0f},  // 1: Blue
    {0.10f, 0.72f, 0.20f, 1.0f},  // 2: Green
    {0.92f, 0.78f, 0.05f, 1.0f},  // 3: Yellow
    {0.85f, 0.42f, 0.08f, 1.0f},  // 4: Orange
    {0.62f, 0.15f, 0.78f, 1.0f},  // 5: Purple
    {0.10f, 0.75f, 0.75f, 1.0f},  // 6: Cyan
    {0.92f, 0.45f, 0.65f, 1.0f},  // 7: Pink
}};

inline constexpr ImVec4 PEG_EMPTY = {0.22f, 0.22f, 0.22f, 1.0f};
inline constexpr ImVec4 DOT_BLACK = {0.05f, 0.05f, 0.05f, 1.0f};
inline constexpr ImVec4 DOT_WHITE = {0.90f, 0.90f, 0.90f, 1.0f};
inline constexpr ImVec4 DOT_EMPTY = {0.28f, 0.28f, 0.28f, 0.45f};
inline constexpr ImVec4 HIGHLIGHT = {0.98f, 0.78f, 0.10f, 0.18f};  // current row glow

inline ImU32 to_u32(ImVec4 c) {
    return IM_COL32(static_cast<int>(c.x * 255), static_cast<int>(c.y * 255),
                    static_cast<int>(c.z * 255), static_cast<int>(c.w * 255));
}

inline ImVec4 peg_color(uint8_t digit) {
    return PEG_COLORS[digit % 8];
}

inline ImVec4 peg_color_mapped(uint8_t digit, const int perm[8]) {
    return PEG_COLORS[perm[digit % 8]];
}
