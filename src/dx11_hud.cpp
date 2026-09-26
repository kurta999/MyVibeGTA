#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "dx11_assets.h"
#include "game.h"
#include "ui.h"
#include "weapons.h"
#include "camera.h"
#include "police.h"
#include "commerce.h"
#include "weather.h"
#include "regions.h"
#include "debug_menu.h"
#include "fire.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif
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
HFONT bannerFont=nullptr;
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
    bannerFont=CreateFontA(-32,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,ANSI_CHARSET,
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
void wantedStar(int x,int y,bool filled){
    POINT points[10]{};
    for(int i=0;i<10;++i){
        float angle=-game::PI/2+i*game::PI/5;
        float radius=i%2?3.5f:8.0f;
        points[i]={x+int(std::cos(angle)*radius),y+int(std::sin(angle)*radius)};
    }
    COLORREF tint=filled?RGB(255,206,87):RGB(105,116,121);
    HBRUSH brush=CreateSolidBrush(filled?tint:RGB(31,41,49));
    HPEN pen=CreatePen(PS_SOLID,1,tint);
    HGDIOBJ oldBrush=SelectObject(memoryDC,brush),oldPen=SelectObject(memoryDC,pen);
    Polygon(memoryDC,points,10);
    SelectObject(memoryDC,oldPen);SelectObject(memoryDC,oldBrush);
    DeleteObject(pen);DeleteObject(brush);
}
void scopeOverlay(int width,int height){
    int x=width/2,y=height/2,radius=std::min(width,height)*43/100;
    HRGN full=CreateRectRgn(0,0,width,height);
    HRGN hole=CreateEllipticRgn(x-radius,y-radius,x+radius,y+radius);
    HRGN outside=CreateRectRgn(0,0,0,0);
    CombineRgn(outside,full,hole,RGN_DIFF);
    HBRUSH shade=CreateSolidBrush(RGB(2,4,6));
    FillRgn(memoryDC,outside,shade);
    DeleteObject(shade);DeleteObject(full);DeleteObject(hole);DeleteObject(outside);
    HPEN border=CreatePen(PS_SOLID,3,RGB(31,42,46));
    HGDIOBJ oldPen=SelectObject(memoryDC,border);
    HGDIOBJ oldBrush=SelectObject(memoryDC,GetStockObject(NULL_BRUSH));
    Ellipse(memoryDC,x-radius,y-radius,x+radius,y+radius);
    SelectObject(memoryDC,oldBrush);SelectObject(memoryDC,oldPen);DeleteObject(border);
    COLORREF marks=RGB(62,72,75);
    line(x-radius+18,y,x+radius-18,y,marks,1);
    line(x,y-radius+18,x,y+radius-18,marks,1);
    for(int step=1;step<=3;++step){
        int offset=step*radius/5;
        line(x+offset,y-7,x+offset,y+7,marks,1);
        line(x-offset,y-7,x-offset,y+7,marks,1);
        line(x-7,y+offset,x+7,y+offset,marks,1);
        line(x-7,y-offset,x+7,y-offset,marks,1);
    }
    rect(x-2,y-2,4,4,RGB(215,221,215));
    char zoom[32]{};
    std::snprintf(zoom,sizeof(zoom),"%s  %dx",
        game::telescopeActive?"TELESCOPE":"SNIPER",camera::scopeMagnification());
    label(x-63,y-radius+30,zoom,RGB(220,224,218));
    label(x-83,y+radius-52,"SCROLL TO ZOOM",RGB(203,212,207));
}
void map(int x,int y,int width,int height,bool large){
    rect(x-3,y-3,width+6,height+6,RGB(26,31,39));
    rect(x,y,width,height,RGB(116,147,107));
    float viewW=large?regions::WIDTH:900,viewD=large?regions::DEPTH:700;
    float minX=large?0:std::clamp(game::player.x-viewW*0.5f,0.0f,regions::WIDTH-viewW);
    float minZ=large?0:std::clamp(game::player.z-viewD*0.5f,0.0f,regions::DEPTH-viewD);
    auto sx=[&](float px){return x+int((px-minX)/viewW*width);};
    auto sy=[&](float pz){return y+int((pz-minZ)/viewD*height);};
    auto mapRect=[&](float x0,float z0,float x1,float z1,COLORREF tint){
        x0=std::max(x0,minX);z0=std::max(z0,minZ);
        x1=std::min(x1,minX+viewW);z1=std::min(z1,minZ+viewD);
        if(x1<=x0||z1<=z0)return;
        rect(sx(x0),sy(z0),std::max(1,sx(x1)-sx(x0)),
            std::max(1,sy(z1)-sy(z0)),tint);
    };
    auto marker=[&](game::Vec2 p,int radius,COLORREF tint){
        if(p.x<minX||p.x>minX+viewW||p.z<minZ||p.z>minZ+viewD)return;
        rect(sx(p.x)-radius,sy(p.z)-radius,radius*2+1,radius*2+1,tint);
    };
    int saved=SaveDC(memoryDC);
    IntersectClipRect(memoryDC,x,y,x+width,y+height);
    for(const auto& region:regions::all()){
        COLORREF tint=region.biome==regions::Biome::City?RGB(119,147,115):
            region.biome==regions::Biome::Snow?RGB(218,226,227):
            region.biome==regions::Biome::Savanna?RGB(185,173,105):
            region.biome==regions::Biome::Desert?RGB(219,190,133):RGB(137,174,111);
        mapRect(region.x0,region.z0,region.x1,region.z1,tint);
    }
    mapRect(7600,0,8000,regions::DEPTH,RGB(49,124,175));
    mapRect(0,game::BEACH_START,game::WORLD_W,game::SHORE,RGB(225,200,151));
    mapRect(0,game::SHORE,game::WORLD_W,game::WORLD_D,RGB(49,124,175));
    for(int col=0;col<5;++col)
        mapRect(300+col*450-game::ROAD_W/2,0,
            300+col*450+game::ROAD_W/2,game::BEACH_START,RGB(77,80,84));
    for(int row=0;row<4;++row)
        mapRect(0,250+row*390-game::ROAD_W/2,
            game::WORLD_W,250+row*390+game::ROAD_W/2,RGB(77,80,84));
    for(const auto& road:regions::roads()){
        float half=road.width*0.5f;
        mapRect(std::min(road.start.x,road.end.x)-half,
            std::min(road.start.z,road.end.z)-half,
            std::max(road.start.x,road.end.x)+half,
            std::max(road.start.z,road.end.z)+half,RGB(77,80,84));
    }
    mapRect(8040,8810,8160,10015,RGB(203,198,178));
    for(int z=10000;z<10720;z+=24){
        float q=(float(z+12)-10360.0f)/360.0f;
        float edge=8010.0f+770.0f*std::sqrt(std::max(0.0f,1.0f-q*q));
        mapRect(8000,float(z),edge,float(z+24),RGB(49,124,175));
    }
    for(const auto& pier: {std::array<float,3>{8300,8730,10220},
                           std::array<float,3>{8350,8780,10390},
                           std::array<float,3>{8260,8670,10560}})
        mapRect(pier[0],pier[2]-11,pier[1],pier[2]+11,RGB(171,139,100));
    auto city=regions::secondCity();
    for(int col=0;col<=city.columns;++col)
        mapRect(city.x+col*city.spacing-52,city.z-100,
            city.x+col*city.spacing+52,city.z+city.rows*city.spacing+100,RGB(77,80,84));
    for(int row=0;row<=city.rows;++row)
        mapRect(city.x-100,city.z+row*city.spacing-52,
            city.x+city.columns*city.spacing+100,city.z+row*city.spacing+52,RGB(77,80,84));
    mapRect(7600,8400,8000,8600,RGB(119,120,117));
    if(large)for(const auto& b:game::buildings)
        mapRect(b.x,b.z,b.x+b.w,b.z+b.d,RGB(160,137,113));
    int next=game::nextMission();
    if(game::activeMission>=0&&game::missionStep<int(game::missions[game::activeMission].goals.size())){
        game::Vec2 goal=game::missions[game::activeMission].goals[game::missionStep];
        if(large)line(sx(game::player.x),sy(game::player.z),sx(goal.x),sy(goal.z),RGB(170,255,137));
        marker(goal,4,RGB(120,255,115));
    }else if(next>=0){
        game::Vec2 start=game::missions[next].start;
        if(large)line(sx(game::player.x),sy(game::player.z),sx(start.x),sy(start.z),RGB(255,210,112));
        marker(start,4,RGB(255,210,112));
    }
    for(const auto& pickup:game::pickups)if(pickup.available)
        marker(pickup.p,2,RGB(92,238,231));
    for(const auto& shop:commerce::shops)
        marker(shop.p,3,RGB(109,245,171));
    for(const auto& house:commerce::houses)
        marker(house.p,3,house.owned?RGB(91,170,245):RGB(185,134,230));
    for(size_t i=0;i<game::missions.size();++i){auto p=game::missions[i].start;
        COLORREF marker=game::missionDone[i]?RGB(136,211,140):
            int(i)==next?RGB(245,184,100):
            i>=6?RGB(246,126,76):RGB(110,119,125);
        if(p.x>=minX&&p.x<=minX+viewW&&p.z>=minZ&&p.z<=minZ+viewD)
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
        label(sx(8280),sy(9050),"MARINA PART",RGB(249,242,224));
        label(sx(8200),sy(10550),"FOKA BAY",RGB(235,242,240));
        label(sx(11100),sy(7100),"EAST CITY",RGB(249,242,224));
        label(sx(2700),sy(11300),"SNOWFIELDS",RGB(62,77,83));
        label(sx(11200),sy(11800),"SAVANNA",RGB(77,73,45));
        label(sx(11700),sy(2700),"SAHARA",RGB(99,73,42));
        label(sx(1370),sy(1690),"BEACH",RGB(97,76,50));}
    else if(game::player.x>=8000&&game::player.x<10000&&
            game::player.z>=8600&&game::player.z<10900){
        rect(x+5,y+5,112,20,RGB(31,48,55));
        label(x+10,y+7,"MARINA PART",RGB(242,235,211));
    }
    RestoreDC(memoryDC,saved);
}
void pauseMenu(int width,int height){
    if(!ui::paused())return;
    rect(0,0,width,height,RGB(20,29,38));
    int menuHeight=ui::page==ui::Page::Graphics?640:500;
    int x=width/2-280,y=(height-menuHeight)/2;
    rect(x,y,560,menuHeight,RGB(34,45,56));
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
            quality[ui::vegetationDensity],quality[ui::effectsQuality],shadows[ui::shadowQuality],
            shadows[ui::reflectionQuality],shadows[ui::antiAliasingQuality],shadows[ui::aoQuality]};
        const char* names[]={"Scene quality","Window size","Vegetation","Effects","Shadows",
            "Reflections (SSR)","Anti-aliasing (FXAA)","Ambient occlusion (SSAO)",
            "Draw distance","LOD distance","Grass distance"};
        for(int i=0;i<11;++i){int row=y+105+i*42;
            if(i==ui::selection)rect(x+22,row-4,510,38,RGB(73,113,134));
            if(i<8){
                std::snprintf(buffer,sizeof(buffer),"%s:  < %s >",names[i],items[i]);
                label(x+42,row+5,buffer,RGB(239,241,229));
            }else{
                int value=i==8?ui::drawDistance:i==9?ui::lodDistance:ui::grassDistance;
                if(i==8)std::snprintf(buffer,sizeof(buffer),"%s: %.0f m",
                    names[i],1250.0f*ui::drawDistanceScale());
                else if(i==10)std::snprintf(buffer,sizeof(buffer),"%s: %d m",names[i],40+value*2);
                else std::snprintf(buffer,sizeof(buffer),"%s: %d%%",names[i],value);
                label(x+42,row+5,buffer,RGB(239,241,229));
                rect(x+290,row+12,220,8,RGB(58,72,82));
                rect(x+290,row+12,220*value/100,8,RGB(241,191,100));
                rect(x+286+220*value/100,row+7,8,18,RGB(248,231,185));
            }}
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
    label(x+28,y+menuHeight-33,"Arrows or mouse: adjust    Enter: choose    Esc: back",RGB(197,207,212));
}
void debugMenu(int width,int height){
    if(!debug_menu::open)return;
    const int w=640,h=570,x=(width-w)/2,y=(height-h)/2;
    rect(x,y,w,h,RGB(25,37,47));
    rect(x,y,w,5,RGB(255,193,99));
    label(x+25,y+20,"DEBUG MENU  /  F4",RGB(255,226,156));
    label(x+25,y+48,"Time, weather, weapons and health save with game progress",
        RGB(202,221,221));
    int total=debug_menu::entryCount();
    int first=std::clamp(debug_menu::selection-5,0,std::max(0,total-11));
    int last=std::min(total,first+11);
    for(int index=first;index<last;++index){
        int row=y+88+(index-first)*39;
        if(index==debug_menu::selection)rect(x+15,row-4,w-30,34,RGB(72,106,124));
        char entry[160]{};
        if(index==0)std::snprintf(entry,sizeof(entry),"God mode: %s",
            debug_menu::godMode?"ON":"OFF");
        else if(index==1)std::snprintf(entry,sizeof(entry),"Fly: %s  (Space up, Ctrl down)",
            debug_menu::flyMode?"ON":"OFF");
        else if(index==2)std::snprintf(entry,sizeof(entry),"Restore health to 400");
        else if(index==3){
            int hour=int(game::gameHour),minute=int((game::gameHour-hour)*60);
            std::snprintf(entry,sizeof(entry),"Time of day: < %02d:%02d >",hour,minute);
        }
        else if(index==4)std::snprintf(entry,sizeof(entry),"Weather: < %s >",
            weather::current().name.c_str());
        else{
            int weaponIndex=index-debug_menu::WEAPONS_START;
            std::snprintf(entry,sizeof(entry),"%2d. %s%s",weaponIndex+1,
                weapons::stats(weaponIndex).name.c_str(),
                game::weapon==weaponIndex?"  [EQUIPPED]":"");
        }
        label(x+33,row+2,entry,index==debug_menu::selection?
            RGB(255,238,171):RGB(225,236,232));
    }
    char range[80]{};
    std::snprintf(range,sizeof(range),"%d-%d of %d",first+1,last,total);
    label(x+25,y+526,range,RGB(195,215,218));
    label(x+168,y+526,"Up/Down select  Left/Right change  Enter choose  Esc/F4 close",
        RGB(195,215,218));
}
}
void buildHud(unsigned char* pixels,int width,int height){
    resize(width,height);
    if(!memoryDC||!dib){std::memset(pixels,0,size_t(width)*height*4);return;}
    std::memset(dib,0,size_t(width)*height*4);
    char textBuffer[240];
    rect(12,12,ui::showHelp?760:190,ui::showHelp?72:53,RGB(31,41,49));
    int hour=int(game::gameHour),minute=int((game::gameHour-hour)*60);
    label(24,19,"MINI CITY 3D",RGB(255,225,151));
    if(ui::showHelp&&game::occupied>=0){
        const auto& vehicle=game::vehicles[game::occupied];
        const char* name=vehicle.kind==game::Kind::Boat?"BOAT":vehicle.kind==game::Kind::Bike?"BIKE":
            vehicle.kind==game::Kind::SportCar?"SPORT CAR":"CAR";
        std::snprintf(textBuffer,sizeof(textBuffer),"%s  |  HP %d  |  SPACE drift  |  H lights  |  E exit",
            name,int(game::vehicleHealth(game::occupied)));
    }else if(ui::showHelp)std::snprintf(textBuffer,sizeof(textBuffer),
        "WASD move  |  SHIFT run  |  ALT slow  |  LCTRL crouch  |  RMB aim  |  LMB fire");
    if(ui::showHelp){
        label(220,19,textBuffer,RGB(223,230,230));
        label(24,49,"1-9 / Q guns  |  C camera  |  B telescope  |  E vehicle  |  F use  |  TAB choose  |  G carry",RGB(201,215,215));
    }else{
        std::snprintf(textBuffer,sizeof(textBuffer),"%s  |  F1 HELP",
            weather::current().name.c_str());
        label(24,40,textBuffer,RGB(211,230,236));
    }
    map(18,height-169,180,140,false);
    int statusX=width-360;
    rect(statusX,12,348,156,RGB(31,41,49));
    std::snprintf(textBuffer,sizeof(textBuffer),"%02d:%02d       $%06d",hour,minute,game::money);
    label(statusX+15,19,textBuffer,RGB(255,231,166));
    for(int star=0;star<4;++star)
        wantedStar(statusX+251+star*22,29,star<police::wantedLevel());
    rect(statusX+14,45,320,1,RGB(96,110,117));
    label(statusX+15,53,weapons::stats(game::weapon).name.c_str(),RGB(244,245,234));
    if(weapons::stats(game::weapon).melee)std::snprintf(textBuffer,sizeof(textBuffer),"MELEE");
    else if(game::reloadRemaining>0)std::snprintf(textBuffer,sizeof(textBuffer),"RELOADING");
    else if(game::ammo[game::weapon]<0)std::snprintf(textBuffer,sizeof(textBuffer),"%d / --",game::magazine[game::weapon]);
    else std::snprintf(textBuffer,sizeof(textBuffer),"%d / %d",game::magazine[game::weapon],game::ammo[game::weapon]);
    label(statusX+15,77,textBuffer,RGB(255,221,137));
    if(game::dualWieldActive(game::weapon))
        label(statusX+116,77,"DUAL",RGB(255,221,137));
    int gunX=statusX+225,gunY=64;
    const std::string& icon=weapons::stats(game::weapon).id;
    COLORREF steel=RGB(199,211,211),dark=RGB(92,111,117),wood=RGB(159,111,72);
    if(icon=="pistol"){
        rect(gunX+20,gunY,43,7,steel);rect(gunX+14,gunY+7,35,8,dark);
        rect(gunX+17,gunY+15,12,19,dark);rect(gunX+9,gunY+2,7,5,steel);
    }else if(icon=="silenced-pistol"){
        rect(gunX+21,gunY+4,41,7,steel);rect(gunX+9,gunY+5,16,6,dark);
        rect(gunX+19,gunY+11,30,8,dark);rect(gunX+24,gunY+19,11,18,dark);
    }else if(icon=="smg"){
        rect(gunX+7,gunY+4,72,7,steel);rect(gunX+16,gunY+11,47,7,dark);
        rect(gunX+32,gunY+18,10,17,dark);rect(gunX+5,gunY+13,15,5,steel);
    }else if(icon=="shotgun"){
        rect(gunX+5,gunY+4,82,5,steel);rect(gunX+5,gunY+10,82,4,wood);
        rect(gunX+19,gunY+14,29,7,wood);rect(gunX+14,gunY+21,12,12,wood);
    }else if(icon=="rifle"){
        rect(gunX+3,gunY+5,86,6,steel);rect(gunX+24,gunY+11,41,8,dark);
        rect(gunX+43,gunY+19,9,16,dark);rect(gunX+3,gunY+15,20,5,wood);
    }else if(icon=="sniper"){
        rect(gunX+2,gunY+6,88,5,steel);rect(gunX+23,gunY+11,47,7,wood);
        rect(gunX+33,gunY,28,5,dark);rect(gunX+37,gunY-3,17,3,steel);
        rect(gunX+45,gunY+18,8,16,dark);
    }else if(icon=="rpg"){
        rect(gunX+4,gunY+3,72,12,dark);rect(gunX+67,gunY,19,18,steel);
        rect(gunX+30,gunY+15,10,21,wood);rect(gunX+6,gunY+16,22,5,wood);
    }else if(icon=="flamethrower"){
        rect(gunX+5,gunY+9,80,8,steel);rect(gunX+23,gunY+17,30,16,dark);
        rect(gunX+65,gunY+5,20,4,RGB(239,121,51));
    }else if(icon=="fire-extinguisher"){
        rect(gunX+27,gunY+2,27,31,RGB(203,48,43));
        rect(gunX+33,gunY-3,18,5,steel);rect(gunX+55,gunY+1,28,4,dark);
    }else if(icon=="water-cannon"){
        rect(gunX+9,gunY+8,70,10,RGB(49,124,170));
        rect(gunX+27,gunY+18,35,14,dark);rect(gunX+66,gunY+4,16,5,steel);
    }else if(icon=="katana"){
        rect(gunX+8,gunY+10,22,6,dark);rect(gunX+29,gunY+8,9,10,wood);
        rect(gunX+38,gunY+10,54,4,steel);
    }else if(icon=="knife"){
        rect(gunX+20,gunY+12,29,9,wood);rect(gunX+47,gunY+10,7,13,dark);
        rect(gunX+54,gunY+12,33,6,steel);
    }else if(icon=="machete"){
        rect(gunX+10,gunY+12,23,8,wood);rect(gunX+32,gunY+10,8,12,dark);
        rect(gunX+40,gunY+8,49,12,steel);
    }else if(icon=="novelty-toy"){
        rect(gunX+24,gunY+12,49,13,RGB(222,104,166));
        rect(gunX+16,gunY+15,12,7,RGB(185,73,130));
    }else if(icon=="rolling-pin"){
        rect(gunX+14,gunY+11,15,7,wood);rect(gunX+29,gunY+7,48,15,RGB(197,153,101));
        rect(gunX+77,gunY+11,15,7,wood);
    }else if(icon=="bat"){
        rect(gunX+12,gunY+12,29,7,wood);rect(gunX+39,gunY+8,53,15,RGB(185,127,72));
    }else if(icon=="bow"){
        line(gunX+48,gunY-2,gunX+74,gunY+17,wood,4);
        line(gunX+74,gunY+17,gunX+48,gunY+36,wood,4);
        line(gunX+48,gunY-2,gunX+48,gunY+36,steel,1);
        line(gunX+28,gunY+17,gunX+79,gunY+17,steel,2);
    }else{
        rect(gunX+8,gunY+6,78,8,steel);rect(gunX+24,gunY+14,12,19,dark);
    }
    std::snprintf(textBuffer,sizeof(textBuffer),"HP %d/%d",
        int(game::health),int(game::PLAYER_MAX_HEALTH));
    label(statusX+15,105,textBuffer,RGB(243,233,215));
    rect(statusX+130,105,204,18,RGB(104,55,59));
    rect(statusX+130,105,int(204*std::clamp(game::health,0.0f,game::PLAYER_MAX_HEALTH)/game::PLAYER_MAX_HEALTH),18,
        game::health>game::PLAYER_MAX_HEALTH*0.35f?
            RGB(72,208,104):RGB(237,139,82));
    std::snprintf(textBuffer,sizeof(textBuffer),"ARMOR %d",int(game::armor));
    label(statusX+15,130,textBuffer,RGB(206,224,243));
    rect(statusX+130,130,204,18,RGB(48,74,105));
    rect(statusX+130,130,int(204*std::clamp(game::armor,0.0f,100.0f)/100.0f),18,
        RGB(80,159,225));
    if(game::activeMission>=0){
        const auto& mission=game::missions[game::activeMission];
        rect(width-360,181,348,96,RGB(31,41,49));
        std::snprintf(textBuffer,sizeof(textBuffer),"%s   %d sec",mission.name,int(game::missionTime));
        label(width-346,189,textBuffer,RGB(255,221,137));
        std::snprintf(textBuffer,sizeof(textBuffer),"Objective %d / %d",
            game::missionStep+1,int(mission.goals.size()));
        label(width-346,218,textBuffer,RGB(225,235,224));
        label(width-346,246,game::missionObjective(),RGB(225,235,224));
    }else if(game::nextMission()>=0){
        int next=game::nextMission();
        rect(width-360,181,348,58,RGB(31,41,49));
        std::snprintf(textBuffer,sizeof(textBuffer),"NEXT: %s",game::missions[next].name);
        label(width-346,189,textBuffer,RGB(255,221,137));
        label(width-346,214,"Follow gold map line, then press F",RGB(225,235,224));
    }
    std::string prompt=game::interactionPrompt();
    std::string carry=game::carryPrompt();
    if(!prompt.empty())label(width/2-125,height-74,prompt.c_str(),RGB(255,226,153));
    if(!carry.empty())label(width/2-125,height-51,carry.c_str(),RGB(210,228,244));
    if(camera::zoomActive()&&game::scopeBlend>0.85f&&!ui::paused())scopeOverlay(width,height);
    else if(game::occupied<0&&game::rightMouse&&!ui::paused()){
        int x=width/2,y=height/2,kick=int(game::recoil*9);
        rect(x-10-kick,y-1,7,2,RGB(251,244,215));rect(x+4+kick,y-1,7,2,RGB(251,244,215));
        rect(x-1,y-10-kick,2,7,RGB(251,244,215));rect(x-1,y+4+kick,2,7,RGB(251,244,215));
    }
    if(game::showMap){
        int mapW=std::min(width-120,650),mapH=std::min(height-130,
            int(mapW*regions::DEPTH/regions::WIDTH));
        int x=(width-mapW)/2,y=(height-mapH)/2;
        rect(x-25,y-49,mapW+50,mapH+100,RGB(30,41,51));
        label(x,y-31,"MAP: yellow route  orange jobs  green shops  blue owned  purple homes",RGB(247,238,211));
        map(x,y,mapW,mapH,true);
    }
    if(game::messageTime>0){
        rect(width/2-310,height-65,620,44,RGB(31,41,49));
        label(width/2-294,height-55,game::message.c_str(),RGB(255,225,154));
    }
    if(game::missionBannerTime>0&&!ui::paused()){
        int left=width/2-315,top=height/2-152;
        rect(left,top,630,104,RGB(27,36,45));
        rect(left,top,630,5,RGB(246,189,85));
        HGDIOBJ previous=SelectObject(memoryDC,bannerFont);
        SIZE titleSize{};
        GetTextExtentPoint32A(memoryDC,game::missionBannerTitle.c_str(),
            int(game::missionBannerTitle.size()),&titleSize);
        label(width/2-titleSize.cx/2,top+16,game::missionBannerTitle.c_str(),RGB(255,222,141));
        SelectObject(memoryDC,previous);
        SIZE detailSize{};
        GetTextExtentPoint32A(memoryDC,game::missionBannerDetail.c_str(),
            int(game::missionBannerDetail.size()),&detailSize);
        label(width/2-detailSize.cx/2,top+70,game::missionBannerDetail.c_str(),RGB(228,236,230));
    }
    if(game::health<=0){rect(width/2-145,height/2-48,290,96,RGB(35,42,48));
        label(width/2-65,height/2-24,"YOU DIED",RGB(252,124,115));
        label(width/2-94,height/2+8,"Press R to restart",RGB(247,242,233));}
    if(game::debugHud){rect(width-276,height-166,261,152,RGB(30,42,48));
        std::snprintf(textBuffer,sizeof(textBuffer),"FPS %.0f   FRAME %.1f ms",game::frameRate,game::frameMs);
        label(width-265,height-157,textBuffer,RGB(224,245,220));
        std::snprintf(textBuffer,sizeof(textBuffer),"SIM %.2f   PHYS %.2f ms",game::simulationMs,game::physicsMs);
        label(width-265,height-129,textBuffer,RGB(224,245,220));
        std::snprintf(textBuffer,sizeof(textBuffer),"DRAWS %d   ACTIVE AI %d",game::drawCalls,game::activeAi);
        label(width-265,height-101,textBuffer,RGB(224,245,220));
#ifdef MINI_CITY_JOLT
        std::snprintf(textBuffer,sizeof(textBuffer),"JOLT BLD %zu   PED %zu   RAG %zu",
            jolt_world::activeBuildingColliderCount(),
            jolt_world::activePedCharacterCount(),game::ragdollParts.size()/6);
        label(width-265,height-73,textBuffer,RGB(224,245,220));
#endif
        std::snprintf(textBuffer,sizeof(textBuffer),"FIRE %zu   SHOTS %zu   PROPS %zu",
            fire::active().size(),game::bullets.size(),game::props.size());
        label(width-265,height-45,textBuffer,RGB(224,245,220));}
    if(ui::showHelp&&!ui::paused()&&!debug_menu::open)
        label(18,height-28,"F4 DEBUG MENU",RGB(197,211,213));
    if(commerce::menu()!=commerce::Menu::None&&!ui::paused()){
        auto entries=commerce::menuEntries();
        int x=width/2-300,y=height/2-275;
        rect(x,y,600,550,RGB(29,40,50));
        rect(x,y,600,5,RGB(111,218,164));
        std::string title=commerce::menuTitle();
        label(x+25,y+18,title.c_str(),RGB(245,234,190));
        std::snprintf(textBuffer,sizeof(textBuffer),"CASH $%d  |  OWNED HOUSES %d / 10",
            game::money,commerce::ownedHouseCount());
        label(x+25,y+47,textBuffer,RGB(217,228,222));
        int selected=commerce::selected();
        int first=std::clamp(selected-5,0,std::max(0,int(entries.size())-11));
        int last=std::min(int(entries.size()),first+11);
        for(int index=first;index<last;++index){
            int row=y+83+(index-first)*38;
            if(index==selected)rect(x+15,row-3,570,33,RGB(72,107,124));
            const auto& entry=entries[index];
            label(x+26,row+3,entry.label.c_str(),
                entry.available?RGB(233,242,224):RGB(145,158,161));
            if(entry.price>0){
                std::snprintf(textBuffer,sizeof(textBuffer),"$%d",entry.price);
                label(x+480,row+3,textBuffer,
                    entry.available?RGB(255,221,141):RGB(145,158,161));
            }
        }
        label(x+24,y+515,"UP/DOWN select   ENTER buy/use   ESC close",RGB(198,215,215));
    }
    debugMenu(width,height);
    pauseMenu(width,height);
    for(size_t i=0;i<size_t(width)*height;++i)
        if(dib[i*4]||dib[i*4+1]||dib[i*4+2])dib[i*4+3]=255;
    std::memcpy(pixels,dib,size_t(width)*height*4);
}
void shutdownHud(){
    if(memoryDC){if(oldFont)SelectObject(memoryDC,oldFont);if(font)DeleteObject(font);
        if(bannerFont)DeleteObject(bannerFont);
        if(oldBitmap)SelectObject(memoryDC,oldBitmap);if(bitmap)DeleteObject(bitmap);
        DeleteDC(memoryDC);}
    memoryDC=nullptr;bitmap=nullptr;oldBitmap=nullptr;font=nullptr;bannerFont=nullptr;oldFont=nullptr;dib=nullptr;
    canvasW=canvasH=0;
}
}
