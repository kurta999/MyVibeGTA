#include "radio.h"
#include "data_file.h"
#include <mfapi.h>
#include <mfplay.h>
#include <atomic>
#include <set>
namespace radio {
namespace {
std::vector<Station> catalog;
std::string error;
int selected=-1,playing=-2;
bool enabled=false,started=false;
IMFPMediaPlayer* player=nullptr;
std::atomic<UINT_PTR> generation{0};
std::atomic<int> state{0}; // off, connecting, playing, unavailable
std::atomic<bool> audible{false};
std::atomic<float> gain{.6f};
class Callback final:public IMFPMediaPlayerCallback {
    std::atomic<ULONG> refs{1};
    const UINT_PTR epoch;
public:
    explicit Callback(UINT_PTR value):epoch(value){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{
        if(!out)return E_POINTER;*out=nullptr;
        if(iid==__uuidof(IUnknown)||iid==__uuidof(IMFPMediaPlayerCallback)){*out=this;AddRef();return S_OK;}return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{auto count=--refs;if(!count)delete this;return count;}
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER* e)override{
        if(!audible||epoch!=generation)return;
        if(e->eEventType==MFP_EVENT_TYPE_MEDIAITEM_CREATED){
            auto item=reinterpret_cast<MFP_MEDIAITEM_CREATED_EVENT*>(e);
            if(item->dwUserData!=generation)return;
            if(FAILED(e->hrEvent)||!item->pMediaItem){state=3;return;}
            if(FAILED(e->pMediaPlayer->SetMediaItem(item->pMediaItem)))state=3;
        }else if(e->eEventType==MFP_EVENT_TYPE_MEDIAITEM_SET){
            auto item=reinterpret_cast<MFP_MEDIAITEM_SET_EVENT*>(e);UINT_PTR token=0;
            if(!item->pMediaItem||FAILED(item->pMediaItem->GetUserData(&token))||token!=generation)return;
            if(FAILED(e->hrEvent)||FAILED(e->pMediaPlayer->SetVolume(gain))||FAILED(e->pMediaPlayer->Play()))state=3;
        }else if(e->eEventType==MFP_EVENT_TYPE_PLAY)state=SUCCEEDED(e->hrEvent)?2:3;
        else if(e->eEventType==MFP_EVENT_TYPE_ERROR||e->eEventType==MFP_EVENT_TYPE_PLAYBACK_ENDED)state=3;
    }
};
std::wstring wide(const std::string& text){int n=MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,nullptr,0);
    if(n<=1)return {};std::wstring result(n,0);MultiByteToWideChar(CP_UTF8,0,text.c_str(),-1,result.data(),n);result.pop_back();return result;}
bool start(){
    if(player)return true;
    if(!started){if(FAILED(MFStartup(MF_VERSION,MFSTARTUP_FULL)))return false;started=true;}
    auto callback=new Callback(generation.load());
    HRESULT hr=MFPCreateMediaPlayer(nullptr,FALSE,MFP_OPTION_FREE_THREADED_CALLBACK,callback,nullptr,&player);callback->Release();
    return SUCCEEDED(hr);
}
}
bool load(const char* path){
    data_file::Ini file;int count=0;
    if(!file.load(path?path:data_file::resourcePath("radio.ini"))||!file.version(1)||
        !file.integer("Radio","Count",count,0,64)){error=file.lastError();return false;}
    std::vector<Station> parsed;std::set<std::string> ids;
    for(int n=0;n<count;++n){Station station;std::string section="Station"+std::to_string(n);
        if(!file.string(section,"Id",station.id)||!file.string(section,"Name",station.name)||!file.string(section,"URL",station.url)){
            error=file.lastError();return false;}
        if(!data_file::validId(station.id)||!ids.insert(station.id).second||station.name.size()>100||station.url.size()>2048||
            (station.url.rfind("https://",0)!=0&&station.url.rfind("http://",0)!=0)){
            error="Invalid radio station "+section;return false;}
        parsed.push_back(std::move(station));
    }
    auto old=selectedId();catalog=std::move(parsed);select(old);error.clear();return true;
}
const std::vector<Station>& stations(){return catalog;}
const std::string& lastError(){return error;}
void select(const std::string& id){selected=-1;for(int n=0;n<int(catalog.size());++n)if(catalog[n].id==id){selected=n;break;}}
std::string selectedId(){return selected>=0&&selected<int(catalog.size())?catalog[selected].id:"off";}
void cycle(int direction){
    int count=int(catalog.size())+1;
    selected=((selected+1+(direction>0?1:-1)+count)%count)-1;
}
void update(bool driving,bool paused,int volume){
    gain=std::clamp(volume,0,100)/100.0f*.65f;
    bool desired=driving&&!paused&&selected>=0;
    audible=desired;
    if(!desired){
        if(player&&enabled){++generation;player->Stop();player->ClearMediaItem();}
        enabled=false;playing=-2;state=0;return;
    }
    if(enabled&&playing==selected){if(player)player->SetVolume(gain);return;}
    enabled=true;playing=selected;auto token=++generation;state=1;
    // A fresh player isolates asynchronous network/decoder errors from the
    // station being left. Every callback also carries this selection's epoch.
    if(player){player->Shutdown();player->Release();player=nullptr;}
    if(!start()){state=3;return;}
    player->SetVolume(gain);
    auto url=wide(catalog[selected].url);
    // Resolution, network access and decode run asynchronously in Media Foundation.
    if(FAILED(player->CreateMediaItemFromURL(url.c_str(),FALSE,token,nullptr)))state=3;
}
std::string status(){
    if(selected<0)return "RADIO OFF  |  MOUSE WHEEL";
    static const char* suffix[]={""," (connecting)",""," (unavailable; scroll to retry)"};
    return catalog[selected].name+suffix[std::clamp(state.load(),0,3)];
}
void shutdown(){audible=false;++generation;
    if(player){player->Shutdown();player->Release();player=nullptr;}
    if(started){MFShutdown();started=false;}enabled=false;playing=-2;state=0;
}
}
