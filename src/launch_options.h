#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

// Process command-line flags. The command line never changes after start,
// so it is captured once (thread-safe static) instead of re-reading
// GetCommandLineA() in every frame and on scene worker threads.
namespace launch {
inline const std::string& commandLine() {
    static const std::string line = GetCommandLineA() ? GetCommandLineA() : "";
    return line;
}
inline bool has(const char* flag) {
    return commandLine().find(flag) != std::string::npos;
}
inline bool smoke() {
    static const bool value = has("--smoke");
    return value;
}
inline bool benchmark() {
    static const bool value = has("--benchmark");
    return value;
}
} // namespace launch
