#pragma once
#include <cstdint>
#include <cstddef>

// True if `data` begins with a valid GIF file signature ("GIF87a" or
// "GIF89a") — the minimum check needed to reject non-GIF uploads before
// they're written to flash as the screensaver.
inline bool isValidGifHeader(const uint8_t* data, size_t len) {
    if (!data || len < 6) return false;
    bool is87a = data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
                 data[3] == '8' && data[4] == '7' && data[5] == 'a';
    bool is89a = data[0] == 'G' && data[1] == 'I' && data[2] == 'F' &&
                 data[3] == '8' && data[4] == '9' && data[5] == 'a';
    return is87a || is89a;
}
