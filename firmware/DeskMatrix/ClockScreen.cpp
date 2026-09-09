// firmware/DeskMatrix/ClockScreen.cpp
// Lives at the sketch root (not screens/) like ScreensaverScreen.cpp,
// SpotifyScreen.cpp, DndScreen.cpp and BrbScreen.cpp — Arduino/arduino-cli
// only auto-compiles .cpp files found directly in the sketch root
// directory, not in subdirectories, so ClockScreen.h stays under screens/
// (found via the include search path) while its .cpp lives here.
#include "screens/ClockScreen.h"

#include "lib/cw-gfx-engine/Locator.h"
#include "lib/cw-commons/CWDateTime.h"

#include "screens/clockfaces/mario/Clockface.h"
#include "screens/clockfaces/words/Clockface.h"
#include "screens/clockfaces/pacman/Clockface.h"

#include "config.h"

// The single display instance owned by DeskMatrix.ino (see initPanel()) —
// referenced directly here the same way ConfigServer.cpp references
// `extern WiFiManager wm;`. loadClockFace() needs a display pointer to hand
// to Locator::provide()/each Clockface's constructor, and per the plan's
// interface it's called with no display argument (mirrors
// loadScreensaverGif()'s signature), so it reaches for the global directly.
extern MatrixPanel_I2S_DMA* dma_display;

namespace {
// Shared across whichever clockface is active — begin() only needs calling
// once, since NTP sync/timezone offset are already handled by
// DeskMatrix.ino's configTime() call (see CWDateTime.cpp's adaptation
// notes), not by this object.
CWDateTime g_dateTime;
bool g_dateTimeStarted = false;

IClockface* g_activeClockface = nullptr;
std::string g_activeName;

// Ported clockfaces draw incrementally (e.g. Mario's Block only redraws
// the digits when they change) — correct for a single persistent
// framebuffer, but wrong against this panel's true double buffering
// (mxconfig.double_buff = true, see initPanel()): flipDMABuffer()
// alternates between two distinct physical buffers, so partial draws
// straight to dma_display leave each buffer with a different, stale
// mix of past frames — visible as flashing/incorrect rendering (found
// on real hardware). Fixed the same way ScreensaverScreen.cpp's GIF
// playback handles delta-encoded frames: draw onto one persistent
// off-screen canvas, then blit the *entire* canvas into whichever
// physical buffer is currently being flipped to, every frame.
GFXcanvas16 g_canvas(PANEL_RES_X, PANEL_RES_Y);
}  // namespace

void loadClockFace(const std::string& name) {
  std::string resolved = (name == "words" || name == "pacman") ? name : "mario";
  if (g_activeClockface != nullptr && resolved == g_activeName) return; // already active: no-op

  if (!g_dateTimeStarted) {
    g_dateTime.begin("", true);
    g_dateTimeStarted = true;
  }

  delete g_activeClockface; // IClockface has a virtual destructor — see lib/cw-commons/IClockface.h
  g_activeClockface = nullptr;

  if (dma_display == nullptr) return; // called before initPanel(); caller error — bail safely, retried on next drawClockFrame()

  // Clockfaces draw onto the persistent canvas, not dma_display directly —
  // see g_canvas's comment above.
  if (resolved == "words") {
    g_activeClockface = new WordsClockface(&g_canvas);
  } else if (resolved == "pacman") {
    g_activeClockface = new PacmanClockface(&g_canvas);
  } else {
    g_activeClockface = new MarioClockface(&g_canvas);
  }
  g_canvas.fillScreen(0);
  g_activeClockface->setup(&g_dateTime);
  g_activeName = resolved;
}

void drawClockFrame(MatrixPanel_I2S_DMA* display) {
  if (g_activeClockface == nullptr) {
    // First call arrived before setup()'s loadClockFace() ran (shouldn't
    // happen in normal boot order, but fail safe rather than crash/blank
    // forever) — retry with whatever name was last requested, default mario.
    loadClockFace(g_activeName.empty() ? "mario" : g_activeName);
    if (g_activeClockface == nullptr) return;
  }
  g_activeClockface->update();
  // Blit the whole canvas into whichever physical buffer is about to
  // become visible, so both buffers always show a complete, current
  // frame regardless of double-buffer ping-pong parity.
  display->drawRGBBitmap(0, 0, g_canvas.getBuffer(), PANEL_RES_X, PANEL_RES_Y);
  display->flipDMABuffer();
}
