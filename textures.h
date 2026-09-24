#pragma once
#include <GL/gl.h>

namespace textures {
bool load();
void bind(int tile);
void unbind();
void shutdown();
bool ready();
}
