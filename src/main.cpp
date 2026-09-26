#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <array>
#include <cmath>
#include "game.h"
#include "audio.h"
#include "ui.h"
#include "savegame.h"
#include "input.h"
#include "logging.h"
#include "weapons.h"
#ifdef MINI_CITY_JOLT
#include "debug_menu.h"
#endif
#include "content.h"
#include "physics.h"
#include "fire.h"
#include "police.h"
#include "commerce.h"
#include "traversal.h"
#include "weather.h"
#include "regions.h"
#ifdef MINI_CITY_JOLT
#include "jolt_world.h"
#endif
#include <cstring>
#include <cstdio>
#include <psapi.h>
int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR commandLine,int show){
    using namespace game;
    bool smoke=commandLine&&std::strstr(commandLine,"--smoke")!=nullptr;
    bool benchmark=smoke&&commandLine&&std::strstr(commandLine,"--benchmark")!=nullptr;
    bool benchmarkTravel=benchmark&&commandLine&&
        std::strstr(commandLine,"--benchmark-travel")!=nullptr;
    bool fullHd=smoke&&commandLine&&std::strstr(commandLine,"--1080p")!=nullptr;
    logging::initialize();
    std::srand(unsigned(std::time(nullptr)));
    WNDCLASSA wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=input::windowProc;wc.hInstance=instance;
    wc.hCursor=LoadCursor(nullptr,IDC_CROSS);wc.lpszClassName="MiniCity3D";
    if(!RegisterClassA(&wc)){logging::write("Window class registration failed");logging::shutdown();return 1;}
    win=CreateWindowExA(0,wc.lpszClassName,"Mini City 3D - Direct3D 11",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,fullHd?1940:1620,fullHd?1120:940,
        nullptr,nullptr,instance,nullptr);
    if(!win){logging::write("Window creation failed");logging::shutdown();return 1;}
    RAWINPUTDEVICE mouse{};mouse.usUsagePage=0x01;mouse.usUsage=0x02;
    mouse.dwFlags=0;mouse.hwndTarget=win;RegisterRawInputDevices(&mouse,1,sizeof(mouse));
    RECT area{};GetClientRect(win,&area);screenW=area.right;screenH=area.bottom;
    if(!initRenderer()){if(!smoke)MessageBoxA(win,"Graphics initialization failed. Check the GPU and assets folder.",
        "Mini City 3D",MB_ICONERROR);logging::write("Renderer initialization failed");logging::shutdown();return 1;}
    logging::write("Renderer initialized");
    if(!audio::init())logging::write("XAudio2 initialization failed; continuing without audio");
    ui::load();
    if(fullHd){
        RECT bounds{0,0,1920,1080};
        AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
        SetWindowPos(win,nullptr,0,0,bounds.right-bounds.left,bounds.bottom-bounds.top,
            SWP_NOMOVE|SWP_NOZORDER);
        // Headless captures and benchmarks must render exactly 1920 x 1080
        // even when Windows reports a DPI-adjusted hidden client rectangle.
        screenW=1920;screenH=1080;
    }
    if(!weapons::load()){
        logging::write(weapons::lastError().c_str());
        if(!smoke)MessageBoxA(win,weapons::lastError().c_str(),"Invalid weapon data",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    if(!physics::load()){
        logging::write(physics::lastError().c_str());
        if(!smoke)MessageBoxA(win,physics::lastError().c_str(),"Invalid vehicle data",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    if(!fire::load()){
        logging::write(fire::lastError().c_str());
        if(!smoke)MessageBoxA(win,fire::lastError().c_str(),"Invalid surface data",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    if(!police::load()){
        logging::write(police::lastError().c_str());
        if(!smoke)MessageBoxA(win,police::lastError().c_str(),"Invalid police data",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    if(!commerce::load()){
        logging::write(commerce::lastError().c_str());
        if(!smoke)MessageBoxA(win,commerce::lastError().c_str(),"Invalid shop data",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    if(!traversal::load()){
        logging::write(traversal::lastError().c_str());
        if(!smoke)MessageBoxA(win,traversal::lastError().c_str(),"Invalid traversal data",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    if(!weather::load()){
        logging::write(weather::lastError().c_str());
        if(!smoke)MessageBoxA(win,weather::lastError().c_str(),"Invalid weather data",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    if(!regions::load()){
        logging::write(regions::lastError().c_str());
        if(!smoke)MessageBoxA(win,regions::lastError().c_str(),"Invalid region data",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    reset();
    if(!content::lastError().empty()){
        logging::write(content::lastError().c_str());
        if(!smoke)MessageBoxA(win,content::lastError().c_str(),"Invalid world data",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    if(!traversal::lastError().empty()){
        logging::write(traversal::lastError().c_str());
        if(!smoke)MessageBoxA(win,traversal::lastError().c_str(),"Invalid climb location",MB_ICONERROR);
        audio::shutdown();shutdownRenderer();DestroyWindow(win);logging::shutdown();return 1;
    }
    if(!smoke&&savegame::load()){message="Saved progress loaded. Press F near a marker for a mission.";messageTime=5;
        logging::write("Saved progress loaded");}
    if(smoke&&commandLine&&std::strstr(commandLine,"--day"))gameHour=12;
    if(smoke&&commandLine&&std::strstr(commandLine,"--night"))gameHour=22;
    if(smoke&&commandLine&&std::strstr(commandLine,"--lamp-dim"))worldTime=0.4f;
    if(smoke&&commandLine&&std::strstr(commandLine,"--sunrise"))gameHour=6.5f;
    if(smoke&&commandLine&&std::strstr(commandLine,"--sunset"))gameHour=18.5f;
    if(smoke&&commandLine&&std::strstr(commandLine,"--scope")){
        weapon=weapons::indexOf("sniper");unlocked[weapon]=true;
        magazine[weapon]=weapons::stats(weapon).magazine;
        rightMouse=true;scopeBlend=1;scopeLevel=1;
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--fire")){
        player={100,100};previousPlayer=player;cameraYaw=0;
        for(float x:{170.0f,194.0f,218.0f})fire::ignite({x,100});
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--wanted"))
        police::setWantedLevel(4);
    if(smoke&&commandLine&&std::strstr(commandLine,"--rain"))weather::set("rain");
    if(smoke&&commandLine&&std::strstr(commandLine,"--snow"))weather::set("snow");
    if(smoke&&commandLine&&std::strstr(commandLine,"--east"))
        player=previousPlayer={12000,8500};
    if(smoke&&commandLine&&std::strstr(commandLine,"--marina")){
        player=previousPlayer={8125,9210};cameraYaw=0;
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--bridge"))
        player=previousPlayer={7800,8500};
    if(smoke&&commandLine&&std::strstr(commandLine,"--causeway")){
        player=previousPlayer={1200,1790};cameraYaw=game::PI/2;
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--desert"))
        player=previousPlayer={12000,3500};
    if(smoke&&commandLine&&std::strstr(commandLine,"--desert-hub")){
        player=previousPlayer={12000,3000};cameraYaw=game::PI/2;
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--snowfield"))
        player=previousPlayer={4000,14500};
    if(smoke&&commandLine&&std::strstr(commandLine,"--savanna"))
        player=previousPlayer={12000,14500};
    if(smoke&&commandLine&&std::strstr(commandLine,"--woods")){
        player=previousPlayer={4400,9000};cameraYaw=game::PI;
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--forest-fire")){
        player=previousPlayer={4000,9000};
        for(int index=0;index<20;++index)
            fire::ignite({player.x+float(index%5)*24-48,
                player.z+float(index/5)*24-36},fire::Material::Grass);
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--effects-preview")){
        player=previousPlayer={100,100};cameraYaw=0;
        blasts.push_back({{205,18,113},0.47f,95});
        Bullet preview{};preview.p={143,19,114};preview.v={2800,0,0};
        preview.life=0.2f;preview.range=650;preview.damage=52;
        bullets.push_back(preview);
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--burn-preview")&&
       !peds.empty()&&!vehicles.empty()){
        player=previousPlayer={80,100};cameraYaw=0;
        auto& car=vehicles.front();
        car.p={190,100};car.angle=PI;car.lightsOn=true;car.lightsManual=true;
        fire::igniteVehicle(car);
        auto& ped=peds.front();ped.p={175,140};ped.alive=true;
        fire::ignitePed(ped);
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--aim-up")){
        cameraYaw=0;cameraPitch=0.6f;rightMouse=true;
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--aim-down")){
        cameraYaw=0;cameraPitch=-0.6f;rightMouse=true;
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--weapon-preview")){
        cameraYaw=0;cameraPitch=0;rightMouse=true;
        if(std::strstr(commandLine,"--rifle-preview")){
            weapon=weapons::indexOf("rifle");unlocked[weapon]=true;
            magazine[weapon]=weapons::stats(weapon).magazine;
        }else if(std::strstr(commandLine,"--shotgun-preview")){
            weapon=weapons::indexOf("shotgun");unlocked[weapon]=true;
            magazine[weapon]=weapons::stats(weapon).magazine;
        }
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--combat-preview")&&
       peds.size()>=2){
        player=previousPlayer={300,250};cameraYaw=0;cameraPitch=0;
        cameraMode=CameraMode::ThirdNear;rightMouse=false;
#ifdef MINI_CITY_JOLT
        jolt_world::teleportCharacter(player,0);
#endif
        for(auto& ped:peds){ped.alive=false;ped.respawn=999999;}
        peds[0].alive=true;peds[0].p={355,230};peds[0].style=2;
        peds[0].angle=PI;peds[0].armed=false;peds[0].hitFlash=0.2f;
        peds[1].alive=true;peds[1].p={385,270};peds[1].style=0;
        peds[1].angle=PI;peds[1].armed=true;peds[1].attackVisualTime=0.18f;
        shotVisualTime=0.18f;muzzleFlash=0.12f;
        lastMuzzle={player.x+16,20,player.z+7};
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--casing-preview")){
        player=previousPlayer={300,210};cameraYaw=0;cameraPitch=0;
        cameraMode=CameraMode::ThirdNear;rightMouse=false;
#ifdef MINI_CITY_JOLT
        jolt_world::teleportCharacter(player,0);
#endif
        for(auto& ped:peds){ped.alive=false;ped.respawn=999999;}
        weapon=0;magazine[weapon]=12;
        for(int shot=0;shot<10;++shot){
            fireCooldown=0;shoot();bullets.clear();
            update(1.0f/60.0f);
        }
        for(int tick=0;tick<12;++tick)update(1.0f/60.0f);
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--blood-preview")){
        player=previousPlayer={100,100};cameraYaw=0;
        hitFlashes.push_back({{145,18,105},0.22f,rgb(205,34,29),true});
        impacts.push_back({{145,105},1.8f,true});
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--checkpoint-preview")&&
       !missions.empty()&&!missions[0].goals.empty()){
        activeMission=0;missionStep=0;missionTime=missions[0].seconds;
        player=previousPlayer=missions[0].goals[0]+Vec2{-95,0};cameraYaw=0;
#ifdef MINI_CITY_JOLT
        jolt_world::teleportCharacter(player,0);
#endif
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--map"))showMap=true;
#ifdef MINI_CITY_JOLT
    if(smoke&&commandLine&&std::strstr(commandLine,"--swim")){
        player=previousPlayer={850,SHORE+35};cameraYaw=PI/2;
        jolt_world::teleportCharacter(player,0);
        keys[ui::bindings[int(ui::Action::Forward)]]=true;
        for(int tick=0;tick<20;++tick)update(1.0f/60.0f);
        keys[ui::bindings[int(ui::Action::Forward)]]=false;
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--climb")&&
       !traversal::ladders.empty()){
        player=previousPlayer=traversal::ladders.front().bottom;
        jolt_world::teleportCharacter(player,0);
        if(traversal::startLadder(0)){
            keys[ui::bindings[int(ui::Action::Forward)]]=true;
            for(int tick=0;tick<30;++tick)update(1.0f/60.0f);
            keys[ui::bindings[int(ui::Action::Forward)]]=false;
        }
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--debug-menu"))
        debug_menu::toggle();
#endif
    if(smoke&&commandLine&&std::strstr(commandLine,"--wall-impact")&&
       !buildings.empty()){
        const auto& wall=buildings.front();
        float z=wall.z+wall.d*0.5f;
        player=previousPlayer={wall.x-95,z};cameraYaw=0;
#ifdef MINI_CITY_JOLT
        jolt_world::teleportCharacter(player,0);
#endif
        Bullet shot{};shot.p={wall.x-50,20,z};shot.v={6000,0,0};
        shot.life=1;shot.damage=10;shot.range=600;
        bullets.push_back(shot);
        update(1.0f/60.0f);
        logging::write(hitFlashes.empty()?"Wall impact smoke: no hit flash":
            "Wall impact smoke: hit flash visible");
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--arrow-pin")&&
       !buildings.empty()&&!peds.empty()){
        const auto& wall=buildings.front();
        float z=wall.z+wall.d*0.5f;
        player=previousPlayer={wall.x-95,z};cameraYaw=0;
        for(std::size_t index=0;index<props.size();++index){
            props[index].alive=false;
#ifdef MINI_CITY_JOLT
            jolt_world::remove(index);
#endif
        }
        for(std::size_t index=1;index<peds.size();++index){
            peds[index].alive=false;peds[index].respawn=999999;
        }
        peds[0].p={wall.x-18,z};peds[0].health=40;
        peds[0].armor=0;peds[0].knockedDown=1;
        Bullet arrow{};arrow.p={wall.x-80,20,z};arrow.v={900,0,0};
        arrow.life=1;arrow.damage=100;arrow.range=600;
        arrow.arrow=true;arrow.silent=true;
        bullets.push_back(arrow);
        for(int tick=0;tick<40;++tick)update(1.0f/60.0f);
        logging::write(peds[0].pinned?"Arrow pin smoke: pinned ragdoll":
            "Arrow pin smoke: target not pinned");
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--no-shadows"))ui::shadowQuality=0;
#ifdef MINI_CITY_JOLT
    if(smoke&&commandLine&&std::strstr(commandLine,"--traffic-preview")){
        player=previousPlayer={520,235};cameraYaw=0;cameraPitch=0;
        cameraMode=CameraMode::ThirdNear;
        jolt_world::teleportCharacter(player,0);
        int placed=0;
        for(int i=0;i<int(vehicles.size())&&placed<2;++i){
            auto& car=vehicles[i];
            if(car.driver<0||car.kind!=Kind::Car||car.p.x>2400||car.p.z>1600)continue;
            jolt_world::teleportVehicle(i,placed==0?Vec2{600,273}:Vec2{773,440},
                placed==0?0:-PI/2);
            car.roadFrom=car.roadTo=-1;++placed;
        }
        for(int tick=0;tick<150;++tick)update(1.0f/60.0f);
        previousPlayer=player;
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--ragdoll")){
        player={600,BEACH_START+220};previousPlayer=player;
        Ped fallen{};fallen.p={player.x+50,player.z};fallen.style=0;
        jolt_world::spawnRagdoll(fallen,{1100,0,180});
        for(int tick=0;tick<40;++tick)jolt_world::step(1.0f/60.0f);
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--fall")){
        for(int tick=0;tick<5;++tick)update(1.0f/60.0f);
        keys[VK_SPACE]=true;update(1.0f/60.0f);keys[VK_SPACE]=false;
        for(int tick=0;tick<12;++tick)update(1.0f/60.0f);
    }
    if(smoke&&commandLine&&std::strstr(commandLine,"--entry")){
        int car=-1;
        for(int index=0;index<int(vehicles.size());++index)
            if(vehicles[index].id=="starter-car")car=index;
        if(car<0)car=0;
        player=vehicles[car].p+Vec2{-50,0};previousPlayer=player;
        cameraYaw=vehicles[car].angle;
        enterExit();
        for(int tick=0;tick<20;++tick)update(1.0f/60.0f);
    }
#endif
    if(smoke&&commandLine&&std::strstr(commandLine,"--graphics-menu"))
        ui::page=ui::Page::Graphics;
    if(smoke&&commandLine&&std::strstr(commandLine,"--crouch-preview"))
        crouched=true;
    if(smoke&&commandLine&&std::strstr(commandLine,"--screenshot"))
        input::windowProc(win,WM_KEYDOWN,VK_F11,0);
    if(smoke){
        if(benchmark){
            constexpr int frames=120;
            LARGE_INTEGER frequency{},start{},middle{},end{};
            QueryPerformanceFrequency(&frequency);
            render();
            std::array<double,frames> frameTimes{};
            const std::array<Vec2,6> travelStops{{{12000,8500},{7800,8500},
                {5000,6000},{12000,3000},{12000,14000},{4000,14000}}};
            double simulationTotal=0,renderTotal=0,physicsTotal=0;
            double drawTotal=0,aiTotal=0;
            for(int frame=0;frame<frames;++frame){
                QueryPerformanceCounter(&start);
                if(benchmarkTravel&&frame%20==0){
                    player=travelStops[frame/20];playerY=0;occupied=-1;
#ifdef MINI_CITY_JOLT
                    jolt_world::teleportCharacter(player,0);
#endif
                }
                update(1.0f/60.0f);
                physicsTotal+=physicsMs;
                QueryPerformanceCounter(&middle);
                render();
                drawTotal+=drawCalls;
                aiTotal+=activeAi;
                QueryPerformanceCounter(&end);
                frameTimes[frame]=1000.0*(end.QuadPart-start.QuadPart)/
                    double(frequency.QuadPart);
                simulationTotal+=1000.0*(middle.QuadPart-start.QuadPart)/
                    double(frequency.QuadPart);
                renderTotal+=1000.0*(end.QuadPart-middle.QuadPart)/
                    double(frequency.QuadPart);
            }
            double total=0;
            for(double duration:frameTimes)total+=duration;
            std::sort(frameTimes.begin(),frameTimes.end());
            double p95=frameTimes[int(std::ceil(frames*0.95))-1];
            double p99=frameTimes[int(std::ceil(frames*0.99))-1];
            PROCESS_MEMORY_COUNTERS memory{};
            memory.cb=sizeof(memory);
            GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory));
            std::size_t nearTrees=regions::nearbyTreeIndices(player,850).size();
            std::size_t nearDecorations=regions::nearbyDecorationIndices(player,550).size();
            char result[384]{};
            std::snprintf(result,sizeof(result),
                "Benchmark%s %dx%d: avg %.2f ms (sim %.2f, physics %.2f, render %.2f), p95 %.2f ms, p99 %.2f ms, max %.2f ms, draws %.0f, active AI %.0f over %d frames, %zu flames, RAM %.0f MiB, nearby trees %zu/%zu, decorations %zu/%zu",
                benchmarkTravel?" travel":"",screenW,screenH,total/frames,
                simulationTotal/frames,physicsTotal/frames,renderTotal/frames,
                p95,p99,frameTimes.back(),
                drawTotal/frames,aiTotal/frames,frames,fire::active().size(),
                memory.WorkingSetSize/1048576.0,nearTrees,trees.size(),
                nearDecorations,regions::decorations().size());
            logging::write(result);
        }else render();
        audio::shutdown();shutdownRenderer();DestroyWindow(win);
        logging::write("Smoke render completed");logging::shutdown();return 0;}
    ShowWindow(win,show);
    input::syncLookCapture();
    LARGE_INTEGER frequency{},last{},now{};QueryPerformanceFrequency(&frequency);QueryPerformanceCounter(&last);
    bool running=true;double accumulator=0,fpsTimer=0;int frames=0,overloadEvents=0;
    constexpr double fixedStep=1.0/60.0;
    while(running){MSG msg{};while(PeekMessageA(&msg,nullptr,0,0,PM_REMOVE)){
        if(msg.message==WM_QUIT){running=false;break;}TranslateMessage(&msg);DispatchMessageA(&msg);}
        if(!running)break;
        input::syncLookCapture();
        QueryPerformanceCounter(&now);
        double dt=std::clamp(double(now.QuadPart-last.QuadPart)/double(frequency.QuadPart),0.0,0.25);last=now;
        frameMs=frameMs*0.9f+float(dt*1000.0)*0.1f;
        if(!ui::paused()
#ifdef MINI_CITY_JOLT
            &&!debug_menu::open
#endif
            )accumulator+=dt;
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

