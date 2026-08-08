#pragma once

struct IconSample {
    int sx;
    int sy;
};

// Nearest-neighbor: which pixel of a `srcSize`x`srcSize` source bitmap
// should supply the color for output pixel (outX, outY) of a `outW`x`outH`
// destination. Pure integer math, no rounding surprises for our fixed
// srcSize=64 source icons.
inline IconSample sampleIconPixel(int outX, int outY, int outW, int outH, int srcSize = 64) {
    IconSample s;
    s.sx = outX * srcSize / outW;
    s.sy = outY * srcSize / outH;
    return s;
}
