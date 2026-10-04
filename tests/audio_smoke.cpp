#include "../src/audio.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <set>

int main(){
    for(int kind:{0,1,2,3,4,7,8,9,10,12})assert(audio::recordedEngineSamples(kind)>48000);
    for(int kind:{5,6,11})assert(audio::recordedEngineSamples(kind)==0);
    std::set<unsigned long long> fingerprints;
    for(int n=0;n<21;++n)assert(fingerprints.insert(audio::effectFingerprint(audio::Effect::Shot,n)).second);
    // CI hosts may have no output device; CPU save jobs have a separate suite.
    if(!audio::init()){std::puts("No audio device; live audio verification skipped");return 0;}
    audio::setVolume(0);
    for(int cycle=0;cycle<2;++cycle){
        audio::setListener(300,250,1.2f);
        for(int round=0;round<8;++round)for(int effect=0;effect<int(audio::Effect::Count);++effect)
            for(int variant:{-1,0,1,2,3,4,5,99}){
                audio::play(audio::Effect(effect),variant);
                audio::playAt(audio::Effect(effect),400,275,variant);
            }
        audio::play(audio::Effect(-1));audio::play(audio::Effect::Count);
        for(int kind=0;kind<13;++kind)for(int n=0;n<120;++n)
            audio::updateEngine(kind,float(n)/120,n%2,1.0f/60);
        audio::stopEngine();audio::stopEngine();
        audio::shutdown();assert(audio::init());audio::setVolume(0);
    }
    audio::shutdown();audio::shutdown();
    std::puts("Live XAudio2 cached effects, overlap/voice stealing, variants and reopening passed");
}
