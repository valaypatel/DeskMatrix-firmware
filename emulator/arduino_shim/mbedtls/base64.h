// emulator/arduino_shim/mbedtls/base64.h
//
// Minimal stand-in for ESP-IDF's mbedtls base64 decoder -- only
// mbedtls_base64_decode() is used, by CanvasClockface.cpp's
// base64DecodeGuarded() (see its two-call probe-then-decode pattern: a
// dst=nullptr call to learn the required output length, then a real
// decode call). Implements an actual, correct base64 decode (rather than
// an always-fail stub) so canvas presets with embedded base64 PNG sprite
// data (nyancat/starwars) decode correctly in the emulator, not just the
// no-sprite custom-JSON smoke test path.
#pragma once
#include <cstddef>
#include <cstdint>

#define MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL -0x002C
#define MBEDTLS_ERR_BASE64_INVALID_CHARACTER -0x002E

namespace {
inline int base64_char_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
}  // namespace

inline int mbedtls_base64_decode(unsigned char* dst, size_t dlen, size_t* olen,
                                  const unsigned char* src, size_t slen) {
    // Strip trailing padding/whitespace for length computation.
    size_t validChars = 0;
    size_t padding = 0;
    for (size_t i = 0; i < slen; i++) {
        unsigned char c = src[i];
        if (c == '=') {
            padding++;
        } else if (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            continue;
        } else {
            if (base64_char_value(c) < 0) return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
            validChars++;
        }
    }
    if (validChars == 0) {
        *olen = 0;
        return dst ? 0 : MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
    }

    size_t needed = (validChars * 3) / 4;
    // Adjust down for actual padding characters trailing the input.
    if (padding > 0 && needed > 0) needed -= (padding > 2 ? 2 : padding);

    *olen = needed;

    if (dst == nullptr) {
        return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
    }
    if (dlen < needed) {
        return MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL;
    }

    uint32_t accum = 0;
    int bits = 0;
    size_t outPos = 0;
    for (size_t i = 0; i < slen; i++) {
        unsigned char c = src[i];
        if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        int v = base64_char_value(c);
        if (v < 0) return MBEDTLS_ERR_BASE64_INVALID_CHARACTER;
        accum = (accum << 6) | static_cast<uint32_t>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (outPos < dlen) {
                dst[outPos++] = static_cast<unsigned char>((accum >> bits) & 0xFF);
            }
        }
    }
    *olen = outPos;
    return 0;
}
