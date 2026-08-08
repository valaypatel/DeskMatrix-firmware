// firmware/DeskMatrix/ColorUtil.h
#pragma once
#include <cstdlib>
#include <string>

// Parses a "#RRGGBB" hex color string into packed 8-bit RGB components.
// Returns true and fills r,g,b on success; returns false (leaving r,g,b
// untouched) if `hex` isn't a well-formed "#RRGGBB" string, so callers can
// fall back to their own default color.
inline bool parseHexColor(const std::string& hex, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (hex.size() != 7 || hex[0] != '#') return false;
    long rgb = strtol(hex.c_str() + 1, nullptr, 16);
    r = (rgb >> 16) & 0xFF;
    g = (rgb >> 8) & 0xFF;
    b = rgb & 0xFF;
    return true;
}
