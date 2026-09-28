#include "../src/dx11_shader_loading.h"
#include <cstring>
#include <cstdio>
#include <thread>
int main(){
    using namespace dx11::shader;
    const char* source="float4 VS(float4 p:POSITION):SV_Position{return p;} float4 PS():SV_Target{return float4(1,0,0,1);}";
    std::vector<Request> requests;
    for(int i=0;i<12;++i)requests.push_back({source,i%2?"PS":"VS",i%2?"ps_5_0":"vs_5_0"});
    std::vector<Compiled> serial,parallel;Stats a,b;
    auto mainThread=std::this_thread::get_id();
    auto progress=[&](size_t,size_t){return std::this_thread::get_id()==mainThread;};
    if(!compileBatch(requests,1,progress,serial,a)||!compileBatch(requests,4,progress,parallel,b)||
       a.peakActive!=1||b.peakActive<2||b.peakActive>4)return 1;
    for(size_t i=0;i<requests.size();++i)
        if(serial[i].blob->GetBufferSize()!=parallel[i].blob->GetBufferSize()||
           std::memcmp(serial[i].blob->GetBufferPointer(),parallel[i].blob->GetBufferPointer(),serial[i].blob->GetBufferSize()))return 1;
    if(compileBatch({{"invalid HLSL","VS","vs_5_0"}},4,progress,parallel,b)||parallel[0].error.empty())return 1;
    if(compileBatch(requests,4,[](size_t done,size_t){return done<1;},parallel,b))return 1;
    if(compileBatch(requests,4,[](size_t,size_t){return false;},parallel,b)||b.completed!=0)return 1;
    if(!compileBatch(requests,4,progress,parallel,b))return 1;
    std::puts("Shader loading checks passed: exact bytecode, concurrency, caller progress, errors, cancellation, reopening");
}
