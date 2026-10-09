#pragma once
#include "../src/builder.h"
#include "../src/excavation.h"
#include <chrono>
#include <thread>

// Older actor/terrain fixtures describe a 40-unit obstacle. Build the same
// volume from the new 20-unit blocks, retaining their original world geometry.
inline bool placeEditVolume(builder::Cell c,int item,bool consume=false){
    for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z)
        if(!builder::place({c.x*2+x,c.y*2+y,c.z*2+z},item,consume))return false;
    return true;
}
inline bool mineEditVolume(builder::Cell c){
    for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z)
        if(!builder::mineBlock({c.x*2+x,c.y*2+y,c.z*2+z}))return false;
    return true;
}
inline bool canPlaceEditVolume(builder::Cell c,int item){
    for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z)
        if(!builder::canPlace({c.x*2+x,c.y*2+y,c.z*2+z},item))return false;
    return true;
}

// Test fixtures advance simulation time quickly, but saves now finish on a
// real worker. Give that worker time without weakening the production gate.
inline bool finishBuilderTransition(bool throughGameUpdate=false){
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    float previousProgress=builder::transitionProgress();
    while(builder::transitioning()&&std::chrono::steady_clock::now()<deadline){
        if(throughGameUpdate)game::update(1.0f/60);else builder::advance(1.0f/60);
        // Bound a stalled stage, rather than abandoning the remaining stages
        // just because successful collision reconstruction used real time.
        float nextProgress=builder::transitionProgress();
        if(nextProgress!=previousProgress){previousProgress=nextProgress;deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);}
        if(builder::transitioning())std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return !builder::transitioning();
}

inline bool mineTerrainVolume(builder::Cell c){
    for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z)
        if(!builder::mineTerrain({c.x*2+x,c.y*2+y,c.z*2+z}))return false;
    return true;
}
inline bool cutTerrainVolume(builder::Cell c){
    for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z)
        if(!excavation::cut({c.x*2+x,c.y*2+y,c.z*2+z}))return false;
    return true;
}
inline bool removedTerrainVolume(builder::Cell c){
    for(int x=0;x<2;++x)for(int y=0;y<2;++y)for(int z=0;z<2;++z)
        if(!excavation::removed({c.x*2+x,c.y*2+y,c.z*2+z}))return false;
    return true;
}
