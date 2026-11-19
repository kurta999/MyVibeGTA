#pragma once
#include <array>
#include <string>

namespace ui {
enum class Page { Closed, Main, Graphics, Controls, Audio };
enum class Action { Forward, Backward, Left, Right, Sprint, Interact, Count };
extern Page page;
extern int selection;
extern int graphicsQuality;
extern int shadowQuality;
extern int reflectionQuality;
extern int antiAliasingQuality;
extern int fsr2Quality;
extern int fsr2Sharpness;
extern int aoQuality;
extern int textureQuality;
extern int filteringQuality;
extern int vegetationDensity;
extern int grassDistance;
extern int grassLodDistance;
constexpr int GRAPHICS_ROW_HEIGHT=30;
#ifdef MINI_CITY_DX12
constexpr int GRAPHICS_MENU_ROWS=18;
#else
constexpr int GRAPHICS_MENU_ROWS=16;
#endif
// VSync is always the last graphics row so the FSR2 rows keep their indices.
constexpr int GRAPHICS_VSYNC_ROW=GRAPHICS_MENU_ROWS-1;
constexpr int CONTROLS_MENU_ROWS=9;
constexpr int CONTROLS_ROW_HEIGHT=36;
constexpr int CONTROLS_MENU_HEIGHT=150+CONTROLS_MENU_ROWS*CONTROLS_ROW_HEIGHT+40;
constexpr int FIELD_OF_VIEW_MIN=55,FIELD_OF_VIEW_MAX=95,FIELD_OF_VIEW_DEFAULT=65;
constexpr int GRAPHICS_MENU_HEIGHT=160+GRAPHICS_MENU_ROWS*GRAPHICS_ROW_HEIGHT;
extern int effectsQuality;
extern int drawDistance;
extern int lodDistance;
extern int windowChoice;
extern int windowMode;
void applyWindow();
void toggleFullscreen();
void setWindowActive(bool active);
extern int mouseSensitivity;
extern int masterVolume;
extern bool invertY;
extern bool vsync;
// Third-person horizontal field of view in degrees; other views scale with it.
extern int fieldOfView;
extern bool showHelp;
// Feedback for the last rejected key binding, shown on the Controls page.
extern std::string bindingNotice;
extern int waitingForBinding;
extern std::array<int,int(Action::Count)> bindings;
float drawDistanceScale();
float lodDistanceScale();
float grassDistanceUnits();
float grassLodDistanceUnits();
void handleMouse(int x,int y,bool dragging);
bool paused();
void load();
void save();
// Marks settings changed; flushSettings writes them once changes settle.
void saveSoon();
void flushSettings(bool force=false);
void handleKey(int key);
const char* keyName(int key);
bool bindKey(int action,int key);
}
