#include "feedback.hpp"
#include <algorithm>
#include <array>
#include <iostream>

Feedback compute_feedback(Code guess, Code secret, const GameConfig& cfg) {
    std::array<int, 8> freq_g{}, freq_s{};
    int blacks = 0;

    for (uint32_t p = 0; p < cfg.positions; ++p) {
        uint32_t g = (guess / cfg.pow_c[p]) % cfg.colors;
        uint32_t s = (secret / cfg.pow_c[p]) % cfg.colors;
        if (g == s) {
            ++blacks;
        } else {
            ++freq_g[g];
            ++freq_s[s];
        }
    }

    int whites = 0;
    for (uint32_t c = 0; c < cfg.colors; ++c)
        whites += std::min(freq_g[c], freq_s[c]);

    return pack_feedback(static_cast<uint8_t>(blacks),
                         static_cast<uint8_t>(whites), cfg);
}

FeedbackTable::FeedbackTable(const GameConfig& cfg) : cfg_(cfg), precomputed_(false) {
    if (cfg.use_precomputed_table()) {
        precomputed_ = true;
        build_table();
    }
}

void FeedbackTable::build_table() {
    uint32_t N = cfg_.total_codes;
    table_.resize(static_cast<size_t>(N) * N);
    for (uint32_t g = 0; g < N; ++g)
        for (uint32_t s = 0; s < N; ++s)
            table_[static_cast<size_t>(g) * N + s] = compute_feedback(g, s, cfg_);
}

Feedback FeedbackTable::get(Code guess, Code secret) const {
    if (precomputed_)
        return table_[static_cast<size_t>(guess) * cfg_.total_codes + secret];
    return compute_feedback(guess, secret, cfg_);
}
