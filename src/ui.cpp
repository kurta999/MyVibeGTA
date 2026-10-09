#include "ui.h"
#include "game.h"
#include "audio.h"
#ifdef MINI_CITY_JOLT
#include "radio.h"
#endif
#include "savegame.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace ui {
Page page=Page::Closed;
int selection=0,graphicsQuality=2,shadowQuality=1,reflectionQuality=1,
    antiAliasingQuality=1,aoQuality=1,textureQuality=2,filteringQuality=2,
    vegetationDensity=2,grassDistance=50,grassLodDistance=50,
    effectsQuality=2,drawDistance=21,lodDistance=50,windowChoice=1,
    windowMode=0,mouseSensitivity=7,masterVolume=80,waitingForBinding=-1;
bool invertY=false;
int fsr2Quality=1,fsr2Sharpness=20;
bool showHelp=false;
std::array<int,int(Action::Count)> bindings{{'W','S','A','D',VK_SHIFT,'E'}};

namespace {
bool applyingWindow=false;
std::string configPath(){
    char path[MAX_PATH]{};GetModuleFileNameA(nullptr,path,MAX_PATH);
    std::string result(path);auto slash=result.find_last_of("\\/");
    return result.substr(0,slash+1)+"settings.ini";
}
int count(Page p){
#ifdef MINI_CITY_DX12
    constexpr int graphicsRows=17;
#else
    constexpr int graphicsRows=15;
#endif
    return p==Page::Main?7:p==Page::Graphics?graphicsRows:p==Page::Controls?8:p==Page::Audio?1:0;
}
void writeValue(const char* section,const char* key,int value,const std::string& path){
    char text[32];std::snprintf(text,sizeof(text),"%d",value);
    WritePrivateProfileStringA(section,key,text,path.c_str());
}
}
void applyWindow(){
    if(!game::win||applyingWindow)return;
    applyingWindow=true;
    windowMode=std::clamp(windowMode,0,2);windowChoice=std::clamp(windowChoice,0,2);
    const int widths[]={1280,1600,1920},heights[]={720,900,1080};
#ifdef MINI_CITY_JOLT
    game::setExclusiveFullscreen(false,widths[windowChoice],heights[windowChoice]);
#endif
    MONITORINFO monitor{sizeof(MONITORINFO)};
    GetMonitorInfoA(MonitorFromWindow(game::win,MONITOR_DEFAULTTONEAREST),&monitor);
    auto area=monitor.rcMonitor;
    if(windowMode==0){
        SetWindowLongPtrA(game::win,GWL_STYLE,WS_OVERLAPPEDWINDOW);
        RECT r{0,0,widths[windowChoice],heights[windowChoice]};AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
        SetWindowPos(game::win,HWND_NOTOPMOST,monitor.rcWork.left+20,monitor.rcWork.top+20,r.right-r.left,r.bottom-r.top,SWP_FRAMECHANGED);
    }else{
        SetWindowLongPtrA(game::win,GWL_STYLE,WS_POPUP);
        SetWindowPos(game::win,HWND_NOTOPMOST,area.left,area.top,area.right-area.left,area.bottom-area.top,SWP_FRAMECHANGED);
#ifdef MINI_CITY_JOLT
        if(windowMode==2&&IsWindowVisible(game::win)&&GetForegroundWindow()==game::win&&
           !game::setExclusiveFullscreen(true,widths[windowChoice],heights[windowChoice])){
            windowMode=1;game::message="Fullscreen unavailable; using borderless.";game::messageTime=3;
        }
#endif
    }
    applyingWindow=false;
}
void setWindowActive(bool active){
#ifdef MINI_CITY_JOLT
    if(!game::win||applyingWindow||windowMode!=2)return;
    const int widths[]={1280,1600,1920},heights[]={720,900,1080};
    applyingWindow=true;
    bool ok=game::setExclusiveFullscreen(active,widths[std::clamp(windowChoice,0,2)],heights[std::clamp(windowChoice,0,2)]);
    applyingWindow=false;
    if(active&&!ok){windowMode=1;applyWindow();game::message="Fullscreen unavailable; using borderless.";game::messageTime=3;}
#else
    (void)active;
#endif
}
void toggleFullscreen(){windowMode=windowMode==0?1:0;applyWindow();save();}
bool paused(){return page!=Page::Closed;}
float drawDistanceScale(){
    int value=std::clamp(drawDistance,0,100);
    return value<=50?0.65f+0.85f*value/50.0f:
        1.5f+6.0f*(value-50)/50.0f;
}
float lodDistanceScale(){
    return 0.55f+1.95f*std::clamp(lodDistance,0,100)/100.0f;
}
float grassDistanceUnits(){
    float value=float(std::clamp(grassDistance,0,100))/100;
    return value==0?0:40+760*value*value;
}
float grassLodDistanceUnits(){
    float value=float(std::clamp(grassLodDistance,0,100));
    if(value<=50)return 40+55*value/50;
    float extended=(value-50)/50;
    return 95+705*extended*extended;
}
void load(){
    std::string path=configPath();
    fsr2Quality=std::clamp(int(GetPrivateProfileIntA("Graphics","FSR2",1,path.c_str())),0,4);
    fsr2Sharpness=std::clamp(int(GetPrivateProfileIntA("Graphics","FSR2Sharpness",20,path.c_str())),0,100);
    graphicsQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","Quality",2,path.c_str())),0,2);
    shadowQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","Shadows",1,path.c_str())),0,2);
    reflectionQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","Reflections",1,path.c_str())),0,2);
    antiAliasingQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","AntiAliasing",1,path.c_str())),0,2);
    aoQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","SSAO",1,path.c_str())),0,2);
    textureQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","Textures",2,path.c_str())),0,2);
    filteringQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","Filtering",2,path.c_str())),0,2);
    vegetationDensity=std::clamp(int(GetPrivateProfileIntA("Graphics","Vegetation",2,path.c_str())),0,2);
    grassDistance=std::clamp(int(GetPrivateProfileIntA("Graphics","GrassDistance",50,path.c_str())),0,100);
    grassLodDistance=std::clamp(int(GetPrivateProfileIntA("Graphics","GrassLodDistance",50,path.c_str())),0,100);
    effectsQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","Effects",2,path.c_str())),0,2);
    int distanceVersion=GetPrivateProfileIntA("Graphics","DistanceVersion",0,path.c_str());
    drawDistance=std::clamp(int(GetPrivateProfileIntA("Graphics","DrawDistance",distanceVersion?21:1,path.c_str())),0,100);
    lodDistance=std::clamp(int(GetPrivateProfileIntA("Graphics","LodDistance",distanceVersion?50:1,path.c_str())),0,100);
    if(distanceVersion<1){
        const int oldDraw[]={0,21,50},oldLod[]={0,23,59};
        drawDistance=oldDraw[std::clamp(drawDistance,0,2)];
        lodDistance=oldLod[std::clamp(lodDistance,0,2)];
    }
    int resolutionVersion=GetPrivateProfileIntA("Graphics","ResolutionVersion",0,path.c_str());
    windowChoice=resolutionVersion<2?1:std::clamp(int(GetPrivateProfileIntA("Graphics","WindowSize",1,path.c_str())),0,2);
    windowMode=std::clamp(int(GetPrivateProfileIntA("Graphics","WindowMode",0,path.c_str())),0,2);
    mouseSensitivity=std::clamp(int(GetPrivateProfileIntA("Controls","Sensitivity",7,path.c_str())),1,20);
    invertY=GetPrivateProfileIntA("Controls","InvertY",0,path.c_str())!=0;
    masterVolume=std::clamp(int(GetPrivateProfileIntA("Audio","Volume",80,path.c_str())),0,100);
    const char* names[]={"Forward","Backward","Left","Right","Sprint","Interact"};
    for(int i=0;i<int(bindings.size());++i)bindings[i]=GetPrivateProfileIntA("Controls",names[i],bindings[i],path.c_str());
#ifdef MINI_CITY_JOLT
    char station[80]{};GetPrivateProfileStringA("Audio","RadioStation","off",station,sizeof(station),path.c_str());radio::select(station);
#endif
    audio::setVolume(masterVolume);
    applyWindow();
}
void save(){
    std::string path=configPath();
    writeValue("Graphics","FSR2",fsr2Quality,path);
    writeValue("Graphics","FSR2Sharpness",fsr2Sharpness,path);
    writeValue("Graphics","Quality",graphicsQuality,path);
    writeValue("Graphics","Shadows",shadowQuality,path);
    writeValue("Graphics","Reflections",reflectionQuality,path);
    writeValue("Graphics","AntiAliasing",antiAliasingQuality,path);
    writeValue("Graphics","SSAO",aoQuality,path);
    writeValue("Graphics","Textures",textureQuality,path);
    writeValue("Graphics","Filtering",filteringQuality,path);
    writeValue("Graphics","Vegetation",vegetationDensity,path);
    writeValue("Graphics","GrassDistance",grassDistance,path);
    writeValue("Graphics","GrassLodDistance",grassLodDistance,path);
    writeValue("Graphics","Effects",effectsQuality,path);
    writeValue("Graphics","DrawDistance",drawDistance,path);
    writeValue("Graphics","LodDistance",lodDistance,path);
    writeValue("Graphics","DistanceVersion",1,path);
    writeValue("Graphics","WindowMode",windowMode,path);
    writeValue("Graphics","WindowSize",windowChoice,path);
    writeValue("Graphics","ResolutionVersion",2,path);
    writeValue("Controls","Sensitivity",mouseSensitivity,path);
    writeValue("Controls","InvertY",invertY?1:0,path);
#ifdef MINI_CITY_JOLT
    WritePrivateProfileStringA("Audio","RadioStation",radio::selectedId().c_str(),path.c_str());
#endif
    writeValue("Audio","Volume",masterVolume,path);
    const char* names[]={"Forward","Backward","Left","Right","Sprint","Interact"};
    for(int i=0;i<int(bindings.size());++i)writeValue("Controls",names[i],bindings[i],path);
}
const char* keyName(int key){
    static char label[32];
    if(key==VK_SHIFT)return "SHIFT";
    if(key==VK_CONTROL)return "CTRL";
    if(key==VK_SPACE)return "SPACE";
    if(key>=32&&key<127){label[0]=char(key);label[1]=0;return label;}
    std::snprintf(label,sizeof(label),"KEY %d",key);return label;
}
void handleKey(int key){
    if(waitingForBinding>=0){
        if(key!=VK_ESCAPE){bindings[waitingForBinding]=key;save();}
        waitingForBinding=-1;return;
    }
    if(key==VK_ESCAPE){
        if(page==Page::Closed)page=Page::Main;
        else if(page==Page::Main)page=Page::Closed;
        else page=Page::Main;
        selection=0;return;
    }
    if(page==Page::Closed)return;
    if(key==VK_UP)selection=(selection+count(page)-1)%count(page);
    if(key==VK_DOWN)selection=(selection+1)%count(page);
    int direction=key==VK_LEFT?-1:key==VK_RIGHT?1:0;
    if(page==Page::Main&&key==VK_RETURN){
        if(selection==0)page=Page::Closed;
        else if(selection==1)page=Page::Graphics;
        else if(selection==2)page=Page::Controls;
        else if(selection==3)page=Page::Audio;
        else if(selection==4){bool ok=savegame::save();game::message=ok?"Game saved.":"Could not save game.";game::messageTime=3;page=Page::Closed;}
        else if(selection==5){bool ok=savegame::load();game::message=ok?"Saved game loaded.":"No saved game found.";game::messageTime=3;page=Page::Closed;}
        else if(selection==6)DestroyWindow(game::win);
        selection=0;return;
    }
    if(page==Page::Graphics){
        if(selection==15&&direction)fsr2Quality=std::clamp(fsr2Quality+direction,0,4);
        if(selection==16&&direction)fsr2Sharpness=std::clamp(fsr2Sharpness+direction*5,0,100);
        if(selection==0&&direction)graphicsQuality=std::clamp(graphicsQuality+direction,0,2);
        if(selection==1&&direction){windowChoice=std::clamp(windowChoice+direction,0,2);applyWindow();}
        if(selection==2&&direction)vegetationDensity=std::clamp(vegetationDensity+direction,0,2);
        if(selection==3&&direction)effectsQuality=std::clamp(effectsQuality+direction,0,2);
        if(selection==4&&direction)shadowQuality=std::clamp(shadowQuality+direction,0,2);
        if(selection==5&&direction)reflectionQuality=std::clamp(reflectionQuality+direction,0,2);
        if(selection==6&&direction)antiAliasingQuality=std::clamp(antiAliasingQuality+direction,0,2);
        if(selection==7&&direction)aoQuality=std::clamp(aoQuality+direction,0,2);
        if(selection==8&&direction)textureQuality=std::clamp(textureQuality+direction,0,2);
        if(selection==9&&direction)filteringQuality=std::clamp(filteringQuality+direction,0,2);
        if(selection==10&&direction)drawDistance=std::clamp(drawDistance+direction*2,0,100);
        if(selection==11&&direction)lodDistance=std::clamp(lodDistance+direction*2,0,100);
        if(selection==12&&direction)grassDistance=std::clamp(grassDistance+direction*2,0,100);
        if(selection==14&&direction){windowMode=std::clamp(windowMode+direction,0,2);applyWindow();}
        if(selection==13&&direction)grassLodDistance=std::clamp(grassLodDistance+direction*2,0,100);
    }
    if(page==Page::Controls){
        if(selection==0&&direction)mouseSensitivity=std::clamp(mouseSensitivity+direction,1,20);
        if(selection==1&&(direction||key==VK_RETURN))invertY=!invertY;
        if(selection>=2&&key==VK_RETURN)waitingForBinding=selection-2;
    }
    if(page==Page::Audio&&direction){masterVolume=std::clamp(masterVolume+direction*10,0,100);
        audio::setVolume(masterVolume);}
    if(direction||key==VK_RETURN)save();
}
void handleMouse(int x,int y,bool dragging){
    if(page!=Page::Graphics)return;
    RECT client{};GetClientRect(game::win,&client);
    int left=(client.right-client.left)/2-280;
    int top=((client.bottom-client.top)-GRAPHICS_MENU_HEIGHT)/2;
    for(int row=10;row<count(Page::Graphics);++row){
        if(row==14||row==15)continue;
        int rowY=top+105+row*GRAPHICS_ROW_HEIGHT;
        if(y<rowY-4||y>rowY+GRAPHICS_ROW_HEIGHT-4)continue;
        if(!dragging&&x<left+285)return;
        selection=row;
        int value=std::clamp((x-(left+290))*100/220,0,100);
        int& setting=row==10?drawDistance:row==11?lodDistance:row==12?grassDistance:row==13?grassLodDistance:fsr2Sharpness;
        if(setting!=value){setting=value;save();}
        return;
    }
}
}
