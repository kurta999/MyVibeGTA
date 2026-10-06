#pragma once
#include "../src/builder.h"
#include <chrono>
#include <thread>

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
