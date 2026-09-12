// tests/native/test_spotify_service.cpp
#include "test_framework.h"
#include "services/SpotifyService.h"

int main() {
    CHECK(albumArtChanged("", "https://i.scdn.co/image/abc"));
    CHECK(albumArtChanged("https://i.scdn.co/image/abc", "https://i.scdn.co/image/def"));
    CHECK(!albumArtChanged("https://i.scdn.co/image/abc", "https://i.scdn.co/image/abc"));
    CHECK(!albumArtChanged("https://i.scdn.co/image/abc", "")); // new URL empty: nothing to draw, don't treat as a change

    TEST_SUMMARY();
}
