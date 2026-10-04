#include "dx11_sky.h"
#include <algorithm>
#include <cmath>
namespace dx11::sky {
namespace {
std::uint32_t hash(int x,int y,int z,int period){
    auto wrap=[&](int v){return (v%period+period)%period;};
    std::uint32_t h=std::uint32_t(wrap(x))*73856093u^
        std::uint32_t(wrap(y))*19349663u^std::uint32_t(wrap(z))*83492791u;
    h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;return h^(h>>16);
}
float random(int x,int y,int z,int period){return (hash(x,y,z,period)&65535)/65535.0f;}
float lerp(float a,float b,float t){return a+(b-a)*t;}
float value(float x,float y,float z,int period){
    x*=period;y*=period;z*=period;
    int ix=int(std::floor(x)),iy=int(std::floor(y)),iz=int(std::floor(z));
    auto smooth=[](float v){return v*v*(3-2*v);};
    float fx=smooth(x-ix),fy=smooth(y-iy),fz=smooth(z-iz),layers[2]{};
    for(int k=0;k<2;++k)layers[k]=lerp(
        lerp(random(ix,iy,iz+k,period),random(ix+1,iy,iz+k,period),fx),
        lerp(random(ix,iy+1,iz+k,period),random(ix+1,iy+1,iz+k,period),fx),fy);
    return lerp(layers[0],layers[1],fz);
}
float cellular(float x,float y,float z,int period){
    x*=period;y*=period;z*=period;
    int ix=int(std::floor(x)),iy=int(std::floor(y)),iz=int(std::floor(z));
    float nearest=3;
    for(int k=-1;k<=1;++k)for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i){
        auto h=hash(ix+i,iy+j,iz+k,period);
        float dx=ix+i+float(h&1023)/1023-x;
        float dy=iy+j+float((h>>10)&1023)/1023-y;
        float dz=iz+k+float((h>>20)&1023)/1023-z;
        nearest=std::min(nearest,dx*dx+dy*dy+dz*dz);
    }
    return std::clamp(1-std::sqrt(nearest),0.0f,1.0f);
}
}
std::vector<std::uint8_t> noiseVolume(){
    std::vector<std::uint8_t> pixels(noiseSize*noiseSize*noiseSize*4);
    for(int z=0;z<noiseSize;++z)for(int y=0;y<noiseSize;++y)for(int x=0;x<noiseSize;++x){
        float u=(x+.5f)/noiseSize,v=(y+.5f)/noiseSize,w=(z+.5f)/noiseSize;
        auto at=((z*noiseSize+y)*noiseSize+x)*4;
        float fbm=value(u,v,w,4)*.5f+value(u,v,w,8)*.25f+
            value(u,v,w,16)*.14f+value(u,v,w,32)*.07f+value(u,v,w,64)*.04f;
        pixels[at]=std::uint8_t(fbm*255);
        pixels[at+1]=std::uint8_t(cellular(u,v,w,8)*255);
        pixels[at+2]=std::uint8_t(cellular(u,v,w,32)*255);
        pixels[at+3]=255;
    }
    return pixels;
}
}
