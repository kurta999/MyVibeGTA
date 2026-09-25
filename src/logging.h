#pragma once

namespace logging {
void initialize();
void write(const char* message);
void shutdown();
}
