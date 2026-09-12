// firmware/DeskMatrix/EzTimeFormat.h
#pragma once
#include <string>

// Translates a subset of ezTime's date/time format tokens (used by
// Clockwise's Canvas clockface JSON themes, e.g. "H:i") into a formatted
// string, given already-computed calendar fields. Needed because
// CWDateTime.cpp (see its port-history comment) replaced ezTime with
// strftime-based formatting, which doesn't understand these tokens.
//
// Supported tokens:
//   H/G  24-hour, padded/unpadded
//   h/g  12-hour, padded/unpadded (0 -> 12)
//   i    minute, padded
//   s    second, padded
//   d/j  day of month, padded/unpadded
//   m/n  month, padded/unpadded
//   Y/y  4-digit/2-digit year
// Any other character (e.g. ':', '-') is copied through unchanged.
std::string formatEzTime(const std::string& format, int hour24, int minute,
                          int second, int day, int month, int year);
