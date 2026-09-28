#include "dx11_probes.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace dx11::probes {
std::array<float,3> timeWeights(float hour){
    if(!std::isfinite(hour))return {1,0,0};
    hour=std::fmod(hour,24.0f);if(hour<0)hour+=24;
    constexpr float hours[]={0,6,6.5f,9,17,18.5f,21,24};
    constexpr unsigned states[]={2,2,1,0,0,1,2,2};
    for(unsigned i=1;i<8;++i)if(hour<=hours[i]){
        float t=(hour-hours[i-1])/(hours[i]-hours[i-1]);
        t=t*t*(3-2*t);std::array<float,3> result{};
        result[states[i-1]]+=1-t;result[states[i]]+=t;return result;
    }
    return {0,0,1};
}
bool load(const std::wstring& path,Set& result,std::string& error){
    result={};error.clear();Set candidate;
    std::ifstream file(std::filesystem::path(path),std::ios::binary);
    auto fail=[&](const char* why){error=why;return false;};
    if(!file)return fail("Probe file missing");
    auto read=[&](void* output,size_t bytes){return bool(file.read(
        static_cast<char*>(output),std::streamsize(bytes)));};
    char magic[8]{};std::uint32_t header[6]{};
    if(!read(magic,8)||std::memcmp(magic,"MCPB1\0\0\0",8)||
       !read(header,sizeof(header)))return fail("Invalid probe header");
    if(header[0]!=1||header[1]!=LOCAL_COUNT||header[2]!=TIME_COUNT||
       header[3]<16||header[3]>128||(header[3]&(header[3]-1))||
       header[5]<16||header[5]>256||(header[5]&(header[5]-1)))
        return fail("Unsupported probe dimensions/version");
    unsigned expectedMips=1;for(unsigned n=header[3];n>1;n>>=1)++expectedMips;
    if(header[4]!=expectedMips)return fail("Incomplete probe mip chain");
    candidate.size=header[3];candidate.mips=header[4];candidate.lutSize=header[5];
    if(!read(candidate.positions.data(),sizeof(candidate.positions))||
       !read(candidate.sh.data(),sizeof(candidate.sh)))return fail("Truncated probe metadata");
    for(const auto& position:candidate.positions){
        for(float n:position)if(!std::isfinite(n)||std::abs(n)>100000)
            return fail("Invalid probe position");
        if(position[3]<1||position[3]>10000)return fail("Invalid probe influence radius");
    }
    for(const auto& cube:candidate.sh)for(const auto& coefficient:cube)
        for(float n:coefficient)if(!std::isfinite(n)||std::abs(n)>1000)
            return fail("Invalid probe irradiance");
    candidate.subresources.resize(CUBE_COUNT*6*candidate.mips);
    for(unsigned cube=0;cube<CUBE_COUNT;++cube)for(unsigned face=0;face<6;++face)
        for(unsigned mip=0;mip<candidate.mips;++mip){
            unsigned n=std::max(1u,candidate.size>>mip);
            auto& pixels=candidate.subresources[(cube*6+face)*candidate.mips+mip];
            pixels.resize(size_t(n)*n*4);
            if(!read(pixels.data(),pixels.size()*2))return fail("Truncated probe cube");
            for(auto half:pixels)if((half&0x7c00)==0x7c00||
                ((half&0x8000)&&(half&0x7fff)))return fail("Invalid HDR probe texel");
        }
    candidate.brdf.resize(size_t(candidate.lutSize)*candidate.lutSize*2);
    if(!read(candidate.brdf.data(),candidate.brdf.size()*sizeof(float)))
        return fail("Truncated BRDF lookup");
    for(float n:candidate.brdf)if(!std::isfinite(n)||n<0||n>4)
        return fail("Invalid BRDF lookup");
    if(file.peek()!=std::char_traits<char>::eof())return fail("Unexpected probe trailing data");
    result=std::move(candidate);return true;
}
}
