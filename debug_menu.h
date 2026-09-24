#pragma once

namespace debug_menu {
extern bool open;
extern bool godMode;
extern bool flyMode;
extern int selection;

void reset();
void toggle();
void handleKey(int key);
int entryCount();
}
