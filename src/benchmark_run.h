#pragma once

#include "fire.h"
#include "game.h"
#include "logging.h"
#include "profiling.h"
#include "regions.h"
#include "ui.h"
#ifdef MINI_CITY_JOLT
#include "dx11_assets.h"
#include "jolt_world.h"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <locale>
#include <psapi.h>

namespace benchmark {
inline bool Run(const char* commandLine,bool benchmarkRoute,bool benchmarkTravel,
                int benchmarkFrames,int benchmarkWarmup){
    using namespace game;
    const int frames=benchmarkFrames?benchmarkFrames:benchmarkRoute?3600:std::strstr(commandLine,"--benchmark-probe")?3:std::strstr(commandLine,"--benchmark-short")?20:120;
    const int warmup=benchmarkWarmup>=0?benchmarkWarmup:benchmarkRoute?120:1;
    std::ofstream csv;
    if(std::strstr(commandLine,"--benchmark-csv")){
        csv.open("benchmark-frames.csv",std::ios::trunc);
        csv.imbue(std::locale::classic());csv<<std::fixed<<std::setprecision(6);
        if(!csv){logging::write("Could not create benchmark-frames.csv");return false;}
        csv<<"frame,frame_ms,simulation_ms,physics_ms,render_ms,scene_cpu_ms,upload_cpu_ms,draw_cpu_ms,present_cpu_ms,gpu_shadow_ms,gpu_scene_ms,gpu_post_ms,draws,triangles,active_ai,working_set_mib,private_mib\n";
    }
    LARGE_INTEGER frequency{},start{},middle{},end{};
    QueryPerformanceFrequency(&frequency);
    std::vector<double> frameTimes(frames);
    const std::array<Vec2,6> travelStops{{{12000,8500},{7800,8500},
        {5000,6000},{12000,3000},{12000,14000},{4000,14000}}};
    const std::array<Vec2,6> routeStops{{{300,250},{8125,9210},
        {1200,1790},{4400,9000},{12000,3000},{4000,14500}}};
    const std::array<float,6> routeYaw{{0,0,PI/2,PI,PI/2,0}};
    if(benchmarkRoute){
        player=previousPlayer=routeStops[0];playerY=0;occupied=-1;
        cameraYaw=routeYaw[0];cameraPitch=0;
#ifdef MINI_CITY_JOLT
        jolt_world::teleportCharacter(player,0);
#endif
    }
    for(int tick=0;tick<warmup;++tick){update(1.0f/60.0f);render();}
    char metadata[200];std::snprintf(metadata,sizeof(metadata),"Benchmark configuration: %d measured frames, %d warm-up ticks, seed 1, fixed 60 Hz, VSync off",frames,warmup);
    logging::write(metadata);
    double simulationTotal=0,renderTotal=0,physicsTotal=0;
    double drawTotal=0,aiTotal=0,triangleTotal=0;
    double gpuShadowTotal=0,gpuSceneTotal=0,gpuPostTotal=0;
    double sceneCpuTotal=0,uploadCpuTotal=0,drawCpuTotal=0,presentCpuTotal=0;
    double skinCpuTotal=0,grassCpuTotal=0,skinVerticesTotal=0,sceneJobsTotal=0;
    int gpuSamples=0;
    for(int frame=0;frame<frames;++frame){
        QueryPerformanceCounter(&start);
        if(benchmarkRoute){
            int segment=int(6ll*frame/frames);
            if(frame==0||segment!=int(6ll*(frame-1)/frames)){
                player=previousPlayer=routeStops[segment];playerY=0;occupied=-1;
#ifdef MINI_CITY_JOLT
                jolt_world::teleportCharacter(player,0);
#endif
            }
            float progress=float(6ll*frame%frames)/frames;
            cameraYaw=routeYaw[segment]+0.22f*std::sin(progress*2*PI);
            keys[ui::bindings[int(ui::Action::Forward)]]=true;
        }
        if(benchmarkTravel&&frame%20==0){
            player=travelStops[(frame/20)%travelStops.size()];playerY=0;occupied=-1;
#ifdef MINI_CITY_JOLT
            jolt_world::teleportCharacter(player,0);
#endif
        }
        update(1.0f/60.0f);
        physicsTotal+=physicsMs;
        QueryPerformanceCounter(&middle);
        render();
        sceneCpuTotal+=renderSceneMs;uploadCpuTotal+=renderUploadMs;
        drawCpuTotal+=renderDrawMs;presentCpuTotal+=renderPresentMs;
#ifdef MINI_CITY_JOLT
        const auto& sceneWork=dx11::sceneWorkStats();
        skinCpuTotal+=sceneWork.skinMs;grassCpuTotal+=sceneWork.grassMs;
        skinVerticesTotal+=sceneWork.skinVertices;sceneJobsTotal+=sceneWork.jobBatches;
#endif
        drawTotal+=drawCalls;
        triangleTotal+=double(triangleCount);
        aiTotal+=activeAi;
        if(gpuShadowMs>=0){
            gpuShadowTotal+=gpuShadowMs;gpuSceneTotal+=gpuSceneMs;
            gpuPostTotal+=gpuPostMs;++gpuSamples;
        }
        QueryPerformanceCounter(&end);
        frameTimes[frame]=1000.0*(end.QuadPart-start.QuadPart)/
            double(frequency.QuadPart);
        const double simMs=1000.0*(middle.QuadPart-start.QuadPart)/double(frequency.QuadPart);
        const double renderMs=1000.0*(end.QuadPart-middle.QuadPart)/double(frequency.QuadPart);
        CITY_PLOT("Frame milliseconds",frameTimes[frame]);
        CITY_PLOT("Active AI",double(activeAi));
        CITY_PLOT("Submitted triangles",double(triangleCount));
        if(csv.is_open()){
            PROCESS_MEMORY_COUNTERS_EX sample{};sample.cb=sizeof(sample);
            GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&sample),sizeof(sample));
            csv<<frame<<','<<frameTimes[frame]<<','<<simMs<<','<<physicsMs<<','<<renderMs<<','
               <<renderSceneMs<<','<<renderUploadMs<<','<<renderDrawMs<<','<<renderPresentMs<<','
               <<gpuShadowMs<<','<<gpuSceneMs<<','<<gpuPostMs<<','<<drawCalls<<','<<triangleCount<<','<<activeAi<<','
               <<sample.WorkingSetSize/1048576.0<<','<<sample.PrivateUsage/1048576.0<<'\n';
            CITY_PLOT("Working set MiB",sample.WorkingSetSize/1048576.0);
            CITY_PLOT("Private memory MiB",sample.PrivateUsage/1048576.0);
        }
        simulationTotal+=1000.0*(middle.QuadPart-start.QuadPart)/
            double(frequency.QuadPart);
        renderTotal+=1000.0*(end.QuadPart-middle.QuadPart)/
            double(frequency.QuadPart);
    }
    if(csv.is_open()){csv.flush();if(!csv){logging::write("Benchmark CSV write failed");return false;}}
    if(benchmarkRoute)keys[ui::bindings[int(ui::Action::Forward)]]=false;
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
        benchmarkRoute?" route v1":benchmarkTravel?" travel":"",
        screenW,screenH,total/frames,
        simulationTotal/frames,physicsTotal/frames,renderTotal/frames,
        p95,p99,frameTimes.back(),
        drawTotal/frames,aiTotal/frames,frames,fire::active().size(),
        memory.WorkingSetSize/1048576.0,nearTrees,trees.size(),
        nearDecorations,regions::decorations().size());
    logging::write(result);
    char gpuResult[240]{};
    if(gpuSamples)
        std::snprintf(gpuResult,sizeof(gpuResult),
            "GPU timing: shadow %.2f ms, scene %.2f ms, post/HUD %.2f ms (%d nonblocking samples); submitted triangles %.0f/frame",
            gpuShadowTotal/gpuSamples,gpuSceneTotal/gpuSamples,
            gpuPostTotal/gpuSamples,gpuSamples,triangleTotal/frames);
    else std::snprintf(gpuResult,sizeof(gpuResult),
        "GPU timing unavailable; submitted triangles %.0f/frame",triangleTotal/frames);
    logging::write(gpuResult);
    std::snprintf(gpuResult,sizeof(gpuResult),
        "CPU scene work: deformation %.3f ms (%0.f vertices/frame), grass rebuild %.3f ms, %.2f parallel batches/frame",
        skinCpuTotal/frames,skinVerticesTotal/frames,grassCpuTotal/frames,sceneJobsTotal/frames);
    logging::write(gpuResult);
    std::snprintf(gpuResult,sizeof(gpuResult),
        "CPU rendering: scene %.3f ms, upload/cull %.3f ms, draw/HUD %.3f ms, present %.3f ms (seed 1)",
        sceneCpuTotal/frames,uploadCpuTotal/frames,drawCpuTotal/frames,presentCpuTotal/frames);
    logging::write(gpuResult);
    return true;
}
} // namespace benchmark
