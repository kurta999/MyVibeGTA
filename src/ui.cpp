#include "ui.h"
#include "game.h"
#include "audio.h"
#include "savegame.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace ui {
Page page=Page::Closed;
int selection=0,graphicsQuality=2,shadowQuality=1,reflectionQuality=1,
    antiAliasingQuality=1,aoQuality=1,vegetationDensity=2,grassDistance=50,
    effectsQuality=2,drawDistance=21,lodDistance=50,windowChoice=1,
    mouseSensitivity=7,masterVolume=80,waitingForBinding=-1;
bool invertY=false;
bool showHelp=false;
std::array<int,int(Action::Count)> bindings{{'W','S','A','D',VK_SHIFT,'E'}};

namespace {
std::string configPath(){
    char path[MAX_PATH]{};GetModuleFileNameA(nullptr,path,MAX_PATH);
    std::string result(path);auto slash=result.find_last_of("\\/");
    return result.substr(0,slash+1)+"settings.ini";
}
void applyWindow(){
    if(!game::win)return;
    const int widths[]={1280,1600,1920};const int heights[]={720,900,1080};
    RECT r{0,0,widths[windowChoice],heights[windowChoice]};
    AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
    SetWindowPos(game::win,nullptr,0,0,r.right-r.left,r.bottom-r.top,SWP_NOMOVE|SWP_NOZORDER);
}
int count(Page p){return p==Page::Main?7:p==Page::Graphics?11:p==Page::Controls?8:p==Page::Audio?1:0;}
void writeValue(const char* section,const char* key,int value,const std::string& path){
    char text[32];std::snprintf(text,sizeof(text),"%d",value);
    WritePrivateProfileStringA(section,key,text,path.c_str());
}
}
bool paused(){return page!=Page::Closed;}
float drawDistanceScale(){
    int value=std::clamp(drawDistance,0,100);
    return value<=50?0.65f+0.85f*value/50.0f:
        1.5f+6.0f*(value-50)/50.0f;
}
float lodDistanceScale(){
    return 0.55f+1.95f*std::clamp(lodDistance,0,100)/100.0f;
}
void load(){
    std::string path=configPath();
    graphicsQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","Quality",2,path.c_str())),0,2);
    shadowQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","Shadows",1,path.c_str())),0,2);
    reflectionQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","Reflections",1,path.c_str())),0,2);
    antiAliasingQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","AntiAliasing",1,path.c_str())),0,2);
    aoQuality=std::clamp(int(GetPrivateProfileIntA("Graphics","SSAO",1,path.c_str())),0,2);
    vegetationDensity=std::clamp(int(GetPrivateProfileIntA("Graphics","Vegetation",2,path.c_str())),0,2);
    grassDistance=std::clamp(int(GetPrivateProfileIntA("Graphics","GrassDistance",50,path.c_str())),0,100);
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
    mouseSensitivity=std::clamp(int(GetPrivateProfileIntA("Controls","Sensitivity",7,path.c_str())),1,20);
    invertY=GetPrivateProfileIntA("Controls","InvertY",0,path.c_str())!=0;
    masterVolume=std::clamp(int(GetPrivateProfileIntA("Audio","Volume",80,path.c_str())),0,100);
    const char* names[]={"Forward","Backward","Left","Right","Sprint","Interact"};
    for(int i=0;i<int(bindings.size());++i)bindings[i]=GetPrivateProfileIntA("Controls",names[i],bindings[i],path.c_str());
    audio::setVolume(masterVolume);
    applyWindow();
}
void save(){
    std::string path=configPath();
    writeValue("Graphics","Quality",graphicsQuality,path);
    writeValue("Graphics","Shadows",shadowQuality,path);
    writeValue("Graphics","Reflections",reflectionQuality,path);
    writeValue("Graphics","AntiAliasing",antiAliasingQuality,path);
    writeValue("Graphics","SSAO",aoQuality,path);
    writeValue("Graphics","Vegetation",vegetationDensity,path);
    writeValue("Graphics","GrassDistance",grassDistance,path);
    writeValue("Graphics","Effects",effectsQuality,path);
    writeValue("Graphics","DrawDistance",drawDistance,path);
    writeValue("Graphics","LodDistance",lodDistance,path);
    writeValue("Graphics","DistanceVersion",1,path);
    writeValue("Graphics","WindowSize",windowChoice,path);
    writeValue("Graphics","ResolutionVersion",2,path);
    writeValue("Controls","Sensitivity",mouseSensitivity,path);
    writeValue("Controls","InvertY",invertY?1:0,path);
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
        if(selection==0&&direction)graphicsQuality=std::clamp(graphicsQuality+direction,0,2);
        if(selection==1&&direction){windowChoice=std::clamp(windowChoice+direction,0,2);applyWindow();}
        if(selection==2&&direction)vegetationDensity=std::clamp(vegetationDensity+direction,0,2);
        if(selection==3&&direction)effectsQuality=std::clamp(effectsQuality+direction,0,2);
        if(selection==4&&direction)shadowQuality=std::clamp(shadowQuality+direction,0,2);
        if(selection==5&&direction)reflectionQuality=std::clamp(reflectionQuality+direction,0,2);
        if(selection==6&&direction)antiAliasingQuality=std::clamp(antiAliasingQuality+direction,0,2);
        if(selection==7&&direction)aoQuality=std::clamp(aoQuality+direction,0,2);
        if(selection==8&&direction)drawDistance=std::clamp(drawDistance+direction*2,0,100);
        if(selection==9&&direction)lodDistance=std::clamp(lodDistance+direction*2,0,100);
        if(selection==10&&direction)grassDistance=std::clamp(grassDistance+direction*2,0,100);
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
    int top=(client.bottom-client.top)/2-320;
    for(int row=8;row<=10;++row){
        int rowY=top+105+row*42;
        if(y<rowY-5||y>rowY+38)continue;
        if(!dragging&&x<left+285)return;
        selection=row;
        int value=std::clamp((x-(left+290))*100/220,0,100);
        int& setting=row==8?drawDistance:row==9?lodDistance:grassDistance;
        if(setting!=value){setting=value;save();}
        return;
    }
}
}
