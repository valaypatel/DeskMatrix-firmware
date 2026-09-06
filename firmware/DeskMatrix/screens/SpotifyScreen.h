// firmware/DeskMatrix/screens/SpotifyScreen.h
#pragma once
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <string>

// Draws the current Spotify album art full-screen. Only re-fetches/decodes
// the JPEG when `albumArtUrl` differs from what's already on screen (see
// albumArtChanged() in services/SpotifyService.h) — cheap no-op otherwise.
// Call this once per render tick while in ScreenMode::SPOTIFY_PLAYING.
void drawSpotifyScreen(MatrixPanel_I2S_DMA* display, const std::string& albumArtUrl);
