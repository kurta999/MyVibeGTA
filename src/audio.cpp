#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <xaudio2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <fstream>
#include <filesystem>
#include <cstring>
#include "audio.h"

namespace audio {
namespace {
constexpr int RATE=48000;
constexpr int VARIANTS=24;
constexpr float PI=3.14159265359f;
struct Channel { IXAudio2SourceVoice* voice=nullptr; std::vector<short> samples; };
IXAudio2* engine=nullptr;
IXAudio2MasteringVoice* master=nullptr;
bool comInitialized=false;
std::array<Channel,16> channels;
#ifdef MINI_CITY_JOLT
// Immutable PCM lives until all voices have been destroyed. Noise-based sounds
// rotate through four prepared takes rather than generating samples on play.
constexpr int EFFECTS=int(Effect::Count);
std::array<Channel,EFFECTS*VARIANTS*4> prepared;
std::array<unsigned,EFFECTS*VARIANTS> takeCursor{};
int normalizedVariant(Effect effect,int variant){
    if(effect==Effect::Engine)return std::clamp(variant,0,5);
    if(effect==Effect::Shot||effect==Effect::SilencedShot)return std::clamp(variant,0,VARIANTS-1);
    if(effect==Effect::Step)return variant==1?1:0;
    return 0;
}
int takes(Effect effect){
    return effect==Effect::Shot||effect==Effect::SilencedShot||effect==Effect::Step||
        effect==Effect::Splash||effect==Effect::Hit||effect==Effect::Reload||
        effect==Effect::Surf||effect==Effect::Skid||effect==Effect::Explosion?4:1;
}
#endif
int voiceCursor=0;
Channel motor;
std::array<std::vector<short>,13> motorClips;
int motorKind=-1;
float motorPitch=1,motorGain=0;
void prepareMotors(){
    const char* names[]={"car","sport-car","motorcycle","boat","helicopter",nullptr,nullptr,
        "tractor","combine","tank","truck",nullptr,"airplane"};
    wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
    auto folder=std::filesystem::path(executable).parent_path()/L"assets"/L"audio"/L"vehicles";
    for(int kind=0;kind<13;++kind){auto& pcm=motorClips[kind];pcm.clear();if(!names[kind])continue;
        std::ifstream file(folder/(std::string(names[kind])+".wav"),std::ios::binary);
        char header[12]{};if(!file.read(header,12)||std::memcmp(header,"RIFF",4)||std::memcmp(header+8,"WAVE",4))continue;
        bool valid=false;
        while(file){char chunk[4];std::uint32_t bytes=0;
            if(!file.read(chunk,4)||!file.read(reinterpret_cast<char*>(&bytes),4)||bytes>16*1024*1024)break;
            if(!std::memcmp(chunk,"fmt ",4)){
                unsigned char format[16]{};if(bytes<16||!file.read(reinterpret_cast<char*>(format),16))break;
                std::uint16_t type,channels,bits;std::uint32_t rate;
                std::memcpy(&type,format,2);std::memcpy(&channels,format+2,2);
                std::memcpy(&rate,format+4,4);std::memcpy(&bits,format+14,2);
                valid=type==1&&channels==1&&rate==RATE&&bits==16;file.seekg(bytes-16,std::ios::cur);
            }else if(!std::memcmp(chunk,"data",4)&&valid){
                if(bytes<2||bytes%2)break;pcm.resize(bytes/2);
                if(!file.read(reinterpret_cast<char*>(pcm.data()),bytes))pcm.clear();break;
            }else file.seekg(bytes,std::ios::cur);
            if(bytes%2)file.seekg(1,std::ios::cur);
        }
    }
}
float listenerX=0,listenerZ=0,listenerYaw=0;
uint32_t noiseState=0x4a3b2c1du;
float noise(){noiseState^=noiseState<<13;noiseState^=noiseState>>17;noiseState^=noiseState<<5;
    return float(int(noiseState&65535)-32768)/32768.0f;}
float wave(float t,float frequency){return std::sin(2*PI*frequency*t);}

void synthesize(Channel& channel,Effect effect,int variant){
    float duration=0.25f;
    switch(effect){
    case Effect::Shot:duration=variant==20?.95f:variant==2?.47f:variant==4?.58f:.16f+float(variant%7)*.045f;break;
    case Effect::SilencedShot:duration=0.12f;break;
    case Effect::Pickup:duration=0.35f;break;
    case Effect::Success:duration=0.75f;break;
    case Effect::Step:duration=0.12f;break;
    case Effect::Engine:duration=0.40f;break;
    case Effect::Splash:duration=0.44f;break;
    case Effect::Hit:duration=0.22f;break;
    case Effect::Fail:duration=0.5f;break;
    case Effect::Reload:duration=0.35f;break;
    case Effect::Surf:duration=2.2f;break;
    case Effect::Skid:duration=0.34f;break;
    case Effect::Traffic:duration=0.85f;break;
    case Effect::Explosion:duration=1.6f;break;
    case Effect::Bow:duration=.22f;break;
    case Effect::Flame:duration=.18f;break;
    case Effect::Water:duration=.18f;break;
    case Effect::Electric:duration=.2f;break;
    default:break;
    }
    channel.samples.resize(int(duration*RATE));
    float rumble=0;
    for(int i=0;i<int(channel.samples.size());++i){
        float t=float(i)/RATE,u=t/duration,signal=0;
        switch(effect){
        case Effect::Shot:{
            float base=variant==20?45.0f:variant==4?90.0f:variant==2?105.0f:110.0f+float(variant)*27;
            float envelope=std::exp(-u*(variant==2?6.0f:9.0f));
            float n=noise();rumble+=.13f*(n-rumble);
            float crack=n*(.25f+float(variant%5)*.045f)*std::exp(-t*(35+variant*3));
            signal=(crack+rumble*.5f+wave(t,base*(1-u*.55f))*.4f+
                wave(t,base*2.37f)*.09f*std::exp(-t*28))*envelope;break;
        }
        case Effect::SilencedShot:signal=(noise()*0.12f+wave(t,120+variant*18)*0.08f)*std::exp(-u*13);break;
        case Effect::Pickup:signal=(wave(t,680)+wave(t,1020)*0.5f)*std::sin(PI*u)*0.32f;break;
        case Effect::Success:{float note=u<0.30f?523:u<0.60f?659:784;
            signal=wave(t,note)*0.3f*std::sin(PI*std::fmod(u*3,1.0f));break;}
        case Effect::Step:{float sand=variant==1?0.65f:1.0f;
            signal=(noise()*0.25f*sand+wave(t,variant==1?48.0f:70.0f)*0.25f)*std::exp(-u*8);break;}
        case Effect::Engine:{float rev=std::clamp(variant,0,5)*24.0f;
            signal=(wave(t,60+rev+35*u)+0.35f*wave(t,120+rev*2+70*u))*0.22f*(1-u);break;}
        case Effect::Splash:signal=noise()*0.34f*std::exp(-u*4);break;
        case Effect::Hit:signal=(noise()*0.35f+wave(t,95)*0.25f)*std::exp(-u*8);break;
        case Effect::Fail:signal=wave(t,320-180*u)*0.3f*(1-u);break;
        case Effect::Reload:signal=(noise()*0.18f+wave(t,380+230*u)*0.09f)*std::exp(-u*7);break;
        case Effect::Surf:signal=noise()*(0.08f+0.09f*std::sin(PI*u))*std::sin(PI*u);break;
        case Effect::Skid:signal=noise()*0.24f*std::exp(-u*2.3f)+wave(t,175-90*u)*0.06f;break;
        case Effect::Traffic:signal=(wave(t,88+15*u)+wave(t,177+21*u)*0.25f)*0.12f*std::sin(PI*u);break;
        case Effect::Explosion:{
            // Sharp crack, low-frequency blast and a longer decaying debris tail.
            float n=noise();rumble+=0.055f*(n-rumble);
            float attack=std::min(1.0f,t*900);
            signal=attack*(n*0.55f*std::exp(-t*32)+
                (rumble*1.6f+wave(t,48-18*u)*0.42f)*std::exp(-t*3.8f)+
                n*0.14f*std::exp(-t*2.8f))*(1-u);break;
        }
        case Effect::Bow:signal=(wave(t,380-180*u)*.28f+noise()*.06f)*std::exp(-u*8);break;
        case Effect::Flame:{float n=noise();rumble+=.08f*(n-rumble);signal=(rumble*.8f+wave(t,58)*.12f)*std::sin(PI*u);break;}
        case Effect::Water:{float n=noise();rumble+=.35f*(n-rumble);signal=rumble*.35f*std::sin(PI*u);break;}
        case Effect::Electric:signal=(wave(t,740)+wave(t,1480)*.3f+noise()*.35f)*.18f*std::exp(-u*5);break;
        default:break;
        }
        channel.samples[i]=short(std::clamp(signal,-1.0f,1.0f)*27000);
    }
}

void submit(Effect effect,int variant,bool positioned,float x,float z){
    if(!engine||!master)return;
    if(int(effect)<0||int(effect)>=int(Effect::Count))return;
    int chosen=-1;
    for(int i=0;i<int(channels.size());++i){
        int index=(voiceCursor+i)%int(channels.size());
        if(!channels[index].voice)continue;
        XAUDIO2_VOICE_STATE state{};
        channels[index].voice->GetState(&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if(state.BuffersQueued==0){chosen=index;break;}
    }
    if(chosen<0){for(int i=0;i<int(channels.size());++i){
        int index=(voiceCursor+i)%int(channels.size());
        if(channels[index].voice){chosen=index;break;}
    }}
    if(chosen<0)return;
    voiceCursor=(chosen+1)%int(channels.size());
    Channel& channel=channels[chosen];
    channel.voice->Stop(0);channel.voice->FlushSourceBuffers();
#ifdef MINI_CITY_JOLT
    int clip=int(effect)*VARIANTS+normalizedVariant(effect,variant);
    const auto& samples=prepared[clip*4+takeCursor[clip]++%takes(effect)].samples;
#else
    synthesize(channel,effect,variant);
    const auto& samples=channel.samples;
#endif
    float gains[2]={0.7071f,0.7071f};
    if(positioned){
        float dx=x-listenerX,dz=z-listenerZ;
        float distance=std::sqrt(dx*dx+dz*dz);
        float pan=distance>0.001f?std::clamp((-std::sin(listenerYaw)*dx+
            std::cos(listenerYaw)*dz)/distance,-1.0f,1.0f):0.0f;
        float volume=1.0f/(1.0f+(distance/280.0f)*(distance/280.0f));
        gains[0]=volume*std::sqrt((1-pan)*0.5f);
        gains[1]=volume*std::sqrt((1+pan)*0.5f);
    }
    channel.voice->SetOutputMatrix(master,1,2,gains);
    XAUDIO2_BUFFER buffer{};
    buffer.AudioBytes=UINT32(samples.size()*sizeof(short));
    buffer.pAudioData=reinterpret_cast<const BYTE*>(samples.data());
    buffer.Flags=XAUDIO2_END_OF_STREAM;
    if(SUCCEEDED(channel.voice->SubmitSourceBuffer(&buffer)))channel.voice->Start(0);
}
}

bool init(){
    if(engine)return true;
    HRESULT comStatus=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if(FAILED(comStatus)&&comStatus!=RPC_E_CHANGED_MODE)return false;
    comInitialized=SUCCEEDED(comStatus);
    if(FAILED(XAudio2Create(&engine,0,XAUDIO2_DEFAULT_PROCESSOR))){shutdown();return false;}
    if(FAILED(engine->CreateMasteringVoice(&master,2,XAUDIO2_DEFAULT_SAMPLERATE))){shutdown();return false;}
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=1;
    format.nSamplesPerSec=RATE;format.wBitsPerSample=16;format.nBlockAlign=2;
    format.nAvgBytesPerSec=RATE*2;
    prepareMotors();engine->CreateSourceVoice(&motor.voice,&format,0,3.0f);
    int ready=0;
    for(auto& channel:channels)if(SUCCEEDED(engine->CreateSourceVoice(&channel.voice,&format)))++ready;
    if(ready==0){shutdown();return false;}
#ifdef MINI_CITY_JOLT
    noiseState=0x4a3b2c1du;takeCursor={};
    for(int effect=0;effect<EFFECTS;++effect)for(int variant=0;variant<VARIANTS;++variant){
        auto kind=Effect(effect);
        if(normalizedVariant(kind,variant)!=variant)continue;
        for(int take=0;take<takes(kind);++take)
            synthesize(prepared[(effect*VARIANTS+variant)*4+take],kind,variant);
    }
#endif
    return true;
}
void setListener(float x,float z,float yaw){listenerX=x;listenerZ=z;listenerYaw=yaw;}
void play(Effect effect,int variant){submit(effect,variant,false,0,0);}
void playAt(Effect effect,float x,float z,int variant){submit(effect,variant,true,x,z);}
void setVolume(int percent){if(master)master->SetVolume(std::clamp(percent,0,100)/100.0f);}
unsigned long long effectFingerprint(Effect effect,int variant){
    if(int(effect)<0||int(effect)>=int(Effect::Count))return 0;
    Channel reference;auto saved=noiseState;noiseState=0x4a3b2c1du;
    synthesize(reference,effect,std::clamp(variant,0,VARIANTS-1));noiseState=saved;
    unsigned long long hash=1469598103934665603ULL;
    for(auto sample:reference.samples){hash^=static_cast<unsigned short>(sample);hash*=1099511628211ULL;}return hash;
}
unsigned recordedEngineSamples(int kind){
    if(kind<0||kind>=13)return 0;
    if(motorClips[0].empty())prepareMotors();return unsigned(motorClips[kind].size());
}
void stopEngine(){
    if(motor.voice){motor.voice->Stop();motor.voice->FlushSourceBuffers();}
    motorKind=-1;motorGain=0;
}
void updateEngine(int kind,float speed,float throttle,float dt){
    if(!motor.voice||dt<=0)return;kind=std::clamp(kind,0,12);speed=std::clamp(speed,0.0f,1.5f);
    if(motorClips[kind].empty()){stopEngine();return;}
    if(kind!=motorKind){stopEngine();motorKind=kind;
        const auto& pcm=motorClips[kind];XAUDIO2_BUFFER buffer{};buffer.pAudioData=reinterpret_cast<const BYTE*>(pcm.data());
        buffer.AudioBytes=UINT32(pcm.size()*2);buffer.LoopCount=XAUDIO2_LOOP_INFINITE;
        if(FAILED(motor.voice->SubmitSourceBuffer(&buffer))){motorKind=-1;return;}
        motor.voice->SetVolume(0);motor.voice->Start();
    }
    float gear=std::min(4.0f,std::floor(speed*4));
    float rpm=kind==4?.85f+throttle*.25f:kind==12?.8f+throttle*1.5f:
        kind==5||kind==6?.7f+speed*1.7f:.85f+std::max(0.0f,speed*4-gear)*1.25f+std::abs(throttle)*.18f;
    motorPitch+=(rpm-motorPitch)*(1-std::exp(-dt*5));
    float target=kind==5||kind==6?speed*.22f:.20f+std::abs(throttle)*.13f+speed*.12f;
    motorGain+=(target-motorGain)*(1-std::exp(-dt*4));
    motor.voice->SetFrequencyRatio(std::clamp(motorPitch,.5f,3.0f));motor.voice->SetVolume(motorGain);
}
void shutdown(){
    stopEngine();if(motor.voice){motor.voice->DestroyVoice();motor.voice=nullptr;}
    for(auto& pcm:motorClips)pcm.clear();
    for(auto& channel:channels)if(channel.voice){channel.voice->Stop(0);channel.voice->FlushSourceBuffers();
        channel.voice->DestroyVoice();channel.voice=nullptr;channel.samples.clear();}
    if(master){master->DestroyVoice();master=nullptr;}
    if(engine){engine->Release();engine=nullptr;}
#ifdef MINI_CITY_JOLT
    for(auto& clip:prepared)clip.samples.clear();
    takeCursor={};
#endif
    if(comInitialized){CoUninitialize();comInitialized=false;}
}
}
