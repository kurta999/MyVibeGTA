#pragma once
#include "dx12_backend.h"
#include <ffx_fsr2.h>
namespace dx12 {
inline constexpr const char* fsr2Modes[]={"Off (native)","Quality","Balanced","Performance","Ultra Performance"};
struct Fsr2 {
    FfxFsr2Context context{};
    std::vector<unsigned char> scratch;
    bool active=false;
    int mode=0,width=0,height=0,renderWidth=0,renderHeight=0;
    float jitterX=0,jitterY=0;
    Resource* output=nullptr;View* outputView=nullptr;
    void configure(Device&,int,int,int);
    void destroy(Device&);
    void jitter(unsigned);
    View* dispatch(Device&,Resource*,Resource*,Resource*,Resource*,bool,float,float,float,float,float);
};
}
