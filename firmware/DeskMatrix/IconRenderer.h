#pragma once

#include <string>

class MatrixPanel_I2S_DMA; // forward-declared: keeps this header includable
                            // from native tests without pulling in Arduino headers

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

// Draws a stored icon asset (see POST /api/assets) into the given region,
// nearest-neighbor scaled from its native 64x64 to w x h. Does nothing
// (no crash) if the asset doesn't exist or isn't the expected size.
void drawIcon(MatrixPanel_I2S_DMA* display, const std::string& iconId, int x, int y, int w, int h);
