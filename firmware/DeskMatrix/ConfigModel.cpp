// firmware/DeskMatrix/ConfigModel.cpp
#include "ConfigModel.h"
#include <ArduinoJson.h>

bool parseConfig(const std::string& json, AppConfig& out, std::string& error) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        error = err.c_str();
        return false;
    }

    out.homeWidgets.clear();
    for (JsonObject w : doc["home"]["widgets"].as<JsonArray>()) {
        WidgetConfig wc;
        wc.id = std::string(w["id"] | "");
        wc.type = std::string(w["type"] | "");
        wc.style = std::string(w["style"] | "");
        wc.color = std::string(w["color"] | "");
        wc.icon = std::string(w["icon"] | "");
        wc.x = w["x"] | 0;
        wc.y = w["y"] | 0;
        wc.w = w["w"] | 0;
        wc.h = w["h"] | 0;
        wc.dataSource = std::string(w["dataSource"] | "");
        if (wc.id.empty() || wc.type.empty()) {
            error = "widget missing required id/type";
            return false;
        }
        out.homeWidgets.push_back(wc);
    }

    out.dataSources.clear();
    JsonObject sources = doc["dataSources"].as<JsonObject>();
    for (JsonPair kv : sources) {
        DataSourceConfig dsc;
        dsc.id = std::string(kv.key().c_str());
        JsonObject v = kv.value().as<JsonObject>();
        dsc.type = std::string(v["type"] | "");
        dsc.pollSec = v["pollSec"] | 300;
        dsc.location = std::string(v["location"] | "");
        out.dataSources.push_back(dsc);
    }

    out.dndArt = std::string(doc["dnd"]["art"] | "dnd_default");
    out.brbArt = std::string(doc["brb"]["art"] | "brb_default");

    return true;
}

std::string serializeConfig(const AppConfig& config) {
    JsonDocument doc;
    JsonArray widgets = doc["home"]["widgets"].to<JsonArray>();
    for (const auto& w : config.homeWidgets) {
        JsonObject wo = widgets.add<JsonObject>();
        wo["id"] = w.id;
        wo["type"] = w.type;
        wo["style"] = w.style;
        wo["color"] = w.color;
        wo["icon"] = w.icon;
        wo["x"] = w.x;
        wo["y"] = w.y;
        wo["w"] = w.w;
        wo["h"] = w.h;
        if (!w.dataSource.empty()) wo["dataSource"] = w.dataSource;
    }

    JsonObject sources = doc["dataSources"].to<JsonObject>();
    for (const auto& ds : config.dataSources) {
        JsonObject so = sources[ds.id].to<JsonObject>();
        so["type"] = ds.type;
        so["pollSec"] = ds.pollSec;
        if (!ds.location.empty()) so["location"] = ds.location;
    }

    doc["dnd"]["art"] = config.dndArt;
    doc["brb"]["art"] = config.brbArt;

    std::string out;
    serializeJson(doc, out);
    return out;
}
