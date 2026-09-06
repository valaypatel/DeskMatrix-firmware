// firmware/DeskMatrix/screens/SpotifyScreen.h
#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <string>

// Renders the current Spotify album art as a spinning vinyl record.
// Re-fetches/decodes the JPEG only when `albumArtUrl` differs from what's
// already loaded (see albumArtChanged() in services/SpotifyService.h) —
// cheap no-op otherwise. Spins while `isPlaying` is true, freezes at its
// current angle when false (matching a real record on pause). Self-paced
// (like drawScreensaverFrame()): call every loop() iteration, not gated by
// any fixed tick — it flips the display buffer itself whenever it redraws.
void drawSpotifyScreen(MatrixPanel_I2S_DMA* display, const std::string& albumArtUrl, bool isPlaying);
