#pragma once
#include <array>

namespace ui {
enum class Page { Closed, Main, Graphics, Controls, Audio };
enum class Action { Forward, Backward, Left, Right, Sprint, Interact, Count };
extern Page page;
extern int selection;
extern int graphicsQuality;
extern int shadowQuality;
extern int vegetationDensity;
extern int effectsQuality;
extern int windowChoice;
extern int mouseSensitivity;
extern int masterVolume;
extern bool invertY;
extern int waitingForBinding;
extern std::array<int,int(Action::Count)> bindings;
bool paused();
void load();
void save();
void handleKey(int key);
const char* keyName(int key);
}
