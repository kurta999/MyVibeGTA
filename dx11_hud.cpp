#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "dx11_assets.h"
#include "game.h"
#include "ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace dx11 {
namespace {
HDC memoryDC=nullptr;
HBITMAP bitmap=nullptr;
HGDIOBJ oldBitmap=nullptr;
HFONT font=nullptr;
HGDIOBJ oldFont=nullptr;
unsigned char* dib=nullptr;
int canvasW=0,canvasH=0;
void resize(int width,int height){
    if(width==canvasW&&height==canvasH)return;
    shutdownHud();canvasW=width;canvasH=height;
    memoryDC=CreateCompatibleDC(nullptr);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    bitmap=CreateDIBSection(memoryDC,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&dib),nullptr,0);
    oldBitmap=SelectObject(memoryDC,bitmap);
    font=CreateFontA(-18,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,"Arial");
    oldFont=SelectObject(memoryDC,font);SetBkMode(memoryDC,TRANSPARENT);
}
void rect(int x,int y,int w,int h,COLORREF color){
    RECT area{x,y,x+w,y+h};HBRUSH brush=CreateSolidBrush(color);
    FillRect(memoryDC,&area,brush);DeleteObject(brush);
}
void label(int x,int y,const char* value,COLORREF color){
    SetTextColor(memoryDC,color);TextOutA(memoryDC,x,y,value,int(std::strlen(value)));
}
void line(int x0,int y0,int x1,int y1,COLORREF color,int width=2){
    HPEN pen=CreatePen(PS_SOLID,width,color);HGDIOBJ old=SelectObject(memoryDC,pen);
    MoveToEx(memoryDC,x0,y0,nullptr);LineTo(memoryDC,x1,y1);
    SelectObject(memoryDC,old);DeleteObject(pen);
}
void map(int x,int y,int width,int height,bool large){
    rect(x-3,y-3,width+6,height+6,RGB(26,31,39));
    rect(x,y,width,height,RGB(116,147,107));
    auto sx=[&](float px){return x+int(px/game::WORLD_W*width);};
    auto sy=[&](float pz){return y+int(pz/game::WORLD_D*height);};
    rect(x,sy(game::BEACH_START),width,sy(game::SHORE)-sy(game::BEACH_START),RGB(225,200,151));
    rect(x,sy(game::SHORE),width,y+height-sy(game::SHORE),RGB(49,124,175));
    for(int col=0;col<5;++col)rect(sx(300+col*450-game::ROAD_W/2),y,
        std::max(2,int(game::ROAD_W/game::WORLD_W*width)),sy(game::BEACH_START)-y,RGB(77,80,84));
    for(int row=0;row<4;++row)rect(x,sy(250+row*390-game::ROAD_W/2),width,
        std::max(2,int(game::ROAD_W/game::WORLD_D*height)),RGB(77,80,84));
    if(large)for(const auto& b:game::buildings)
        rect(sx(b.x),sy(b.z),std::max(2,int(b.w/game::WORLD_W*width)),
            std::max(2,int(b.d/game::WORLD_D*height)),RGB(160,137,113));
    int next=game::nextMission();
    if(game::activeMission>=0&&game::missionStep<int(game::missions[game::activeMission].goals.size())){
        game::Vec2 goal=game::missions[game::activeMission].goals[game::missionStep];
        line(sx(game::player.x),sy(game::player.z),sx(goal.x),sy(goal.z),RGB(170,255,137));
        rect(sx(goal.x)-4,sy(goal.z)-4,8,8,RGB(120,255,115));
    }else if(next>=0){
        game::Vec2 start=game::missions[next].start;
        line(sx(game::player.x),sy(game::player.z),sx(start.x),sy(start.z),RGB(255,210,112));
        rect(sx(start.x)-4,sy(start.z)-4,8,8,RGB(255,210,112));
    }
    for(const auto& pickup:game::pickups)if(pickup.available)
        rect(sx(pickup.p.x)-2,sy(pickup.p.z)-2,5,5,RGB(92,238,231));
    for(size_t i=0;i<game::missions.size();++i){auto p=game::missions[i].start;
        COLORREF marker=game::missionDone[i]?RGB(136,211,140):
            int(i)==next?RGB(245,184,100):RGB(110,119,125);
        rect(sx(p.x)-3,sy(p.z)-3,7,7,marker);}
    int px=sx(game::player.x),py=sy(game::player.z);
    float forwardX=std::cos(game::cameraYaw),forwardY=std::sin(game::cameraYaw);
    int arrow=large?17:14;
    POINT points[]={{px+int(forwardX*arrow),py+int(forwardY*arrow)},
        {px-int(forwardX*arrow*0.55f)-int(forwardY*arrow*0.58f),
         py-int(forwardY*arrow*0.55f)+int(forwardX*arrow*0.58f)},
        {px-int(forwardX*arrow*0.55f)+int(forwardY*arrow*0.58f),
         py-int(forwardY*arrow*0.55f)-int(forwardX*arrow*0.58f)}};
    HBRUSH arrowBrush=CreateSolidBrush(RGB(255,230,79));
    HPEN arrowPen=CreatePen(PS_SOLID,2,RGB(25,28,32));
    HGDIOBJ oldBrush=SelectObject(memoryDC,arrowBrush),oldPen=SelectObject(memoryDC,arrowPen);
    Polygon(memoryDC,points,3);
    SelectObject(memoryDC,oldPen);SelectObject(memoryDC,oldBrush);
    DeleteObject(arrowPen);DeleteObject(arrowBrush);
    if(large){label(sx(360),sy(90),"DOWNTOWN",RGB(249,242,224));
        label(sx(1370),sy(1690),"BEACH",RGB(97,76,50));
        label(sx(1250),sy(1950),"HARBOR",RGB(241,244,246));}
}
void pauseMenu(int width,int height){
    if(!ui::paused())return;
    rect(0,0,width,height,RGB(20,29,38));
    int x=width/2-280,y=height/2-250;
    rect(x,y,560,500,RGB(34,45,56));
    label(x+29,y+27,"MINI CITY 3D  /  DIRECT3D 11",RGB(255,225,151));
    const char* heading=ui::page==ui::Page::Main?"PAUSED":ui::page==ui::Page::Graphics?"GRAPHICS":
        ui::page==ui::Page::Controls?"CONTROLS":"AUDIO";
    label(x+29,y+66,heading,RGB(231,241,242));
    char buffer[160];
    if(ui::page==ui::Page::Main){
        const char* items[]={"Resume","Graphics","Controls","Audio","Save game","Load game","Exit game"};
        for(int i=0;i<7;++i){int row=y+115+i*45;
            if(i==ui::selection)rect(x+22,row-4,510,36,RGB(73,113,134));
            label(x+42,row+4,items[i],i==ui::selection?RGB(255,238,168):RGB(222,230,234));}
    }else if(ui::page==ui::Page::Graphics){
        const char* quality[]={"Low","Medium","High"};const char* sizes[]={"1280 x 720","1600 x 900","1920 x 1080"};
        const char* shadows[]={"Off","Medium","High"};
        const char* items[]={quality[ui::graphicsQuality],sizes[ui::windowChoice],
            quality[ui::vegetationDensity],quality[ui::effectsQuality],shadows[ui::shadowQuality]};
        const char* names[]={"Scene quality","Window size","Vegetation","Effects","Shadows"};
        for(int i=0;i<5;++i){int row=y+115+i*65;
            if(i==ui::selection)rect(x+22,row-4,510,38,RGB(73,113,134));
            std::snprintf(buffer,sizeof(buffer),"%s:  < %s >",names[i],items[i]);
            label(x+42,row+5,buffer,RGB(239,241,229));}
    }else if(ui::page==ui::Page::Controls){
        const char* names[]={"Mouse sensitivity","Invert vertical mouse","Forward","Backward","Left","Right","Sprint","Interact"};
        for(int i=0;i<8;++i){int row=y+105+i*39;
            if(i==ui::selection)rect(x+22,row-3,510,32,RGB(73,113,134));
            if(i==0)std::snprintf(buffer,sizeof(buffer),"%s:  < %d >",names[i],ui::mouseSensitivity);
            else if(i==1)std::snprintf(buffer,sizeof(buffer),"%s:  < %s >",names[i],ui::invertY?"On":"Off");
            else std::snprintf(buffer,sizeof(buffer),"%s:  %s",names[i],ui::keyName(ui::bindings[i-2]));
            label(x+42,row+4,buffer,RGB(236,239,231));}
        if(ui::waitingForBinding>=0)label(x+42,y+434,"Press a new key (Esc cancels)",RGB(255,220,132));
    }else{
        std::snprintf(buffer,sizeof(buffer),"Master volume:  < %d%% >",ui::masterVolume);
        rect(x+22,y+124,510,40,RGB(73,113,134));
        label(x+42,y+134,buffer,RGB(239,241,231));
    }
    label(x+28,y+467,"Arrow keys: select/change    Enter: choose    Esc: back",RGB(197,207,212));
}
}
void buildHud(unsigned char* pixels,int width,int height){
    resize(width,height);
    if(!memoryDC||!dib){std::memset(pixels,0,size_t(width)*height*4);return;}
    std::memset(dib,0,size_t(width)*height*4);
    char textBuffer[240];
    rect(12,12,760,72,RGB(31,41,49));
    int hour=int(game::gameHour),minute=int((game::gameHour-hour)*60);
    label(24,19,"MINI CITY 3D",RGB(255,225,151));
    if(game::occupied>=0){
        const auto& vehicle=game::vehicles[game::occupied];
        const char* name=vehicle.kind==game::Kind::Boat?"BOAT":vehicle.kind==game::Kind::Bike?"BIKE":
            vehicle.kind==game::Kind::SportCar?"SPORT CAR":"CAR";
        std::snprintf(textBuffer,sizeof(textBuffer),"%s  |  CONDITION %d%%  |  WASD drive  |  E exit",
            name,int(100-vehicle.damage));
    }else std::snprintf(textBuffer,sizeof(textBuffer),"WASD move  |  SHIFT run  |  RMB aim  |  LMB fire  |  R reload");
    label(220,19,textBuffer,RGB(223,230,230));
    label(24,49,"1-5 / Q weapons  |  E vehicle  |  F mission  |  M map  |  ESC menu",RGB(201,215,215));
    map(18,height-169,180,140,false);
    int statusX=width-360;
    rect(statusX,12,348,137,RGB(31,41,49));
    std::snprintf(textBuffer,sizeof(textBuffer),"%02d:%02d       $%06d",hour,minute,game::money);
    label(statusX+15,19,textBuffer,RGB(255,231,166));
    rect(statusX+14,45,320,1,RGB(96,110,117));
    label(statusX+15,53,game::weaponNames[game::weapon],RGB(244,245,234));
    if(game::reloadRemaining>0)std::snprintf(textBuffer,sizeof(textBuffer),"RELOADING");
    else if(game::ammo[game::weapon]<0)std::snprintf(textBuffer,sizeof(textBuffer),"%d / --",game::magazine[game::weapon]);
    else std::snprintf(textBuffer,sizeof(textBuffer),"%d / %d",game::magazine[game::weapon],game::ammo[game::weapon]);
    label(statusX+15,77,textBuffer,RGB(255,221,137));
    int gunX=statusX+225,gunY=64;
    int barrel=game::weapon==0?43:game::weapon==1?56:game::weapon==2?69:81;
    rect(gunX,gunY,barrel,7,RGB(208,215,213));
    rect(gunX+4,gunY+7,18,8,RGB(135,145,149));
    rect(gunX+8,gunY+14,10,13,RGB(96,106,112));
    if(game::weapon>=2)rect(gunX+35,gunY+7,18,4,RGB(116,127,130));
    std::snprintf(textBuffer,sizeof(textBuffer),"HEALTH %d",int(game::health));
    label(statusX+15,109,textBuffer,RGB(243,233,215));
    rect(statusX+130,109,204,20,RGB(104,55,59));
    rect(statusX+130,109,int(204*std::clamp(game::health,0.0f,100.0f)/100.0f),20,
        game::health>35?RGB(72,208,104):RGB(237,139,82));
    if(game::activeMission>=0){
        const auto& mission=game::missions[game::activeMission];
        rect(width-360,157,348,96,RGB(31,41,49));
        std::snprintf(textBuffer,sizeof(textBuffer),"%s   %d sec",mission.name,int(game::missionTime));
        label(width-346,165,textBuffer,RGB(255,221,137));
        std::snprintf(textBuffer,sizeof(textBuffer),"Objective %d / %d",
            game::missionStep+1,int(mission.goals.size()));
        label(width-346,194,textBuffer,RGB(225,235,224));
        label(width-346,222,game::missionObjective(),RGB(225,235,224));
    }else if(game::nextMission()>=0){
        int next=game::nextMission();
        rect(width-360,157,348,58,RGB(31,41,49));
        std::snprintf(textBuffer,sizeof(textBuffer),"NEXT: %s",game::missions[next].name);
        label(width-346,165,textBuffer,RGB(255,221,137));
        label(width-346,190,"Follow gold map line, then press F",RGB(225,235,224));
    }
    if(game::occupied<0&&game::rightMouse&&!ui::paused()){
        int x=width/2,y=height/2,kick=int(game::recoil*9);
        rect(x-10-kick,y-1,7,2,RGB(251,244,215));rect(x+4+kick,y-1,7,2,RGB(251,244,215));
        rect(x-1,y-10-kick,2,7,RGB(251,244,215));rect(x-1,y+4+kick,2,7,RGB(251,244,215));
    }
    if(game::showMap){
        int mapW=std::min(width-120,650),mapH=std::min(height-130,int(mapW*game::WORLD_D/game::WORLD_W));
        int x=(width-mapW)/2,y=(height-mapH)/2;
        rect(x-25,y-49,mapW+50,mapH+100,RGB(30,41,51));
        label(x,y-31,"CITY MAP  -  yellow arrow: you  green: objective  cyan: weapons",RGB(247,238,211));
        map(x,y,mapW,mapH,true);
    }
    if(game::messageTime>0){
        rect(width/2-310,height-65,620,44,RGB(31,41,49));
        label(width/2-294,height-55,game::message.c_str(),RGB(255,225,154));
    }
    if(game::health<=0){rect(width/2-145,height/2-48,290,96,RGB(35,42,48));
        label(width/2-65,height/2-24,"YOU DIED",RGB(252,124,115));
        label(width/2-94,height/2+8,"Press R to restart",RGB(247,242,233));}
    if(game::debugHud){rect(width-276,height-137,261,123,RGB(30,42,48));
        std::snprintf(textBuffer,sizeof(textBuffer),"FPS %.0f   FRAME %.1f ms",game::frameRate,game::frameMs);
        label(width-265,height-128,textBuffer,RGB(224,245,220));
        std::snprintf(textBuffer,sizeof(textBuffer),"SIM %.2f   PHYS %.2f ms",game::simulationMs,game::physicsMs);
        label(width-265,height-100,textBuffer,RGB(224,245,220));
        std::snprintf(textBuffer,sizeof(textBuffer),"DRAWS %d   ACTIVE AI %d",game::drawCalls,game::activeAi);
        label(width-265,height-72,textBuffer,RGB(224,245,220));
        std::snprintf(textBuffer,sizeof(textBuffer),"PROPS %d",int(game::props.size()));
        label(width-265,height-44,textBuffer,RGB(224,245,220));}
    pauseMenu(width,height);
    for(size_t i=0;i<size_t(width)*height;++i)
        if(dib[i*4]||dib[i*4+1]||dib[i*4+2])dib[i*4+3]=255;
    std::memcpy(pixels,dib,size_t(width)*height*4);
}
void shutdownHud(){
    if(memoryDC){if(oldFont)SelectObject(memoryDC,oldFont);if(font)DeleteObject(font);
        if(oldBitmap)SelectObject(memoryDC,oldBitmap);if(bitmap)DeleteObject(bitmap);
        DeleteDC(memoryDC);}
    memoryDC=nullptr;bitmap=nullptr;oldBitmap=nullptr;font=nullptr;oldFont=nullptr;dib=nullptr;
    canvasW=canvasH=0;
}
}
