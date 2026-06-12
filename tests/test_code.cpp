#include <doctest/doctest.h>

#include "code.hpp"
#include "config.hpp"

TEST_CASE("encode and decode are inverses") {
    GameConfig cfg{6, 4};
    for (Code c = 0; c < cfg.total_codes; ++c) {
        auto digits = decode(c, cfg);
        CHECK(encode(digits, cfg) == c);
    }
}

TEST_CASE("decode returns digits in valid range") {
    GameConfig cfg{6, 4};
    for (Code c = 0; c < cfg.total_codes; c += 7) {
        auto digits = decode(c, cfg);
        REQUIRE(static_cast<uint32_t>(digits.size()) == cfg.positions);
        for (auto d : digits)
            CHECK(static_cast<uint32_t>(d) < cfg.colors);
    }
}

TEST_CASE("enumerate_all_codes returns correct count") {
    for (uint32_t colors : {2u, 4u, 6u, 8u}) {
        for (uint32_t pos : {2u, 3u, 4u}) {
            GameConfig cfg{colors, pos};
            auto codes = enumerate_all_codes(cfg);
            CHECK(codes.size() == cfg.total_codes);
        }
    }
}

TEST_CASE("enumerate_all_codes returns all codes in order") {
    GameConfig cfg{4, 3};
    auto codes = enumerate_all_codes(cfg);
    for (Code c = 0; c < cfg.total_codes; ++c)
        CHECK(codes[c] == c);
}

TEST_CASE("code_to_string and string_to_code are inverses") {
    GameConfig cfg{6, 4};
    for (Code c = 0; c < cfg.total_codes; c += 13) {
        std::string s = code_to_string(c, cfg);
        Code out;
        CHECK(string_to_code(s, cfg, out));
        CHECK(out == c);
    }
}

TEST_CASE("string_to_code rejects invalid input") {
    GameConfig cfg{6, 4};
    Code out;
    CHECK_FALSE(string_to_code("", cfg, out));
    CHECK_FALSE(string_to_code("123", cfg, out));    // too short
    CHECK_FALSE(string_to_code("12345", cfg, out));  // too long
    CHECK_FALSE(string_to_code("1239", cfg, out));   // '9' out of range for 6 colors
    CHECK_FALSE(string_to_code("0123", cfg, out));   // '0' not a valid digit
}

TEST_CASE("code_to_string length equals positions") {
    GameConfig cfg{6, 4};
    for (Code c = 0; c < cfg.total_codes; c += 19) {
        CHECK(code_to_string(c, cfg).size() == cfg.positions);
    }
}
