#include <doctest/doctest.h>

#include "code.hpp"
#include "config.hpp"
#include "feedback.hpp"

static Code from_str(const std::string& s, const GameConfig& cfg) {
    Code c = 0;
    string_to_code(s, cfg, c);
    return c;
}

TEST_CASE("pack/unpack round-trip") {
    GameConfig cfg{6, 4};
    for (uint8_t b = 0; b <= 4; ++b) {
        for (uint8_t w = 0; b + w <= 4; ++w) {
            uint8_t ob, ow;
            unpack_feedback(pack_feedback(b, w, cfg), cfg, ob, ow);
            CHECK(ob == b);
            CHECK(ow == w);
        }
    }
}

TEST_CASE("perfect match gives winning feedback") {
    GameConfig cfg{6, 4};
    Feedback win = winning_feedback(cfg);
    for (Code c = 0; c < cfg.total_codes; c += 17) {
        CHECK(compute_feedback(c, c, cfg) == win);
    }
}

TEST_CASE("no colour overlap gives zero blacks and whites") {
    GameConfig cfg{6, 4};
    Code guess = from_str("1111", cfg);   // all digit 0
    Code secret = from_str("2222", cfg);  // all digit 1
    uint8_t blacks, whites;
    unpack_feedback(compute_feedback(guess, secret, cfg), cfg, blacks, whites);
    CHECK(blacks == 0);
    CHECK(whites == 0);
}

TEST_CASE("all colours correct but wrong position") {
    GameConfig cfg{6, 4};
    Code guess = from_str("1234", cfg);
    Code secret = from_str("4321", cfg);
    uint8_t blacks, whites;
    unpack_feedback(compute_feedback(guess, secret, cfg), cfg, blacks, whites);
    CHECK(blacks == 0);
    CHECK(whites == 4);
}

TEST_CASE("partial match: two blacks zero whites") {
    GameConfig cfg{6, 4};
    Code guess = from_str("1122", cfg);
    Code secret = from_str("1133", cfg);
    uint8_t blacks, whites;
    unpack_feedback(compute_feedback(guess, secret, cfg), cfg, blacks, whites);
    CHECK(blacks == 2);
    CHECK(whites == 0);
}

TEST_CASE("blacks and whites never exceed positions") {
    GameConfig cfg{4, 3};
    for (Code g = 0; g < cfg.total_codes; ++g) {
        for (Code s = 0; s < cfg.total_codes; ++s) {
            uint8_t blacks, whites;
            unpack_feedback(compute_feedback(g, s, cfg), cfg, blacks, whites);
            CHECK(blacks + whites <= cfg.positions);
        }
    }
}

TEST_CASE("FeedbackTable::get matches compute_feedback") {
    GameConfig cfg{6, 4};
    FeedbackTable ft{cfg};
    REQUIRE(ft.is_precomputed());
    for (Code g = 0; g < cfg.total_codes; g += 37) {
        for (Code s = 0; s < cfg.total_codes; s += 29) {
            CHECK(ft.get(g, s) == compute_feedback(g, s, cfg));
        }
    }
}

TEST_CASE("feedback is not symmetric for asymmetric inputs") {
    GameConfig cfg{6, 4};
    Code a = from_str("1123", cfg);
    Code b = from_str("1234", cfg);
    // blacks must be symmetric (same positions matched regardless of order)
    // but whites may differ — just verify we get a well-formed result both ways
    uint8_t b1, w1, b2, w2;
    unpack_feedback(compute_feedback(a, b, cfg), cfg, b1, w1);
    unpack_feedback(compute_feedback(b, a, cfg), cfg, b2, w2);
    CHECK(b1 == b2);  // blacks are always symmetric
    CHECK(b1 + w1 <= cfg.positions);
    CHECK(b2 + w2 <= cfg.positions);
}
