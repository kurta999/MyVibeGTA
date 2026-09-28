#include "dx11_shader_loading.h"
#include "cpu_jobs.h"
#include <atomic>
#include <chrono>

namespace dx11::shader {
bool compileBatch(const std::vector<Request>& requests,unsigned workers,
    const std::function<bool(std::size_t,std::size_t)>& progress,
    std::vector<Compiled>& outputs,Stats& stats){
    using Clock=std::chrono::steady_clock;
    const auto started=Clock::now();
    stats={};stats.workers=std::clamp(workers,1u,8u);outputs.clear();outputs.resize(requests.size());
    struct Finish {Stats& stats;Clock::time_point start;~Finish(){stats.wallSeconds=std::chrono::duration<double>(Clock::now()-start).count();}} finish{stats,started};
    std::atomic<bool> cancelled{false};std::atomic<unsigned> active{0},peak{0};
    cpu::Pool pool(stats.workers);
    struct Cancel {std::atomic<bool>& flag;~Cancel(){flag=true;}} cancelOnExit{cancelled};
    std::vector<std::future<Compiled>> jobs;
    try{
        if(!progress(0,requests.size()))return false;
        for(std::size_t i=0;i<requests.size();++i)jobs.push_back(pool.submit([&,i]{
            Compiled result;if(cancelled.load())return result;
            auto concurrent=active.fetch_add(1)+1,oldPeak=peak.load();
            while(oldPeak<concurrent&&!peak.compare_exchange_weak(oldPeak,concurrent)){}
            struct Done {std::atomic<unsigned>& active;~Done(){--active;}} done{active};
            const auto& request=requests[i];const auto begin=Clock::now();
            ID3DBlob *code=nullptr,*errors=nullptr;
            HRESULT status=D3DCompile(request.source.data(),request.source.size(),"MiniCityShader",nullptr,nullptr,
                request.entry.c_str(),request.profile.c_str(),D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors);
            std::shared_ptr<ID3DBlob> ownedCode(code,[](ID3DBlob* blob){if(blob)blob->Release();});
            std::shared_ptr<ID3DBlob> ownedErrors(errors,[](ID3DBlob* blob){if(blob)blob->Release();});
            if(SUCCEEDED(status))result.blob=std::move(ownedCode);
            else result.error=errors?static_cast<const char*>(errors->GetBufferPointer()):"Shader compilation failed";
            result.seconds=std::chrono::duration<double>(Clock::now()-begin).count();return result;
        }));
        for(std::size_t i=0;i<jobs.size();++i){
            while(jobs[i].wait_for(std::chrono::milliseconds(10))!=std::future_status::ready)
                if(!progress(stats.completed,requests.size()))return false;
            outputs[i]=jobs[i].get();stats.cpuSeconds+=outputs[i].seconds;stats.peakActive=peak.load();
            if(!outputs[i].blob)return false;
            ++stats.completed;
            if(!progress(stats.completed,requests.size()))return false;
        }
        return true;
    }catch(const std::exception&){return false;}
}
}
