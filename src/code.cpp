#include "code.hpp"

std::vector<uint8_t> decode(Code code, const GameConfig& cfg) {
    std::vector<uint8_t> digits(cfg.positions);
    for (uint32_t p = 0; p < cfg.positions; ++p)
        digits[p] = static_cast<uint8_t>((code / cfg.pow_c[p]) % cfg.colors);
    return digits;
}

Code encode(const std::vector<uint8_t>& digits, const GameConfig& cfg) {
    Code code = 0;
    for (uint32_t p = 0; p < cfg.positions; ++p)
        code += static_cast<Code>(digits[p]) * cfg.pow_c[p];
    return code;
}

std::vector<Code> enumerate_all_codes(const GameConfig& cfg) {
    std::vector<Code> codes(cfg.total_codes);
    for (uint32_t i = 0; i < cfg.total_codes; ++i)
        codes[i] = i;
    return codes;
}

// Display from most-significant position (p = positions-1) to least (p = 0).
std::string code_to_string(Code code, const GameConfig& cfg) {
    std::string s(cfg.positions, '0');
    for (uint32_t p = 0; p < cfg.positions; ++p) {
        uint8_t digit = static_cast<uint8_t>((code / cfg.pow_c[p]) % cfg.colors);
        s[cfg.positions - 1 - p] = static_cast<char>('1' + digit);
    }
    return s;
}

bool string_to_code(const std::string& s, const GameConfig& cfg, Code& out) {
    if (s.size() != cfg.positions) return false;
    std::vector<uint8_t> digits(cfg.positions);
    for (uint32_t i = 0; i < cfg.positions; ++i) {
        char ch = s[i];
        if (ch < '1' || ch > static_cast<char>('0' + cfg.colors)) return false;
        // s[0] is most-significant position (p = positions-1)
        digits[cfg.positions - 1 - i] = static_cast<uint8_t>(ch - '1');
    }
    out = encode(digits, cfg);
    return true;
}
