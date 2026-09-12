// firmware/DeskMatrix/CanvasClockface.cpp
// Lives at the sketch root, like ClockScreen.cpp/MarioBlock.cpp/etc. --
// arduino-cli only auto-compiles root-level .cpp files.
#include "screens/clockfaces/canvas/CanvasClockface.h"

#include <PNGdec.h>
#include "mbedtls/base64.h"
#include "esp_heap_caps.h"
#include <cstring>
#include <new>

#include "EzTimeFormat.h"

namespace {
// Shared PNGdec decode state. Only one CanvasClockface is ever being
// constructed at a time (main/loop task), so a single static decoder
// instance is safe -- same approach Clockwise's own upstream Canvas
// clockface uses (jnthas/cw-cf-0x07's PNGRender.h).
//
// PNGDEC's decode state is ~44KB -- as a plain global it was internal RAM
// that's needed elsewhere (confirmed on real hardware: internal free heap
// was too tight for WiFiClientSecure's TLS handshake to Spotify's servers
// to even start). Lazily placed in PSRAM on first use instead; the
// reference lets every existing `g_png.foo()` call site stay unchanged.
PNG& png() {
    static PNG* p = new (heap_caps_malloc(sizeof(PNG), MALLOC_CAP_SPIRAM)) PNG();
    return *p;
}
#define g_png png()
bool g_pngDrawDirect = false;
Adafruit_GFX* g_pngDrawTarget = nullptr;
int16_t g_pngDrawX = 0;
int16_t g_pngDrawY = 0;
uint8_t* g_pngDecodeDst = nullptr;
uint16_t g_pngDecodeDstW = 0;
uint16_t g_pngDecodeDstH = 0;

int pngDrawCallback(PNGDRAW* pDraw) {
    // Defense in depth on top of the width/height guards at both call sites
    // (loadSpriteFrame/renderImageElement already reject w > 64 before
    // decoding) -- bail before getLineAsRGB565 ever writes into the
    // 64-wide stack buffer below, rather than clamping only the later
    // consumers of `line`.
    if (pDraw->iWidth > 64) return 1;
    uint16_t line[64];
    g_png.getLineAsRGB565(pDraw, line, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
    if (g_pngDrawDirect) {
        if (g_pngDrawTarget != nullptr) {
            g_pngDrawTarget->drawRGBBitmap(g_pngDrawX, g_pngDrawY + pDraw->y, line, pDraw->iWidth, 1);
        }
    } else if (g_pngDecodeDst != nullptr && pDraw->y < g_pngDecodeDstH) {
        memcpy(g_pngDecodeDst + (size_t)pDraw->y * g_pngDecodeDstW * 2, line, (size_t)pDraw->iWidth * 2);
    }
    return 1;
}

// Base64-decodes `base64` into `outRaw`. Returns false (leaving outRaw
// untouched) on failure or if the decoded size is implausible for a
// clock-sized sprite -- guards against a malformed/huge pasted value.
bool base64DecodeGuarded(const std::string& base64, std::vector<uint8_t>& outRaw) {
    size_t neededLen = 0;
    // Probe call: dst=nullptr always reports MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL
    // and sets neededLen -- that "failure" is expected and intentionally ignored.
    mbedtls_base64_decode(nullptr, 0, &neededLen, (const unsigned char*)base64.data(), base64.size());
    if (neededLen == 0 || neededLen > 8192) {
        return false;
    }
    outRaw.resize(neededLen);
    size_t actualLen = 0;
    if (mbedtls_base64_decode(outRaw.data(), outRaw.size(), &actualLen,
                               (const unsigned char*)base64.data(), base64.size()) != 0) {
        return false;
    }
    outRaw.resize(actualLen);
    return true;
}
}  // namespace

CanvasClockface::CanvasClockface(Adafruit_GFX* display, const char* themeJson) : display_(display) {
    JsonDocument doc;
    if (deserializeJson(doc, themeJson)) {
        valid_ = false;
        return;
    }

    bgColor_ = doc["bgColor"] | 0;
    int delay = doc["delay"] | 300;
    if (delay < 20) delay = 20;  // guard against a busy-loop from a 0ms theme
    delayMs_ = (uint16_t)delay;

    for (JsonVariantConst item : doc["setup"].as<JsonArrayConst>()) {
        SetupElement el;
        if (!parseSetupElement(item, el)) {
            continue;
        }
        setupElements_.push_back(el);
    }

    // sprites[i] is normally itself an array of {"image": "..."} frames (for
    // multi-frame animation, e.g. Nyan Cat's rainbow trail) but a custom
    // theme could provide a bare {"image": "..."} object for a single-frame
    // sprite -- handle both shapes.
    for (JsonVariantConst spriteEntry : doc["sprites"].as<JsonArrayConst>()) {
        std::vector<SpriteFrame> frames;
        if (spriteEntry.is<JsonArrayConst>()) {
            for (JsonVariantConst frameVar : spriteEntry.as<JsonArrayConst>()) {
                const char* img = frameVar["image"] | "";
                SpriteFrame frame;
                if (img[0] != '\0' && loadSpriteFrame(img, frame)) {
                    frames.push_back(frame);
                }
            }
        } else {
            const char* img = spriteEntry["image"] | "";
            SpriteFrame frame;
            if (img[0] != '\0' && loadSpriteFrame(img, frame)) {
                frames.push_back(frame);
            }
        }
        loadedSprites_.push_back(std::move(frames));
    }

    for (JsonVariantConst item : doc["loop"].as<JsonArrayConst>()) {
        const char* type = item["type"] | "";
        if (strcmp(type, "sprite") == 0) {
            LoopSprite ls;
            ls.spriteIndex = item["sprite"] | -1;
            ls.x = item["x"] | 0;
            ls.y = item["y"] | 0;
            loopSprites_.push_back(ls);
            continue;
        }
        SetupElement el;
        if (!parseSetupElement(item, el)) {
            continue;
        }
        if (el.type == SetupElement::DATETIME) {
            // renderDatetimeElements() already redraws every DATETIME
            // element out of setupElements_ on its own 1s cadence -- route
            // a datetime found in loop[] through that same path instead of
            // creating a second redraw path for it.
            setupElements_.push_back(el);
        } else if (el.type == SetupElement::IMAGE) {
            // Unlike sprites (base64+PNG decoded once at construction into
            // a PSRAM buffer, then just blitted), renderImageElement()
            // re-runs base64 decode + PNG open + PNG decode from scratch on
            // every call -- redrawing it on every delayMs_ tick (as often
            // as ~50x/sec) would be a hot-path decode loop plus repeated
            // [canvas] failure logging on a malformed image. Not supported
            // in loop[]; use setup[] for a static image instead.
        } else {
            loopElements_.push_back(el);
        }
    }

    valid_ = true;
}

bool CanvasClockface::parseSetupElement(JsonVariantConst item, SetupElement& outEl) {
    const char* type = item["type"] | "";
    outEl.x = item["x"] | 0;
    outEl.y = item["y"] | 0;
    outEl.x1 = item["x1"] | 0;
    outEl.y1 = item["y1"] | 0;
    outEl.width = item["width"] | 0;
    outEl.height = item["height"] | 0;
    outEl.color = item["color"] | 0;
    outEl.fgColor = item["fgColor"] | 0xFFFF;
    outEl.bgColor = item["bgColor"] | 0;
    outEl.font = std::string(item["font"] | "");

    if (strcmp(type, "rect") == 0) {
        outEl.type = SetupElement::RECT;
    } else if (strcmp(type, "fillrect") == 0) {
        outEl.type = SetupElement::FILLRECT;
    } else if (strcmp(type, "line") == 0) {
        outEl.type = SetupElement::LINE;
    } else if (strcmp(type, "text") == 0) {
        outEl.type = SetupElement::TEXT;
        outEl.content = std::string(item["content"] | "");
    } else if (strcmp(type, "datetime") == 0) {
        outEl.type = SetupElement::DATETIME;
        outEl.content = std::string(item["content"] | "H:i");
    } else if (strcmp(type, "image") == 0) {
        outEl.type = SetupElement::IMAGE;
        outEl.content = std::string(item["image"] | "");
    } else {
        return false;  // unrecognized element type
    }
    return true;
}

CanvasClockface::~CanvasClockface() {
    for (auto& frames : loadedSprites_) {
        for (auto& frame : frames) {
            if (frame.pixels != nullptr) heap_caps_free(frame.pixels);
        }
    }
}

bool CanvasClockface::loadSpriteFrame(const std::string& base64, SpriteFrame& outFrame) {
    std::vector<uint8_t> raw;
    if (!base64DecodeGuarded(base64, raw)) return false;

    if (g_png.openRAM(raw.data(), (int)raw.size(), pngDrawCallback) != PNG_SUCCESS) {
        return false;
    }
    int w = g_png.getWidth();
    int h = g_png.getHeight();
    if (w <= 0 || h <= 0 || w > 64 || h > 64) {
        g_png.close();
        return false;
    }

    uint16_t* pixels = (uint16_t*)heap_caps_malloc((size_t)w * h * 2, MALLOC_CAP_SPIRAM);
    if (pixels == nullptr) {
        g_png.close();
        return false;
    }

    g_pngDrawDirect = false;
    g_pngDecodeDst = (uint8_t*)pixels;
    g_pngDecodeDstW = (uint16_t)w;
    g_pngDecodeDstH = (uint16_t)h;
    bool ok = (g_png.decode(nullptr, 0) == PNG_SUCCESS);
    g_png.close();
    if (!ok) {
        heap_caps_free(pixels);
        return false;
    }

    outFrame.pixels = pixels;
    outFrame.width = (uint16_t)w;
    outFrame.height = (uint16_t)h;
    return true;
}

void CanvasClockface::renderShape(const SetupElement& el) {
    switch (el.type) {
        case SetupElement::RECT:
            display_->drawRect(el.x, el.y, el.width, el.height, el.color);
            break;
        case SetupElement::FILLRECT:
            display_->fillRect(el.x, el.y, el.width, el.height, el.color);
            break;
        case SetupElement::LINE:
            display_->drawLine(el.x, el.y, el.x1, el.y1, el.color);
            break;
        default:
            break;
    }
}

void CanvasClockface::setFontByName(const std::string& name) {
    if (name == "picopixel") {
        display_->setFont(&Picopixel);
    } else if (name == "square") {
        display_->setFont(&atariFont);
    } else if (name == "big") {
        display_->setFont(&hour8pt7b);
    } else if (name == "medium") {
        display_->setFont(&minute7pt7b);
    } else {
        display_->setFont();
    }
}

void CanvasClockface::renderText(const std::string& text, const SetupElement& el) {
    setFontByName(el.font);
    int16_t x1, y1;
    uint16_t w, h;
    display_->getTextBounds(text.c_str(), 0, 0, &x1, &y1, &w, &h);
    display_->fillRect(el.x + x1, el.y + y1, w, h, el.bgColor);
    display_->setTextColor(el.fgColor);
    display_->setCursor(el.x, el.y);
    display_->print(text.c_str());
}

void CanvasClockface::renderImageElement(const SetupElement& el) {
    std::vector<uint8_t> raw;
    if (!base64DecodeGuarded(el.content, raw)) return;
    if (g_png.openRAM(raw.data(), (int)raw.size(), pngDrawCallback) != PNG_SUCCESS) {
        return;
    }
    int w = g_png.getWidth();
    int h = g_png.getHeight();
    if (w <= 0 || h <= 0 || w > 64 || h > 64) {
        g_png.close();
        return;
    }
    g_pngDrawDirect = true;
    g_pngDrawTarget = display_;
    g_pngDrawX = el.x;
    g_pngDrawY = el.y;
    g_png.decode(nullptr, 0);
    g_png.close();
}

void CanvasClockface::renderDatetimeElements() {
    if (dateTime_ == nullptr) return;
    int hour = dateTime_->getHour();
    int minute = dateTime_->getMinute();
    int second = dateTime_->getSecond();
    int day = dateTime_->getDay();
    int month = dateTime_->getMonth();
    int year = dateTime_->getYear();
    for (const auto& el : setupElements_) {
        if (el.type != SetupElement::DATETIME) continue;
        renderText(formatEzTime(el.content, hour, minute, second, day, month, year), el);
    }
}

void CanvasClockface::setup(CWDateTime* dateTime) {
    dateTime_ = dateTime;
    display_->fillScreen(bgColor_);
    for (const auto& el : setupElements_) {
        switch (el.type) {
            case SetupElement::RECT:
            case SetupElement::FILLRECT:
            case SetupElement::LINE:
                renderShape(el);
                break;
            case SetupElement::TEXT:
                renderText(el.content, el);
                break;
            case SetupElement::IMAGE:
                renderImageElement(el);
                break;
            case SetupElement::DATETIME:
                break;  // drawn below, same code path as every later refresh
        }
    }
    renderDatetimeElements();
    lastLoopMs_ = millis();
    lastDateTimeMs_ = millis();
}

void CanvasClockface::update() {
    unsigned long now = millis();
    if (now - lastLoopMs_ >= delayMs_) {
        lastLoopMs_ = now;
        for (auto& ls : loopSprites_) {
            if (ls.spriteIndex < 0 || (size_t)ls.spriteIndex >= loadedSprites_.size()) continue;
            auto& frames = loadedSprites_[(size_t)ls.spriteIndex];
            if (frames.empty()) continue;
            const SpriteFrame& frame = frames[ls.currentFrame % frames.size()];
            if (frame.pixels != nullptr) {
                display_->drawRGBBitmap(ls.x, ls.y, frame.pixels, frame.width, frame.height);
            }
            if (frames.size() > 1) ls.currentFrame = (ls.currentFrame + 1) % frames.size();
        }
        for (const auto& el : loopElements_) {
            switch (el.type) {
                case SetupElement::RECT:
                case SetupElement::FILLRECT:
                case SetupElement::LINE:
                    renderShape(el);
                    break;
                case SetupElement::TEXT:
                    renderText(el.content, el);
                    break;
                case SetupElement::IMAGE:
                case SetupElement::DATETIME:
                    break;  // never populated here -- see constructor
            }
        }
    }
    if (now - lastDateTimeMs_ >= 1000) {
        lastDateTimeMs_ = now;
        renderDatetimeElements();
    }
}
