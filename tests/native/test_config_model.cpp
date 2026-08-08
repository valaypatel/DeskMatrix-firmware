// tests/native/test_config_model.cpp
#include "test_framework.h"
#include "ConfigModel.h"

int main() {
    std::string validJson = R"({
        "home": { "widgets": [
            { "id": "w1", "type": "clock", "style": "digital", "color": "#FFDE59", "x": 0, "y": 0, "w": 24, "h": 10 },
            { "id": "w2", "type": "weather", "style": "icon_temp", "icon": "weather_sunny", "color": "#4C9AFF", "x": 0, "y": 40, "w": 16, "h": 16, "dataSource": "ds_weather" },
            { "id": "w4", "type": "clock", "style": "digital", "color": "#FFDE59", "location": "51.5074,-0.1278", "x": 0, "y": 16, "w": 32, "h": 16 }
        ]},
        "dataSources": {
            "ds_weather": { "type": "weather", "pollSec": 600, "location": "40.7128,-74.0060" }
        },
        "dnd": { "art": "dnd_default" },
        "brb": { "art": "brb_default" }
    })";

    AppConfig cfg;
    std::string error;
    CHECK(parseConfig(validJson, cfg, error));
    CHECK_EQ(cfg.homeWidgets.size(), (size_t)3);
    CHECK_EQ(cfg.homeWidgets[0].id, "w1");
    CHECK_EQ(cfg.homeWidgets[0].type, "clock");
    CHECK_EQ(cfg.homeWidgets[1].dataSource, "ds_weather");
    CHECK_EQ(cfg.homeWidgets[0].location, ""); // widget without location: empty, local clock
    CHECK_EQ(cfg.dataSources.size(), (size_t)1);
    CHECK_EQ(cfg.dataSources[0].id, "ds_weather");
    CHECK_EQ(cfg.dataSources[0].pollSec, 600);
    CHECK_EQ(cfg.dndArt, "dnd_default");

    // Missing required fields rejected
    AppConfig bad;
    std::string badError;
    std::string missingType = R"({"home":{"widgets":[{"id":"w1","x":0,"y":0,"w":1,"h":1}]}})";
    CHECK(!parseConfig(missingType, bad, badError));
    CHECK(!badError.empty());

    // Malformed JSON rejected
    AppConfig bad2;
    std::string badError2;
    CHECK(!parseConfig("{not json", bad2, badError2));

    // Round-trip: parse -> serialize -> parse gives back same widget count/ids
    std::string serialized = serializeConfig(cfg);
    AppConfig reparsed;
    std::string reparseError;
    CHECK(parseConfig(serialized, reparsed, reparseError));
    CHECK_EQ(reparsed.homeWidgets.size(), cfg.homeWidgets.size());
    CHECK_EQ(reparsed.homeWidgets[0].id, cfg.homeWidgets[0].id);
    CHECK_EQ(reparsed.dataSources[0].id, cfg.dataSources[0].id);
    CHECK_EQ(cfg.homeWidgets[2].location, "51.5074,-0.1278");
    CHECK_EQ(reparsed.homeWidgets[2].location, cfg.homeWidgets[2].location); // round-trips

    TEST_SUMMARY();
}
