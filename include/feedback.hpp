#pragma once
#include "code.hpp"
#include "config.hpp"
#include <cstdint>
#include <vector>

// Feedback packed as: blacks * (positions + 2) + whites.
// Injective for valid (blacks, whites) pairs; max value = P*(P+2) <= 48 for P<=6.
using Feedback = uint8_t;

inline Feedback pack_feedback(uint8_t blacks, uint8_t whites, const GameConfig& cfg) {
    return static_cast<Feedback>(blacks * (cfg.positions + 2) + whites);
}

inline void unpack_feedback(Feedback fb, const GameConfig& cfg,
                             uint8_t& blacks, uint8_t& whites) {
    auto stride = static_cast<uint8_t>(cfg.positions + 2);
    blacks = fb / stride;
    whites = fb % stride;
}

inline Feedback winning_feedback(const GameConfig& cfg) {
    return pack_feedback(static_cast<uint8_t>(cfg.positions), 0, cfg);
}

// Compute feedback for (guess, secret) in O(P) time, O(C) stack space.
Feedback compute_feedback(Code guess, Code secret, const GameConfig& cfg);

// Feedback lookup: precomputes N×N table when N <= PRECOMPUTE_THRESHOLD.
class FeedbackTable {
public:
    explicit FeedbackTable(const GameConfig& cfg);

    Feedback get(Code guess, Code secret) const;

    bool is_precomputed() const { return precomputed_; }
    const GameConfig& config() const { return cfg_; }

private:
    const GameConfig& cfg_;
    bool precomputed_;
    std::vector<uint8_t> table_; // row-major: table_[guess * N + secret]

    void build_table();
};
