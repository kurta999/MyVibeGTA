#include <ctime>
#include <cstdlib>
#include "game.h"
#include "audio.h"
#include "ui.h"
#include "savegame.h"
#include "input.h"
#include "logging.h"
#include "weapons.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif
#include <cstring>
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR commandLine,int show){
    using namespace game;
    bool smoke=commandLine&&std::strstr(commandLine,"--smoke")!=nullptr;
    logging::initialize();
    std::srand(unsigned(std::time(nullptr)));
    WNDCLASSA wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=input::windowProc;wc.hInstance=instance;
    wc.hCursor=LoadCursor(nullptr,IDC_CROSS);wc.lpszClassName="MiniCity3D";
    if(!RegisterClassA(&wc)){logging::write("Window class registration failed");logging::shutdown();return 1;}
    win=CreateWindowExA(0,wc.lpszClassName,"Mini City 3D - Direct3D 11",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,1620,940,nullptr,nullptr,instance,nullptr);
    if(!win){logging::write("Window creation failed");logging::shutdown();return 1;}
    RAWINPUTDEVICE mouse{};mouse.usUsagePage=0x01;mouse.usUsage=0x02;
    mouse.dwFlags=0;mouse.hwndTarget=win;RegisterRawInputDevices(&mouse,1,sizeof(mouse));
    RECT area{};GetClientRect(win,&area);screenW=area.right;screenH=area.bottom;
    if(!initRenderer()){if(!smoke)MessageBoxA(win,"Graphics initialization failed. Check the GPU and assets folder.",
        "Mini City 3D",MB_ICONERROR);logging::write("Renderer initialization failed");logging::shutdown();return 1;}
    logging::write("Renderer initialized");
    if(!audio::init())logging::write("XAudio2 initialization failed; continuing without audio");
    ui::load();
    if(!weapons::load())logging::write("Weapon data missing; using built-in defaults");
    reset();
    if(!smoke&&savegame::load()){message="Saved progress loaded. Press F near a marker for a mission.";messageTime=5;
        logging::write("Saved progress loaded");}
    if(smoke&&commandLine&&std::strstr(commandLine,"--day"))gameHour=12;
    if(smoke&&commandLine&&std::strstr(commandLine,"--night"))gameHour=22;
    if(smoke&&commandLine&&std::strstr(commandLine,"--no-shadows"))ui::shadowQuality=0;
#ifdef MINI_CITY_JOLT
    if(smoke&&commandLine&&std::strstr(commandLine,"--ragdoll")){
        Ped fallen{};fallen.p={player.x+65,player.z};fallen.style=0;
        jolt_world::spawnRagdoll(fallen,{1100,0,180});
        for(int tick=0;tick<40;++tick)jolt_world::step(1.0f/60.0f);
    }
#endif
    if(smoke){render();audio::shutdown();shutdownRenderer();DestroyWindow(win);
        logging::write("Smoke render completed");logging::shutdown();return 0;}
    ShowWindow(win,show);
    LARGE_INTEGER frequency{},last{},now{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&last);
    bool running=true;double accumulator=0,fpsTimer=0;int frames=0,overloadEvents=0;
    constexpr double fixedStep=1.0/60.0;
    while(running){MSG msg{};while(PeekMessageA(&msg,nullptr,0,0,PM_REMOVE)){
        if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageA(&msg);}
        if(!running)break;
        QueryPerformanceCounter(&now);
        double dt=std::clamp(double(now.QuadPart-last.QuadPart)/double(frequency.QuadPart),0.0,0.25);last=now;
        frameMs=frameMs*0.9f+float(dt*1000.0)*0.1f;
        if(!ui::paused())accumulator+=dt;
        else accumulator=0;
        int steps=0;
        while(accumulator>=fixedStep&&steps<5){
            LARGE_INTEGER beforeStep{},afterStep{};QueryPerformanceCounter(&beforeStep);
            update(float(fixedStep));QueryPerformanceCounter(&afterStep);
            float elapsed=float((afterStep.QuadPart-beforeStep.QuadPart)*1000.0/double(frequency.QuadPart));
            simulationMs=simulationMs*0.9f+elapsed*0.1f;
            accumulator-=fixedStep;++steps;
        }
        if(steps==5){accumulator=0;if(overloadEvents++%120==0)
            logging::write("Simulation fell behind; accumulated time dropped");}
        renderAlpha=float(accumulator/fixedStep);
        render();++frames;fpsTimer+=dt;
        if(fpsTimer>=0.5){frameRate=float(frames/fpsTimer);frames=0;fpsTimer=0;}
        Sleep(1);
    }
    audio::shutdown();shutdownRenderer();logging::shutdown();
    return 0;
}

