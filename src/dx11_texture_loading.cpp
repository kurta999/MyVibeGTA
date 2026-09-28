#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "dx11_texture_loading.h"
#include "cpu_jobs.h"
#include <chrono>
#include <cstring>
#include <deque>

namespace dx11::texture {
namespace {
using Clock=std::chrono::steady_clock;
double seconds(Clock::time_point start){return std::chrono::duration<double>(Clock::now()-start).count();}
bool stopped(const std::atomic<bool>* cancel){return cancel&&cancel->load(std::memory_order_relaxed);}
}
unsigned defaultLoadingWorkers(){
    unsigned hardware=std::thread::hardware_concurrency();
    return std::min(4u,hardware>1?hardware-1:1u);
}
Prepared prepare(const Request& request,const std::atomic<bool>* cancel){
    Prepared result;
    if(stopped(cancel))return result;
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
