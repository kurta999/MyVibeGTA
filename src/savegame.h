#pragma once
#ifdef MINI_CITY_JOLT
#include "save_status.h"
#endif
#include <string>
namespace savegame {
bool save();
void request();
#ifdef MINI_CITY_JOLT
WriteReceipt requestCheckpoint();
#endif
bool flush();
bool takeFailure();
bool load();
// Full path of the save file (see user_paths.h for MYVIBEGTA_USER_DIR).
std::string filePath();
}
