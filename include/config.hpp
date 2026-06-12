#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>

struct GameConfig {
    uint32_t colors;
    uint32_t positions;
    uint32_t total_codes;
    uint32_t pow_c[7]; // pow_c[p] = colors^p, p in [0, positions]

    static constexpr uint32_t PRECOMPUTE_THRESHOLD = 5000;

    GameConfig(uint32_t c, uint32_t p) : colors(c), positions(p) {
        if (c < 2 || c > 8)
            throw std::invalid_argument("colors must be in [2, 8]");
        if (p < 2 || p > 6)
            throw std::invalid_argument("positions must be in [2, 6]");
        pow_c[0] = 1;
        for (uint32_t i = 1; i <= p; ++i)
            pow_c[i] = pow_c[i - 1] * c;
        total_codes = pow_c[p];
    }

    bool use_precomputed_table() const { return total_codes <= PRECOMPUTE_THRESHOLD; }

    std::string description() const {
        return std::to_string(colors) + " colors, " + std::to_string(positions) +
               " positions (" + std::to_string(total_codes) + " codes)";
    }
};
