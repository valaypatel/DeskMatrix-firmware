// emulator/spotify_test.cpp
// Manual visual check for Task 5: renders the Spotify screen with a fake
// (never-fetchable) album art URL using the real SpotifyScreen.cpp code,
// unchanged. Since the stub HTTPClient always fails, this always shows the
// idle ring (fillScreen ring pattern) -- the intended v1 behavior.
#include "NativePanel.h"
#include "../firmware/DeskMatrix/screens/SpotifyScreen.h"

int main() {
    NativePanel panel(64, 64, 8);
    while (panel.pollEvents()) {
        drawSpotifyScreen(&panel, "https://example.com/fake-album-art.jpg", true);
        SDL_Delay(16);
    }
    return 0;
}
