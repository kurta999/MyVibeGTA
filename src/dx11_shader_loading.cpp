#include "profiling.h"
#include "dx11_shader_loading.h"
#include "cpu_jobs.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>
#include <sstream>

namespace dx11::shader {
namespace {
// Compiled bytecode is cached beside the executable, keyed by a hash of the
// shader source, entry point, profile, flags and compiler DLL version, so
// shaders are only compiled when they change. Pass --no-shader-cache to
// always compile.
constexpr unsigned COMPILE_FLAGS=D3DCOMPILE_ENABLE_STRICTNESS;
std::filesystem::path cacheFolder(){
    if(std::strstr(GetCommandLineA(),"--no-shader-cache"))return {};
    wchar_t filename[MAX_PATH]{};GetModuleFileNameW(nullptr,filename,MAX_PATH);
    return std::filesystem::path(filename).parent_path()/L"shader-cache-v1";
}
std::uint64_t cacheKey(const Request& request){
    std::uint64_t hash=14695981039346656037ULL;
    auto mix=[&](const void* data,std::size_t size){
        auto bytes=static_cast<const unsigned char*>(data);
        for(std::size_t i=0;i<size;++i)hash=(hash^bytes[i])*1099511628211ULL;
    };
    mix(request.source.data(),request.source.size());mix("|",1);
    mix(request.entry.data(),request.entry.size());mix("|",1);
    mix(request.profile.data(),request.profile.size());
    unsigned flags=COMPILE_FLAGS;mix(&flags,sizeof(flags));
    unsigned compiler=D3D_COMPILER_VERSION;mix(&compiler,sizeof(compiler));
    return hash;
}
std::shared_ptr<ID3DBlob> readCached(const std::filesystem::path& file){
    std::ifstream input(file,std::ios::binary|std::ios::ate);
    if(!input)return {};
    auto size=std::streamsize(input.tellg());
    if(size<=0||size>16*1024*1024)return {};
    ID3DBlob* blob=nullptr;
    if(FAILED(D3DCreateBlob(SIZE_T(size),&blob)))return {};
    std::shared_ptr<ID3DBlob> owned(blob,[](ID3DBlob* b){if(b)b->Release();});
    input.seekg(0);
    if(!input.read(static_cast<char*>(blob->GetBufferPointer()),size))return {};
    return owned;
}
void writeCached(const std::filesystem::path& file,ID3DBlob* blob){
    std::error_code error;std::filesystem::create_directories(file.parent_path(),error);
    std::wostringstream suffix;suffix<<L"."<<GetCurrentProcessId()<<L"."<<std::this_thread::get_id()<<L".tmp";
    auto temporary=file;temporary+=suffix.str();
    {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        if(!output)return;
        output.write(static_cast<const char*>(blob->GetBufferPointer()),std::streamsize(blob->GetBufferSize()));
        if(!output)return;
    }
    if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING))DeleteFileW(temporary.c_str());
}
}
bool compileBatch(const std::vector<Request>& requests,unsigned workers,
    const std::function<bool(std::size_t,std::size_t)>& progress,
    std::vector<Compiled>& outputs,Stats& stats){
    using Clock=std::chrono::steady_clock;
    const auto started=Clock::now();
    stats={};stats.workers=std::clamp(workers,1u,8u);outputs.clear();outputs.resize(requests.size());
    struct Finish {Stats& stats;Clock::time_point start;~Finish(){stats.wallSeconds=std::chrono::duration<double>(Clock::now()-start).count();}} finish{stats,started};
    std::atomic<bool> cancelled{false};std::atomic<unsigned> active{0},peak{0};
    cpu::Pool pool(stats.workers,"Shader loading");
    const auto folder=cacheFolder();std::atomic<std::size_t> hits{0};
    struct CountHits {Stats& stats;std::atomic<std::size_t>& hits;~CountHits(){stats.cacheHits=hits.load();}} countHits{stats,hits};
    struct Cancel {std::atomic<bool>& flag;~Cancel(){flag=true;}} cancelOnExit{cancelled};
    std::vector<std::future<Compiled>> jobs;
    try{
        if(!progress(0,requests.size()))return false;
        for(std::size_t i=0;i<requests.size();++i)jobs.push_back(pool.submit([&,i]{
            CITY_ZONE("Compile shader");
            Compiled result;if(cancelled.load())return result;
            auto concurrent=active.fetch_add(1)+1,oldPeak=peak.load();
            while(oldPeak<concurrent&&!peak.compare_exchange_weak(oldPeak,concurrent)){}
            struct Done {std::atomic<unsigned>& active;~Done(){--active;}} done{active};
            const auto& request=requests[i];const auto begin=Clock::now();
            std::filesystem::path cached;
            if(!folder.empty()){
                cached=folder/(std::to_wstring(cacheKey(request))+L".cso");
                if(auto blob=readCached(cached)){
                    result.blob=std::move(blob);++hits;
                    result.seconds=std::chrono::duration<double>(Clock::now()-begin).count();return result;
                }
            }
            ID3DBlob *code=nullptr,*errors=nullptr;
            HRESULT status=D3DCompile(request.source.data(),request.source.size(),"MyVibeGTAShader",nullptr,nullptr,
                request.entry.c_str(),request.profile.c_str(),COMPILE_FLAGS,0,&code,&errors);
            if(SUCCEEDED(status)&&code&&!cached.empty())writeCached(cached,code);
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
