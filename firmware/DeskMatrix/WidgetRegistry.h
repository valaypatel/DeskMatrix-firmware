#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include "ConfigModel.h"

// displayHandle is void* (rather than a concrete display type) so this
// header — and the registry mechanics it defines — stay testable without
// any hardware/Arduino dependency. Real widget draw functions (Task 9+)
// cast it back to the concrete display type internally.
struct RenderContext {
    void* displayHandle = nullptr;
    const WidgetConfig* widget = nullptr;
};

using DrawFn = std::function<void(const RenderContext&)>;

class WidgetRegistry {
public:
    void registerType(const std::string& type, DrawFn fn) { entries_[type] = fn; }
    bool has(const std::string& type) const { return entries_.count(type) > 0; }

    bool draw(const std::string& type, const RenderContext& ctx) const {
        auto it = entries_.find(type);
        if (it == entries_.end()) return false;
        it->second(ctx);
        return true;
    }

private:
    std::unordered_map<std::string, DrawFn> entries_;
};
