#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace input {
void releaseAim();
void syncLookCapture();
void applyMouseDelta(LONG x,LONG y);
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam);
}
