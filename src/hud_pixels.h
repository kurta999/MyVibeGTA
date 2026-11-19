#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

// CPU-side HUD compositing helpers. The GDI canvas has no alpha channel, so
// any non-black pixel is treated as opaque HUD content.
namespace hud_pixels {
// Copies BGRA pixels from source to destination, marking drawn pixels opaque
// and leaving untouched (pure black) pixels fully transparent. Works on whole
// 32-bit words so the compiler can vectorize it.
inline void finalize(const unsigned char* source,unsigned char* destination,std::size_t pixelCount){
    for(std::size_t i=0;i<pixelCount;++i){
        std::uint32_t value;
        std::memcpy(&value,source+i*4,4);
        value=(value&0x00FFFFFFu)?(value|0xFF000000u):0u;
        std::memcpy(destination+i*4,&value,4);
    }
}

// finalize() plus clearing the source canvas in the same pass, so the next
// frame starts from a blank canvas without a separate full-screen memset.
inline void finalizeAndClear(unsigned char* source,unsigned char* destination,std::size_t pixelCount){
    for(std::size_t i=0;i<pixelCount;++i){
        std::uint32_t value;
        std::memcpy(&value,source+i*4,4);
        value=(value&0x00FFFFFFu)?(value|0xFF000000u):0u;
        const std::uint32_t zero=0;std::memcpy(source+i*4,&zero,4);
        std::memcpy(destination+i*4,&value,4);
    }
}

// Strength of the red damage vignette at a pixel, 0 at the centre rising to
// `intensity` at the edges. Only pixels outside the inner ellipse are tinted.
inline float vignette(int x,int y,int width,int height,float intensity){
    if(width<=0||height<=0||intensity<=0)return 0;
    float nx=(x+0.5f)/width*2-1,ny=(y+0.5f)/height*2-1;
    float distance=std::sqrt(nx*nx*0.85f+ny*ny);
    float edge=std::clamp((distance-0.62f)/0.55f,0.0f,1.0f);
    return std::min(1.0f,intensity)*edge*edge;
}

// Blends a red vignette into transparent and translucent HUD pixels without
// covering opaque HUD widgets. Rows/columns inside the clear centre are skipped.
inline void applyDamageVignette(unsigned char* pixels,int width,int height,float intensity){
    if(!pixels||width<=0||height<=0||intensity<=0.01f)return;
    for(int y=0;y<height;++y){
        unsigned char* row=pixels+std::size_t(y)*width*4;
        for(int x=0;x<width;++x){
            float strength=vignette(x,y,width,height,intensity);
            if(strength<=0.004f){
                // The clear region is convex: jump to its mirrored exit.
                int mirror=width-1-x;
                if(mirror>x)x=mirror-1;
                continue;
            }
            unsigned char* p=row+std::size_t(x)*4;
            if(p[3]==255)continue;
            unsigned alpha=unsigned(std::min(200.0f,strength*200.0f));
            if(alpha<=p[3])continue;
            p[0]=18;p[1]=10;p[2]=150;p[3]=static_cast<unsigned char>(alpha);
        }
    }
}
}
