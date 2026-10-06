#include "../src/audio.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdio>
#include <initializer_list>
#include <set>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cmath>

int main(int argc,char** argv){
    for(int kind:{0,1,2,3,4,7,8,9,10,12})assert(audio::recordedEngineSamples(kind)>48000);
    for(int kind:{5,6,11})assert(audio::recordedEngineSamples(kind)==0);
    std::set<unsigned long long> fingerprints;
    for(int n=0;n<21;++n)assert(fingerprints.insert(audio::effectFingerprint(audio::Effect::Shot,n)).second);
    std::set<unsigned long long> mining;
    for(int n=0;n<8;++n)assert(mining.insert(audio::effectFingerprint(audio::Effect::BuilderContact,n)).second);
    assert(audio::effectFingerprint(audio::Effect::BuilderContact,-1)==audio::effectFingerprint(audio::Effect::BuilderContact,0));
    assert(audio::effectFingerprint(audio::Effect::BuilderContact,99)==audio::effectFingerprint(audio::Effect::BuilderContact,7));
    if(argc==3&&std::string(argv[1])=="--builder-evidence"){
        std::filesystem::path folder(argv[2]);std::filesystem::create_directories(folder);
        const char* names[]{"stone","wood","soil","sand","snow","foliage","metal","brush"};
        for(int n=0;n<8;++n){auto path=folder/(std::string(names[n])+".wav");assert(audio::exportEffectWav(audio::Effect::BuilderContact,n,path.c_str()));
            std::ifstream file(path,std::ios::binary);file.seekg(0,std::ios::end);auto bytes=std::size_t(file.tellg());assert(bytes>44+4800*2);file.seekg(44);
            std::vector<short> samples((bytes-44)/2);assert(file.read(reinterpret_cast<char*>(samples.data()),samples.size()*2));double square=0;int peak=0;
            for(auto sample:samples){square+=double(sample)*sample;peak=std::max(peak,std::abs(int(sample)));}assert(std::sqrt(square/samples.size())>100&&peak<32767);
        }std::puts("Eight distinct nonsilent, unclipped builder contact WAVs exported from runtime synthesis");
    }
    // CI hosts may have no output device; CPU save jobs have a separate suite.
    if(!audio::init()){std::puts("No audio device; live audio verification skipped");return 0;}
    audio::setVolume(0);
    for(int cycle=0;cycle<2;++cycle){
        audio::setListener(300,250,1.2f);
        for(int round=0;round<8;++round)for(int effect=0;effect<int(audio::Effect::Count);++effect)
            for(int variant:{-1,0,1,2,3,4,5,6,7,99}){
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
