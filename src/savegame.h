#pragma once
#ifdef MINI_CITY_JOLT
#include "save_status.h"
#endif
namespace savegame {
bool save();
void request();
#ifdef MINI_CITY_JOLT
WriteReceipt requestCheckpoint();
#endif
bool flush();
bool takeFailure();
bool load();
}
