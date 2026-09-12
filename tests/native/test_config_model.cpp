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
    CHECK_EQ(cfg.brightness, 90);

    // No spotify block yet (fresh device): defaults apply, no error
    AppConfig fresh;
    std::string freshError;
    CHECK(parseConfig(R"({"dnd":{"art":"dnd_default"},"brb":{"art":"brb_default"}})", fresh, freshError));
    CHECK_EQ(fresh.spotify.clientId, "");
    CHECK_EQ(fresh.spotify.pollSec, 5);
    CHECK_EQ(fresh.brightness, 90);

    // Brightness is clamped to 0-255
    AppConfig clampedHigh;
    std::string clampedHighError;
    CHECK(parseConfig(R"({"brightness": 999})", clampedHigh, clampedHighError));
    CHECK_EQ(clampedHigh.brightness, 255);

    AppConfig clampedLow;
    std::string clampedLowError;
    CHECK(parseConfig(R"({"brightness": -50})", clampedLow, clampedLowError));
    CHECK_EQ(clampedLow.brightness, 0);

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
    CHECK_EQ(reparsed.brightness, cfg.brightness);

    // canvasJson: absent defaults to empty, no error
    AppConfig noCanvas;
    std::string noCanvasErr;
    CHECK(parseConfig(R"({})", noCanvas, noCanvasErr));
    CHECK_EQ(noCanvas.canvasJson, "");

    // canvasJson: valid theme JSON round-trips through parse -> serialize -> parse
    AppConfig withCanvas;
    withCanvas.canvasJson = R"({"setup":[{"type":"rect","x":0,"y":0,"width":10,"height":10,"color":1}]})";
    std::string withCanvasSerialized = serializeConfig(withCanvas);
    AppConfig reparsedCanvas;
    std::string reparsedCanvasErr;
    CHECK(parseConfig(withCanvasSerialized, reparsedCanvas, reparsedCanvasErr));
    CHECK_EQ(reparsedCanvas.canvasJson, withCanvas.canvasJson);

    // canvasJson: over 64KB is rejected
    AppConfig oversizedCanvas;
    oversizedCanvas.canvasJson = std::string(70000, 'x');
    std::string oversizedSerialized = serializeConfig(oversizedCanvas);
    AppConfig rejectedCanvas;
    std::string rejectedCanvasErr;
    CHECK(!parseConfig(oversizedSerialized, rejectedCanvas, rejectedCanvasErr));
    CHECK(!rejectedCanvasErr.empty());

    // canvasJson: valid JSON but missing setup/loop arrays is rejected
    AppConfig badStructureCfg;
    badStructureCfg.canvasJson = R"({"name":"missing setup and loop"})";
    std::string badStructureSerialized = serializeConfig(badStructureCfg);
    AppConfig rejectedStructure;
    std::string rejectedStructureErr;
    CHECK(!parseConfig(badStructureSerialized, rejectedStructure, rejectedStructureErr));
    CHECK(!rejectedStructureErr.empty());

    TEST_SUMMARY();
}
