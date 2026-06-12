#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "config.hpp"

using Code = uint32_t;

// Decode code index into per-position digits.
// digit at position p = (code / C^p) % C.
// digits[0] is the least-significant (rightmost displayed) position.
std::vector<uint8_t> decode(Code code, const GameConfig& cfg);

// Encode per-position digits into a code index.
Code encode(const std::vector<uint8_t>& digits, const GameConfig& cfg);

// Enumerate all N codes in order [0, N-1].
std::vector<Code> enumerate_all_codes(const GameConfig& cfg);

// Display code as string of color digits '1'..'8', left=most-significant position.
std::string code_to_string(Code code, const GameConfig& cfg);

// Parse a display string into a Code. Returns false on invalid input.
bool string_to_code(const std::string& s, const GameConfig& cfg, Code& out);
