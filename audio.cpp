#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xaudio2.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "audio.h"

namespace audio {
namespace {
constexpr int RATE=22050;
constexpr float PI=3.14159265359f;
struct Channel { IXAudio2SourceVoice* voice=nullptr; std::vector<short> samples; };
IXAudio2* engine=nullptr;
IXAudio2MasteringVoice* master=nullptr;
bool comInitialized=false;
std::array<Channel,16> channels;
int voiceCursor=0;
float listenerX=0,listenerZ=0,listenerYaw=0;
uint32_t noiseState=0x4a3b2c1du;
float noise(){noiseState^=noiseState<<13;noiseState^=noiseState>>17;noiseState^=noiseState<<5;
    return float(int(noiseState&65535)-32768)/32768.0f;}
float wave(float t,float frequency){return std::sin(2*PI*frequency*t);}

void synthesize(Channel& channel,Effect effect,int variant){
    float duration=0.25f;
    switch(effect){
    case Effect::Shot:duration=variant==2?0.47f:variant==4?0.58f:0.25f;break;
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
    default:break;
    }
    channel.samples.resize(int(duration*RATE));
    for(int i=0;i<int(channel.samples.size());++i){
        float t=float(i)/RATE,u=t/duration,signal=0;
        switch(effect){
        case Effect::Shot:{
            float base=variant==4?90.0f:variant==2?105.0f:170.0f;
            float envelope=std::exp(-u*(variant==2?6.0f:9.0f));
            signal=(0.55f*noise()+0.45f*wave(t,base*(1.0f-u*0.55f)))*envelope;break;
        }
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
        default:break;
        }
        channel.samples[i]=short(std::clamp(signal,-1.0f,1.0f)*27000);
    }
}

void submit(Effect effect,int variant,bool positioned,float x,float z){
    if(!engine||!master)return;
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
    synthesize(channel,effect,variant);
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
    buffer.AudioBytes=UINT32(channel.samples.size()*sizeof(short));
    buffer.pAudioData=reinterpret_cast<const BYTE*>(channel.samples.data());
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
    int ready=0;
    for(auto& channel:channels)if(SUCCEEDED(engine->CreateSourceVoice(&channel.voice,&format)))++ready;
    if(ready==0){shutdown();return false;}
    return true;
}
void setListener(float x,float z,float yaw){listenerX=x;listenerZ=z;listenerYaw=yaw;}
void play(Effect effect,int variant){submit(effect,variant,false,0,0);}
void playAt(Effect effect,float x,float z,int variant){submit(effect,variant,true,x,z);}
void setVolume(int percent){if(master)master->SetVolume(std::clamp(percent,0,100)/100.0f);}
void shutdown(){
    for(auto& channel:channels)if(channel.voice){channel.voice->Stop(0);channel.voice->FlushSourceBuffers();
        channel.voice->DestroyVoice();channel.voice=nullptr;channel.samples.clear();}
    if(master){master->DestroyVoice();master=nullptr;}
    if(engine){engine->Release();engine=nullptr;}
    if(comInitialized){CoUninitialize();comInitialized=false;}
}
}
