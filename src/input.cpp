#include <windowsx.h>
#include <algorithm>
#include <vector>
#include "input.h"
#include "game.h"
#include "ui.h"
#include "weapons.h"
#include "camera.h"
#include "ai.h"
#include "police.h"
#include "commerce.h"
#ifdef MINI_CITY_JOLT
#include "debug_menu.h"
#endif

namespace input {
using namespace game;
int wheelRemainder=0;
bool lookCaptured=false;

void releaseAim(){
    rightMouse=false;
}
void clipLookCursor(){
    RECT client{};GetClientRect(win,&client);
    POINT topLeft{client.left,client.top},bottomRight{client.right,client.bottom};
    ClientToScreen(win,&topLeft);ClientToScreen(win,&bottomRight);
    RECT clip{topLeft.x,topLeft.y,bottomRight.x,bottomRight.y};
    ClipCursor(&clip);
}
void releaseLookCapture(){
    if(!lookCaptured)return;
    ClipCursor(nullptr);
    if(GetCapture()==win)ReleaseCapture();
    ShowCursor(TRUE);
    lookCaptured=false;
}
void syncLookCapture(){
    bool gameplay=win&&IsWindowVisible(win)&&GetForegroundWindow()==win&&
        !ui::paused()&&commerce::menu()==commerce::Menu::None;
#ifdef MINI_CITY_JOLT
    gameplay=gameplay&&!debug_menu::open;
#endif
    if(!gameplay){releaseLookCapture();return;}
    if(!lookCaptured){
        ShowCursor(FALSE);
        SetCapture(win);
        lookCaptured=true;
        clipLookCursor();
    }
}
void applyMouseDelta(LONG x,LONG y){
    float sensitivity=ui::mouseSensitivity*0.001f;
    cameraYaw+=x*sensitivity;
    cameraPitch+=y*sensitivity*(ui::invertY?1.0f:-1.0f);
    cameraPitch=std::clamp(cameraPitch,-0.85f,0.8f);
    if(occupied>=0&&(x||y))vehicleLookTime=2.0f;
}

LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_SIZE:screenW=std::max(1,int(LOWORD(lp)));screenH=std::max(1,int(HIWORD(lp)));
        if(lookCaptured)clipLookCursor();return 0;
    case WM_MOVE:if(lookCaptured)clipLookCursor();return 0;
    case WM_KEYDOWN:
#ifdef MINI_CITY_JOLT
        if(wp==VK_F11){
            if(!(lp&(1<<30)))requestScreenshot();
            return 0;
        }
        if(debug_menu::open){
            if(!(lp&(1<<30)))debug_menu::handleKey(int(wp));
            return 0;
        }
#endif
        if(commerce::menu()!=commerce::Menu::None){
            if(!(lp&(1<<30)))commerce::handleKey(int(wp));
            return 0;
        }
        if(wp==VK_ESCAPE&&!(lp&(1<<30))){releaseAim();leftMouse=false;
            std::fill(std::begin(keys),std::end(keys),false);ui::handleKey(VK_ESCAPE);return 0;}
        if(ui::paused()){if(!(lp&(1<<30)))ui::handleKey(int(wp));return 0;}
#ifdef MINI_CITY_JOLT
        if(wp==VK_F4){
            if(!(lp&(1<<30))){
                releaseAim();leftMouse=false;
                std::fill(std::begin(keys),std::end(keys),false);
                debug_menu::toggle();
                releaseLookCapture();
            }
            return 0;
        }
#endif
        if(wp<256)keys[wp]=true;
        if(!(lp&(1<<30))){
            if(int(wp)==ui::bindings[int(ui::Action::Interact)]&&health>0)enterExit();
            if(wp=='F'){interact();if(commerce::menu()!=commerce::Menu::None)releaseAim();}
            if(wp==VK_TAB)cycleInteraction();
            if(wp=='G')carryDrop();
            if(wp=='B'&&health>0&&occupied<0){
                telescopeActive=!telescopeActive;
                if(telescopeActive){scopeLevel=0;releaseAim();}
                message=telescopeActive?"TELESCOPE  |  SCROLL TO ZOOM  |  B TO CLOSE":
                    "TELESCOPE CLOSED";
                messageTime=2;
            }
            if(wp=='M')showMap=!showMap;
            if(wp=='C'){
                cameraMode=CameraMode((int(cameraMode)+1)%5);
                const char* views[]={"FIRST PERSON CLOSE","FIRST PERSON WIDE",
                    "THIRD PERSON NEAR","THIRD PERSON FAR","OVERVIEW"};
                message=views[int(cameraMode)];messageTime=2;
            }
            if(wp==VK_F3)debugHud=!debugHud;
            if(wp==VK_F1)ui::showHelp=!ui::showHelp;
            if(wp=='T')gameHour=std::fmod(gameHour+1.0f,24.0f);
            if(wp=='Q')for(int i=1;i<=weapons::count();++i){int candidate=(weapon+i)%weapons::count();
                if(unlocked[candidate]){weapon=candidate;break;}}
        }
        if(wp=='R'){if(health<=0)reset();else if(!(lp&(1<<30)))startReload();}return 0;
    case WM_KEYUP:if(wp<256)keys[wp]=false;return 0;
    case WM_LBUTTONDOWN:if(ui::paused()){
            ui::handleMouse(GET_X_LPARAM(lp),GET_Y_LPARAM(lp),false);return 0;
        }
        if(commerce::menu()==commerce::Menu::None
#ifdef MINI_CITY_JOLT
        &&!debug_menu::open
#endif
        )
        leftMouse=true;return 0;
    case WM_LBUTTONUP:leftMouse=false;return 0;
    case WM_MOUSEMOVE:if(ui::paused()&&(wp&MK_LBUTTON))
            ui::handleMouse(GET_X_LPARAM(lp),GET_Y_LPARAM(lp),true);
        return 0;
    case WM_RBUTTONDOWN:if(!ui::paused()&&commerce::menu()==commerce::Menu::None&&
#ifdef MINI_CITY_JOLT
        !debug_menu::open&&
#endif
        occupied<0&&!rightMouse){
        rightMouse=true;
        if(!telescopeActive&&!weapons::stats(weapon).melee&&
            ai::notifyThreat(player,cameraYaw)>0)
            police::report(police::Crime::Threat,player,true);
    }return 0;
    case WM_RBUTTONUP:releaseAim();return 0;
    case WM_MOUSEWHEEL:
        if(!ui::paused()&&commerce::menu()==commerce::Menu::None&&camera::zoomActive()
#ifdef MINI_CITY_JOLT
            &&!debug_menu::open
#endif
            ){
            wheelRemainder+=GET_WHEEL_DELTA_WPARAM(wp);
            while(wheelRemainder>=WHEEL_DELTA){scopeLevel=std::min(2,scopeLevel+1);wheelRemainder-=WHEEL_DELTA;}
            while(wheelRemainder<=-WHEEL_DELTA){scopeLevel=std::max(0,scopeLevel-1);wheelRemainder+=WHEEL_DELTA;}
        }
        return 0;
    case WM_INPUT:if(lookCaptured&&!ui::paused()&&
#ifdef MINI_CITY_JOLT
        !debug_menu::open&&
#endif
        commerce::menu()==commerce::Menu::None){
        UINT size=0;GetRawInputData(HRAWINPUT(lp),RID_INPUT,nullptr,&size,sizeof(RAWINPUTHEADER));
        std::vector<BYTE> buffer(size);
        if(size&&GetRawInputData(HRAWINPUT(lp),RID_INPUT,buffer.data(),&size,sizeof(RAWINPUTHEADER))==size){
            const RAWINPUT* data=reinterpret_cast<const RAWINPUT*>(buffer.data());
            if(data->header.dwType==RIM_TYPEMOUSE){
                applyMouseDelta(data->data.mouse.lLastX,data->data.mouse.lLastY);
            }
        }
    }return 0;
    case WM_KILLFOCUS:std::fill(std::begin(keys),std::end(keys),false);
#ifdef MINI_CITY_JOLT
        debug_menu::open=false;
#endif
        leftMouse=false;releaseAim();releaseLookCapture();
        if(ui::page==ui::Page::Closed)ui::page=ui::Page::Main;return 0;
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:releaseLookCapture();PostQuitMessage(0);return 0;
    }
    return DefWindowProcA(hwnd,msg,wp,lp);
}
}
