#include "../src/dx11_probes.h"
#include <windows.h>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>

int main(){
    using namespace dx11::probes;
    for(float hour=-24;hour<=48;hour+=0.025f){
        auto weights=timeWeights(hour);float sum=0;
        for(float weight:weights){assert(weight>=0&&weight<=1);sum+=weight;}
        assert(std::abs(sum-1)<0.000001f);
        auto next=timeWeights(hour+0.001f);
        for(int i=0;i<3;++i)assert(std::abs(weights[i]-next[i])<0.004f);
    }
    assert(timeWeights(12)[0]==1&&timeWeights(18.5f)[1]==1&&timeWeights(22)[2]==1);
    assert(timeWeights(std::numeric_limits<float>::quiet_NaN())[0]==1);
    Set set;std::string error;
    assert(load(L"assets/lighting/showcase.mcpb",set,error));
    assert(set.size==128&&set.mips==8&&set.lutSize==128);
    assert(set.subresources.size()==9*6*8&&set.brdf.size()==128*128*2);
    assert(set.positions[0][0]==100&&set.positions[1][0]==300);
    auto directory=std::filesystem::temp_directory_path()/
        ("minicity-probe-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(directory);
    auto fixture=directory/"invalid.mcpb";
    {
        std::ifstream source("assets/lighting/showcase.mcpb",std::ios::binary);
        std::ofstream file(fixture,std::ios::binary);file<<source.rdbuf();file.put('x');
    }
    assert(!load(fixture.wstring(),set,error)&&set.subresources.empty());
    assert(error=="Unexpected probe trailing data");
    std::filesystem::copy_file("assets/lighting/showcase.mcpb",fixture,
        std::filesystem::copy_options::overwrite_existing);
    {std::fstream file(fixture,std::ios::binary|std::ios::in|std::ios::out);
        file.seekp(64);float nan=std::numeric_limits<float>::quiet_NaN();
        file.write(reinterpret_cast<const char*>(&nan),sizeof(nan));}
    assert(!load(fixture.wstring(),set,error)&&error=="Invalid probe irradiance");
    std::filesystem::copy_file("assets/lighting/showcase.mcpb",fixture,
        std::filesystem::copy_options::overwrite_existing);
    {std::fstream file(fixture,std::ios::binary|std::ios::in|std::ios::out);
        file.seekp(64+sizeof(Set::sh));std::uint16_t infinity=0x7c00;
        file.write(reinterpret_cast<const char*>(&infinity),sizeof(infinity));}
    assert(!load(fixture.wstring(),set,error)&&error=="Invalid HDR probe texel");
    {std::ofstream file(fixture,std::ios::binary);file.write("MCPB1\0\0\0",8);
        std::uint32_t header[6]={1,2,3,0xffffffff,8,128};
        file.write(reinterpret_cast<const char*>(header),sizeof(header));}
    assert(!load(fixture.wstring(),set,error)&&error=="Unsupported probe dimensions/version");
    {std::ofstream file(fixture,std::ios::binary);file.write("MCPB1\0\0\0",8);
        std::uint32_t header[6]={1,2,3,128,8,128};
        file.write(reinterpret_cast<const char*>(header),sizeof(header));}
    assert(!load(fixture.wstring(),set,error)&&error=="Truncated probe metadata");
    std::filesystem::remove(fixture);std::filesystem::remove(directory);
}
