#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace input {
void releaseAim();
void syncLookCapture();
void applyMouseDelta(LONG x,LONG y);
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam);
}
