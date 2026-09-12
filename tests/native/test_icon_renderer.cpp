#include "test_framework.h"
#include "IconRenderer.h"

int main() {
    // Output same size as source: 1:1 mapping
    IconSample s1 = sampleIconPixel(0, 0, 64, 64);
    CHECK_EQ(s1.sx, 0); CHECK_EQ(s1.sy, 0);
    IconSample s2 = sampleIconPixel(32, 32, 64, 64);
    CHECK_EQ(s2.sx, 32); CHECK_EQ(s2.sy, 32);
    IconSample s3 = sampleIconPixel(63, 63, 64, 64);
    CHECK_EQ(s3.sx, 63); CHECK_EQ(s3.sy, 63);

    // Downscale to 32x32 (half size): output pixel N maps to source pixel 2N
    IconSample s4 = sampleIconPixel(0, 0, 32, 32);
    CHECK_EQ(s4.sx, 0); CHECK_EQ(s4.sy, 0);
    IconSample s5 = sampleIconPixel(16, 16, 32, 32);
    CHECK_EQ(s5.sx, 32); CHECK_EQ(s5.sy, 32);
    IconSample s6 = sampleIconPixel(31, 31, 32, 32);
    CHECK_EQ(s6.sx, 62); CHECK_EQ(s6.sy, 62);

    // Downscale to a small, non-power-of-two flag size (12x8)
    IconSample s7 = sampleIconPixel(0, 0, 12, 8);
    CHECK_EQ(s7.sx, 0); CHECK_EQ(s7.sy, 0);
    IconSample s8 = sampleIconPixel(11, 7, 12, 8);
    CHECK_EQ(s8.sx, 58); CHECK_EQ(s8.sy, 56); // 11*64/12=58, 7*64/8=56

    TEST_SUMMARY();
}
