#include "dx11_texture_mips.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace dx11::texture {
namespace {
float decode(float value){
    return value<=0.04045f?value/12.92f:
        std::pow((value+0.055f)/1.055f,2.4f);
}
float encode(float value){
    value=std::clamp(value,0.0f,1.0f);
    return value<=0.0031308f?12.92f*value:
        1.055f*std::pow(value,1.0f/2.4f)-0.055f;
}
std::uint8_t byte(float value){
    return std::uint8_t(std::lround(std::clamp(value,0.0f,1.0f)*255.0f));
}
float coverage(const Level& level,float scale){
    unsigned covered=0;
    for(std::size_t i=3;i<level.pixels.size();i+=4)
        covered+=std::min(255.0f,level.pixels[i]*scale)>=128.0f;
    return float(covered)/float(level.width*level.height);
}
void dilate(Level& level){
    // Fill transparent texels with nearby opaque RGB before filtering. This
    // avoids dark fringes at cutout edges without changing alpha coverage.
    const auto original=level.pixels;
    for(unsigned y=0;y<level.height;++y)for(unsigned x=0;x<level.width;++x){
        const std::size_t at=(std::size_t(y)*level.width+x)*4;
        if(original[at+3])continue;
        int best=25,bx=-1,by=-1;
        for(int dy=-4;dy<=4;++dy)for(int dx=-4;dx<=4;++dx){
            int xx=int(x)+dx,yy=int(y)+dy,d=dx*dx+dy*dy;
            if(xx<0||yy<0||xx>=int(level.width)||yy>=int(level.height)||d>=best)continue;
            std::size_t sample=(std::size_t(yy)*level.width+xx)*4;
            if(original[sample+3]>=128){best=d;bx=xx;by=yy;}
        }
        if(bx>=0){
            std::size_t from=(std::size_t(by)*level.width+bx)*4;
            std::memcpy(level.pixels.data()+at,original.data()+from,3);
        }
    }
}
}
std::vector<Level> generate(unsigned width,unsigned height,
    const std::uint8_t* bgra,Kind kind){
    if(!width||!height||!bgra)return {};
    std::vector<Level> levels;
    Level first{width,height,std::vector<std::uint8_t>(std::size_t(width)*height*4)};
    std::memcpy(first.pixels.data(),bgra,first.pixels.size());
    if(kind==Kind::MaskedColor)dilate(first);
    if(kind==Kind::Normal)
        for(std::size_t i=3;i<first.pixels.size();i+=4)first.pixels[i]=255;
    const float originalCoverage=kind==Kind::MaskedColor?coverage(first,1):0;
    levels.push_back(std::move(first));
    while(width>1||height>1){
        const Level& previous=levels.back();
        Level next{std::max(1u,width/2),std::max(1u,height/2),{}};
        next.pixels.resize(std::size_t(next.width)*next.height*4);
        for(unsigned y=0;y<next.height;++y)for(unsigned x=0;x<next.width;++x){
            float sum[4]{};
            unsigned x0=x*width/next.width,x1=(x+1)*width/next.width;
            unsigned y0=y*height/next.height,y1=(y+1)*height/next.height;
            float samples=float((x1-x0)*(y1-y0));
            for(unsigned yy=y0;yy<y1;++yy)for(unsigned xx=x0;xx<x1;++xx){
                const auto* pixel=previous.pixels.data()+(std::size_t(yy)*width+xx)*4;
                if(kind==Kind::Normal){
                    float length=pixel[3]/255.0f;
                    for(int channel=0;channel<3;++channel)
                        sum[channel]+=(pixel[channel]/127.5f-1.0f)*length;
                }else{
                    float alpha=pixel[3]/255.0f;
                    for(int channel=0;channel<3;++channel){
                        float value=pixel[channel]/255.0f;
                        if(kind==Kind::Color||kind==Kind::MaskedColor)
                            value=decode(value)*alpha;
                        sum[channel]+=value;
                    }
                    sum[3]+=alpha;
                }
            }
            auto* output=next.pixels.data()+(std::size_t(y)*next.width+x)*4;
            if(kind==Kind::Normal){
                float length=std::sqrt(sum[0]*sum[0]+sum[1]*sum[1]+sum[2]*sum[2]);
                for(int channel=0;channel<3;++channel)
                    output[channel]=byte(0.5f+0.5f*(length>samples*0.02f?sum[channel]/length:
                        (channel==0?1.0f:0.0f)));
                output[3]=byte(length/samples);
            }else{
                float alpha=sum[3]/samples;
                for(int channel=0;channel<3;++channel){
                    float value=(kind==Kind::Color||kind==Kind::MaskedColor)?
                        (sum[3]>1e-6f?sum[channel]/sum[3]:0):sum[channel]/samples;
                    output[channel]=byte((kind==Kind::Color||kind==Kind::MaskedColor)?
                        encode(value):value);
                }
                output[3]=byte(alpha);
            }
        }
        if(kind==Kind::MaskedColor&&originalCoverage>0&&originalCoverage<1){
            float lo=0,hi=8;
            for(int step=0;step<12;++step){
                float mid=(lo+hi)*0.5f;
                if(coverage(next,mid)<originalCoverage)lo=mid;else hi=mid;
            }
            for(std::size_t i=3;i<next.pixels.size();i+=4)
                next.pixels[i]=byte(next.pixels[i]*hi/255.0f);
            dilate(next);
        }
        width=next.width;height=next.height;
        levels.push_back(std::move(next));
    }
    return levels;
}
}
