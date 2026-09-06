// tests/native/test_config_model.cpp
#include "test_framework.h"
#include "ConfigModel.h"

int main() {
    std::string validJson = R"({
        "spotify": {
            "clientId": "abc123",
            "clientSecret": "shh",
            "refreshToken": "AQD...",
            "pollSec": 5
        },
        "dnd": { "art": "dnd_default" },
        "brb": { "art": "brb_default" }
    })";

    AppConfig cfg;
    std::string error;
    CHECK(parseConfig(validJson, cfg, error));
    CHECK_EQ(cfg.spotify.clientId, "abc123");
    CHECK_EQ(cfg.spotify.clientSecret, "shh");
    CHECK_EQ(cfg.spotify.refreshToken, "AQD...");
    CHECK_EQ(cfg.spotify.pollSec, 5);
    CHECK_EQ(cfg.dndArt, "dnd_default");
    CHECK_EQ(cfg.brbArt, "brb_default");

    // No spotify block yet (fresh device): defaults apply, no error
    AppConfig fresh;
    std::string freshError;
    CHECK(parseConfig(R"({"dnd":{"art":"dnd_default"},"brb":{"art":"brb_default"}})", fresh, freshError));
    CHECK_EQ(fresh.spotify.clientId, "");
    CHECK_EQ(fresh.spotify.pollSec, 5);

    // Malformed JSON rejected
    AppConfig bad;
    std::string badError;
    CHECK(!parseConfig("{not json", bad, badError));
    CHECK(!badError.empty());

    // Round-trip: parse -> serialize -> parse gives back the same values
    std::string serialized = serializeConfig(cfg);
    AppConfig reparsed;
    std::string reparseError;
    CHECK(parseConfig(serialized, reparsed, reparseError));
    CHECK_EQ(reparsed.spotify.clientId, cfg.spotify.clientId);
    CHECK_EQ(reparsed.spotify.refreshToken, cfg.spotify.refreshToken);
    CHECK_EQ(reparsed.spotify.pollSec, cfg.spotify.pollSec);
    CHECK_EQ(reparsed.dndArt, cfg.dndArt);

    TEST_SUMMARY();
}
