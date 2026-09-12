// firmware/DeskMatrix/IconRenderer.cpp
#include "IconRenderer.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <LittleFS.h>
#include <vector>

void drawIcon(MatrixPanel_I2S_DMA* display, const std::string& iconId, int x, int y, int w, int h) {
    if (!display || w <= 0 || h <= 0) return;

    std::string path = "/assets/" + iconId + ".bin";
    if (!LittleFS.exists(path.c_str())) return;

    File f = LittleFS.open(path.c_str(), "r");
    if (!f) return;

    const size_t kExpectedBytes = 64 * 64 * 2; // 64x64 RGB565, 2 bytes/pixel
    if ((size_t)f.size() != kExpectedBytes) {
        f.close();
        return;
    }

    std::vector<uint16_t> buffer(64 * 64);
    f.read(reinterpret_cast<uint8_t*>(buffer.data()), kExpectedBytes);
    f.close();

    for (int oy = 0; oy < h; oy++) {
        for (int ox = 0; ox < w; ox++) {
            IconSample s = sampleIconPixel(ox, oy, w, h);
            uint16_t color = buffer[s.sy * 64 + s.sx];
            display->drawPixel(x + ox, y + oy, color);
        }
    }
}
