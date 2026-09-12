// firmware/DeskMatrix/EzTimeFormat.cpp
#include "EzTimeFormat.h"
#include <cstdio>

namespace {
std::string pad2(int v) {
    char buf[4];
    snprintf(buf, sizeof(buf), "%02d", v);
    return std::string(buf);
}
}  // namespace

std::string formatEzTime(const std::string& format, int hour24, int minute,
                          int second, int day, int month, int year) {
    int hour12 = hour24 % 12;
    if (hour12 == 0) hour12 = 12;

    std::string out;
    out.reserve(format.size());
    for (char c : format) {
        switch (c) {
            case 'H': out += pad2(hour24); break;
            case 'G': out += std::to_string(hour24); break;
            case 'h': out += pad2(hour12); break;
            case 'g': out += std::to_string(hour12); break;
            case 'i': out += pad2(minute); break;
            case 's': out += pad2(second); break;
            case 'd': out += pad2(day); break;
            case 'j': out += std::to_string(day); break;
            case 'm': out += pad2(month); break;
            case 'n': out += std::to_string(month); break;
            case 'Y': out += std::to_string(year); break;
            case 'y': out += pad2(year % 100); break;
            default: out += c; break;
        }
    }
    return out;
}
