// tests/native/test_gif_validation.cpp
#include "test_framework.h"
#include "GifValidation.h"

int main() {
    const uint8_t gif87a[] = {'G','I','F','8','7','a', 0x00};
    CHECK(isValidGifHeader(gif87a, sizeof(gif87a)));

    const uint8_t gif89a[] = {'G','I','F','8','9','a', 0x00};
    CHECK(isValidGifHeader(gif89a, sizeof(gif89a)));

    const uint8_t notAGif[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10}; // JPEG magic
    CHECK(!isValidGifHeader(notAGif, sizeof(notAGif)));

    const uint8_t tooShort[] = {'G','I','F'};
    CHECK(!isValidGifHeader(tooShort, sizeof(tooShort)));

    CHECK(!isValidGifHeader(nullptr, 0));

    TEST_SUMMARY();
}
