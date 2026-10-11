#pragma once
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace benchmark {
// Bounds keep accidental CLI values from creating unbounded captures/runs.
inline bool ReadCount(const char* command, const char* option, int minimum, int maximum, int& value) {
    if (!command) return true;
    const char* start = std::strstr(command, option);
    if (!start) return true;
    start += std::strlen(option);
    if (!std::isdigit(static_cast<unsigned char>(*start))) return false;
    char* end = nullptr;
    long parsed = std::strtol(start, &end, 10);
    if ((*end && !std::isspace(static_cast<unsigned char>(*end))) || parsed < minimum || parsed > maximum) return false;
    value = int(parsed);return true;
}
}
