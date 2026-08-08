// firmware/DeskMatrix/ConfigModel.h
#pragma once
#include <string>
#include <vector>

struct WidgetConfig {
    std::string id;
    std::string type;
    std::string style;
    std::string color;
    std::string icon;
    int x = 0, y = 0, w = 0, h = 0;
    std::string dataSource;
};

struct DataSourceConfig {
    std::string id;
    std::string type;
    int pollSec = 300;
    std::string location;
};

struct AppConfig {
    std::vector<WidgetConfig> homeWidgets;
    std::vector<DataSourceConfig> dataSources;
    std::string dndArt = "dnd_default";
    std::string brbArt = "brb_default";
};

// Returns true and fills `out` on success; returns false and fills `error` on failure.
// A widget missing `id` or `type` is treated as invalid.
bool parseConfig(const std::string& json, AppConfig& out, std::string& error);

std::string serializeConfig(const AppConfig& config);
