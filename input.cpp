#include <windowsx.h>
#include <algorithm>
#include <vector>
#include "input.h"
#include "game.h"
#include "ui.h"

namespace input {
using namespace game;

void releaseAim(){
    if(rightMouse){rightMouse=false;ShowCursor(TRUE);ClipCursor(nullptr);}
    if(GetCapture()==win)ReleaseCapture();
}

LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_SIZE:screenW=std::max(1,int(LOWORD(lp)));screenH=std::max(1,int(HIWORD(lp)));return 0;
    case WM_KEYDOWN:
        if(wp==VK_ESCAPE&&!(lp&(1<<30))){releaseAim();leftMouse=false;
            std::fill(std::begin(keys),std::end(keys),false);ui::handleKey(VK_ESCAPE);return 0;}
        if(ui::paused()){if(!(lp&(1<<30)))ui::handleKey(int(wp));return 0;}
        if(wp<256)keys[wp]=true;
        if(!(lp&(1<<30))){
            if(int(wp)==ui::bindings[int(ui::Action::Interact)]&&health>0)enterExit();
            if(wp=='F')startMission();
            if(wp=='M')showMap=!showMap;
            if(wp==VK_F3)debugHud=!debugHud;
            if(wp=='T')gameHour=std::fmod(gameHour+1.0f,24.0f);
            if(wp=='Q')for(int i=1;i<=5;++i){int candidate=(weapon+i)%5;
                if(unlocked[candidate]){weapon=candidate;break;}}
        }
        if(wp=='R'){if(health<=0)reset();else if(!(lp&(1<<30)))startReload();}return 0;
    case WM_KEYUP:if(wp<256)keys[wp]=false;return 0;
    case WM_LBUTTONDOWN:if(!ui::paused()){leftMouse=true;SetCapture(hwnd);}return 0;
    case WM_LBUTTONUP:leftMouse=false;if(!rightMouse)ReleaseCapture();return 0;
    case WM_RBUTTONDOWN:if(!ui::paused()&&occupied<0&&!rightMouse){
        rightMouse=true;ShowCursor(FALSE);SetCapture(hwnd);
        RECT r{};GetClientRect(hwnd,&r);POINT a{r.left,r.top},b{r.right,r.bottom};
        ClientToScreen(hwnd,&a);ClientToScreen(hwnd,&b);RECT clip{a.x,a.y,b.x,b.y};ClipCursor(&clip);
    }return 0;
    case WM_RBUTTONUP:releaseAim();return 0;
    case WM_INPUT:if(rightMouse&&!ui::paused()&&occupied<0){
        UINT size=0;GetRawInputData(HRAWINPUT(lp),RID_INPUT,nullptr,&size,sizeof(RAWINPUTHEADER));
        std::vector<BYTE> buffer(size);
        if(size&&GetRawInputData(HRAWINPUT(lp),RID_INPUT,buffer.data(),&size,sizeof(RAWINPUTHEADER))==size){
            const RAWINPUT* data=reinterpret_cast<const RAWINPUT*>(buffer.data());
            if(data->header.dwType==RIM_TYPEMOUSE){
                float sensitivity=ui::mouseSensitivity*0.001f;
                cameraYaw+=data->data.mouse.lLastX*sensitivity;
                cameraPitch+=data->data.mouse.lLastY*sensitivity*(ui::invertY?1.0f:-1.0f);
                cameraPitch=std::clamp(cameraPitch,-0.85f,0.8f);
            }
        }
    }return 0;
    case WM_KILLFOCUS:std::fill(std::begin(keys),std::end(keys),false);
        leftMouse=false;releaseAim();if(ui::page==ui::Page::Closed)ui::page=ui::Page::Main;return 0;
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcA(hwnd,msg,wp,lp);
}
}
