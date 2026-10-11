#pragma once
#include <windows.h>
#include <string>
struct ID3D11Device;
struct ID3D11DeviceContext;
namespace menus {
bool Init(HWND window,ID3D11Device* device,ID3D11DeviceContext* context);
void Shutdown();
void Draw();
bool HandleMessage(HWND window,UINT message,WPARAM wparam,LPARAM lparam);
// Native review fixture: exercise the real widgets through queued ImGui input.
void EnableReview();
bool ReviewPointer(const std::string& label,bool down,bool right=false,bool shift=false);
}
