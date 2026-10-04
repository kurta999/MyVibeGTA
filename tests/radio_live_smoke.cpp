#include "../src/radio.h"
#include <windows.h>
#include <objbase.h>
#include <cassert>
#include <cstdio>
#include <string>
#ifdef NDEBUG
#undef NDEBUG
#endif
int main(int argc,char** argv){
    std::setvbuf(stdout,nullptr,_IONBF,0);
    if(FAILED(CoInitializeEx(nullptr,COINIT_MULTITHREADED)))return 2;
    if(!radio::load()){std::puts(radio::lastError().c_str());return 2;}
    int failures=0;
    if(argc>1&&std::string(argv[1])=="--live")for(const auto& station:radio::stations()){
        radio::select(station.id);radio::update(true,false,0);
        auto begin=GetTickCount64();bool ready=false;
        while(GetTickCount64()-begin<10000){
            auto state=radio::status();
            if(state.find("connecting")==std::string::npos){ready=state.find("unavailable")==std::string::npos;break;}
            Sleep(25);
        }
        std::printf("%s: %s\n",station.id.c_str(),ready?"decoded and playing":"FAILED");
        failures+=!ready;radio::update(false,false,0);
    }
    radio::shutdown();CoUninitialize();return failures?1:0;
}
