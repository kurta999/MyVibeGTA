#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "dx11_texture_loading.h"
#include "cpu_jobs.h"
#include <chrono>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <array>
#include <cwctype>

namespace dx11::texture {
namespace {
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point start){return std::chrono::duration<double>(Clock::now()-start).count();}
bool stopped(const std::atomic<bool>* cancel){return cancel&&cancel->load(std::memory_order_relaxed);}
Prepared prepareDds(const Request& request,const std::atomic<bool>* cancel){
    Prepared result;const auto started=Clock::now();
    auto fail=[&](const char* error){result.levels.clear();result.error=error;return result;};
    std::ifstream file(std::filesystem::path(request.file),std::ios::binary|std::ios::ate);
    if(!file)return fail("Cannot open DDS texture");
    const auto length=file.tellg();file.seekg(0);
    // Fixed DX10 header, single 2D surface. No permissive legacy reinterpretation.
    std::array<std::uint32_t,37> h{};
    if(length<148||!file.read(reinterpret_cast<char*>(h.data()),148))return fail("Truncated DDS header");
    if(h[0]!=0x20534444||h[1]!=124||h[19]!=32||h[20]!=4||h[21]!=0x30315844)
        return fail("Unsupported DDS header (DX10 required)");
    if((h[2]&0x1007)!=0x1007||!(h[27]&0x1000)||h[6]>1||h[28]||h[33]!=3||h[34]||h[35]!=1||(h[36]&~7u))
        return fail("DDS must contain one ordinary 2D texture");
    const unsigned width=h[4],height=h[3],mips=h[7];
    if(!width||!height||width>8192||height>8192)return fail("Invalid DDS dimensions");
    unsigned expectedMips=1;for(unsigned size=std::max(width,height);size>1;size>>=1)++expectedMips;
    if(mips!=expectedMips)return fail("DDS requires a complete mip chain");
    const bool color=request.kind==Kind::Color||request.kind==Kind::MaskedColor;
    switch(h[32]){
    case 28:case 29:result.format=Format::Rgba8;break;
    case 87:case 91:result.format=Format::Bgra8;break;
    case 98:case 99:result.format=Format::Bc7;break;
    case 83:result.format=Format::Bc5;break;
    case 80:result.format=Format::Bc4;break;
    default:return fail("Unsupported DDS format");
    }
    if(!color&&(h[32]==29||h[32]==91||h[32]==99))return fail("sRGB DDS cannot be used as material data");
    if((result.format==Format::Bc5&&request.kind!=Kind::Normal)||
       (result.format==Format::Bc4&&request.kind!=Kind::Linear)||
       (request.kind==Kind::Normal&&result.format==Format::Bc7))return fail("DDS format does not match texture use");
    if(result.format>=Format::Bc7&&(width%4||height%4))return fail("DDS block texture dimensions must be multiples of four");
    std::uint64_t payload=0;unsigned w=width,y=height;
    for(unsigned i=0;i<mips;++i){payload+=std::uint64_t(rowPitch(w,result.format))*rowCount(y,result.format);w=std::max(1u,w/2);y=std::max(1u,y/2);}
    if(payload+148!=std::uint64_t(length))return fail("DDS payload size does not match its mip chain");
    w=width;y=height;result.levels.reserve(mips);
    for(unsigned i=0;i<mips;++i){
        if(stopped(cancel)){result.levels.clear();return result;}
        Level level;level.width=w;level.height=y;
        level.pixels.resize(std::size_t(rowPitch(w,result.format))*rowCount(y,result.format));
        if(!file.read(reinterpret_cast<char*>(level.pixels.data()),level.pixels.size()))return fail("Truncated DDS mip data");
        result.levels.push_back(std::move(level));w=std::max(1u,w/2);y=std::max(1u,y/2);
    }
    result.decodeSeconds=seconds(started);return result;
}
}
unsigned rowPitch(unsigned width,Format format){
    if(format==Format::Bc4)return std::max(1u,(width+3)/4)*8;
    if(format==Format::Bc5||format==Format::Bc7)return std::max(1u,(width+3)/4)*16;
    return width*4;
}
unsigned rowCount(unsigned height,Format format){return format>=Format::Bc7?std::max(1u,(height+3)/4):height;}
unsigned defaultLoadingWorkers(){
    unsigned hardware=std::thread::hardware_concurrency();
    return std::min(4u,hardware>1?hardware-1:1u);
}
Prepared prepare(const Request& request,const std::atomic<bool>* cancel){
    Prepared result;
    if(stopped(cancel))return result;
    auto extension=std::filesystem::path(request.file).extension().wstring();
    for(auto& ch:extension)ch=std::towlower(ch);
    if(extension==L".dds")return prepareDds(request,cancel);
    auto started=Clock::now();
    Gdiplus::Bitmap image(request.file.c_str());
    if(image.GetLastStatus()!=Gdiplus::Ok){result.error="Cannot decode texture";return result;}
    unsigned width=image.GetWidth(),height=image.GetHeight();
    if(!width||!height||width>8192||height>8192){result.error="Invalid texture dimensions";return result;}
    Gdiplus::Rect region(0,0,width,height);Gdiplus::BitmapData bits{};
    if(image.LockBits(&region,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&bits)!=Gdiplus::Ok){
        result.error="Cannot read texture pixels";return result;
    }
    // Unlock even if allocation fails; the worker reports exceptions to its future.
    struct Unlock {Gdiplus::Bitmap& image;Gdiplus::BitmapData& bits;~Unlock(){image.UnlockBits(&bits);}};
    std::vector<unsigned char> pixels;
    {
        Unlock unlock{image,bits};pixels.resize(std::size_t(width)*height*4);
        for(unsigned row=0;row<height;++row){
            if(stopped(cancel))return result;
            std::memcpy(pixels.data()+std::size_t(row)*width*4,
                static_cast<const unsigned char*>(bits.Scan0)+ptrdiff_t(row)*bits.Stride,width*4);
        }
    }
    result.decodeSeconds=seconds(started);started=Clock::now();
    result.levels=generate(width,height,pixels.data(),request.kind,cancel);
    result.mipSeconds=seconds(started);
    if(result.levels.empty()&&!stopped(cancel))result.error="Cannot generate texture mipmaps";
    return result;
}
bool prepareBatch(const std::vector<Request>& requests,unsigned workers,
    const std::function<bool(std::size_t,const Prepared&)>& consume,
    const std::function<bool(std::size_t,std::size_t)>& progress,LoadingStats& stats){
    stats={};stats.workers=std::clamp(workers,1u,8u);
    const auto started=Clock::now();
    struct Finish {LoadingStats& stats;Clock::time_point started;~Finish(){stats.wallSeconds=seconds(started);}} finish{stats,started};
    std::atomic<bool> cancel{false};
    std::atomic<unsigned> active{0},peak{0};
    cpu::Pool pool(stats.workers);
    // Declare the guard after the pool so cancellation is signalled before join.
    struct Cancel {std::atomic<bool>& flag;~Cancel(){flag=true;}} cancelOnExit{cancel};
    std::deque<std::future<Prepared>> pending;
    std::size_t submitted=0;
    auto enqueue=[&]{
        const auto index=submitted++;
        pending.push_back(pool.submit([&,index]{
            auto concurrent=active.fetch_add(1)+1,oldPeak=peak.load();
            while(oldPeak<concurrent&&!peak.compare_exchange_weak(oldPeak,concurrent)){}
            struct Done {std::atomic<unsigned>& active;~Done(){--active;}} done{active};
            return prepare(requests[index],&cancel);
        }));
        stats.peakPending=std::max(stats.peakPending,pending.size());
    };
    try{
        if(!progress(0,requests.size()))return false;
        while(submitted<requests.size()&&pending.size()<stats.workers)enqueue();
        while(!pending.empty()){
            while(pending.front().wait_for(std::chrono::milliseconds(10))!=std::future_status::ready)
                if(!progress(stats.completed,requests.size()))return false;
            if(!progress(stats.completed,requests.size()))return false;
            {
                auto prepared=pending.front().get();pending.pop_front();
                stats.decodeSeconds+=prepared.decodeSeconds;stats.mipSeconds+=prepared.mipSeconds;
                stats.peakActive=peak.load();
                auto consumed=Clock::now();
                bool ok=consume(stats.completed,prepared);
                stats.consumeSeconds+=seconds(consumed);
                if(!ok)return false;
                ++stats.completed;
                if(!progress(stats.completed,requests.size()))return false;
            } // Release this image before admitting the next job into the window.
            if(submitted<requests.size())enqueue();
        }
        return true;
    }catch(const std::exception&){return false;}
}
}
