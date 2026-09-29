#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include "../src/dx11_texture_loading.h"
#include "../src/cpu_jobs.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <array>

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool equal(const dx11::texture::Prepared& a,const dx11::texture::Prepared& b){
    if(!a.error.empty()||!b.error.empty()||a.format!=b.format||a.levels.size()!=b.levels.size())return false;
    for(size_t i=0;i<a.levels.size();++i)
        if(a.levels[i].width!=b.levels[i].width||a.levels[i].height!=b.levels[i].height||
           a.levels[i].pixels!=b.levels[i].pixels)return false;
    return true;
}
void ddsChecks(const std::filesystem::path& folder){
    using namespace dx11::texture;
    const auto path=folder/L"fixture.dds";
    std::array<std::uint32_t,37> header{};
    header[0]=0x20534444;header[1]=124;header[2]=0x2100f;
    header[3]=8;header[4]=12;header[7]=4;
    header[19]=32;header[20]=4;header[21]=0x30315844;header[27]=0x401008;
    header[32]=98;header[33]=3;header[35]=1;header[36]=1;
    // Non-square, non-power-of-two dimensions exercise rounded-up block pitches.
    std::vector<std::uint8_t> payload(160);
    for(unsigned i=0;i<payload.size();++i)payload[i]=std::uint8_t(i*31);
    auto save=[&](const auto& h,const auto& p){std::ofstream file(path,std::ios::binary);
        file.write(reinterpret_cast<const char*>(h.data()),148);
        file.write(reinterpret_cast<const char*>(p.data()),p.size());};
    save(header,payload);auto result=prepare({path.wstring(),Kind::Color});
    check(result.error.empty()&&result.format==Format::Bc7&&result.levels.size()==4,"valid BC7 DDS failed");
    check(result.levels[0].width==12&&result.levels[0].height==8&&result.levels[0].pixels.size()==96&&
          result.levels[1].pixels.size()==32&&result.levels[2].pixels.size()==16&&result.levels[3].pixels.size()==16,
          "DDS sub-four-pixel mip block sizes failed");
    std::vector<std::uint8_t> joined;for(const auto& level:result.levels)joined.insert(joined.end(),level.pixels.begin(),level.pixels.end());
    check(joined==payload,"DDS mip payload changed");
    check(equal(result,prepare({path.wstring(),Kind::Color})),"DDS repeat preparation differs");
    for(auto item:std::array<std::pair<unsigned,std::uint32_t>,12>{{
        {0,0},{1,123},{3,0},{4,8196},{7,3},{19,31},{21,0},{28,0x200},{32,2},{33,4},{34,4},{35,2}}}){
        auto bad=header;bad[item.first]=item.second;save(bad,payload);
        auto rejected=prepare({path.wstring(),Kind::Color});
        check(!rejected.error.empty()&&rejected.levels.empty(),"malformed DDS accepted");
    }
    auto truncated=payload;truncated.pop_back();save(header,truncated);
    check(!prepare({path.wstring()}).error.empty(),"truncated DDS accepted");
    auto extra=payload;extra.push_back(0);save(header,extra);
    check(!prepare({path.wstring()}).error.empty(),"trailing DDS bytes accepted");
    header[32]=99;save(header,payload);
    check(!prepare({path.wstring(),Kind::Linear}).error.empty(),"sRGB DDS accepted as data");
    header[32]=83;save(header,payload);
    check(prepare({path.wstring(),Kind::Normal}).format==Format::Bc5&&
          prepare({path.wstring(),Kind::Normal}).error.empty(),"BC5 normal DDS rejected");
    check(!prepare({path.wstring(),Kind::Color}).error.empty(),"BC5 accepted as colour");
    header[32]=80;payload.resize(80);save(header,payload);
    check(prepare({path.wstring(),Kind::Linear}).format==Format::Bc4&&
          prepare({path.wstring(),Kind::Linear}).error.empty(),"BC4 data DDS rejected");
    check(!prepare({path.wstring(),Kind::Normal}).error.empty(),"BC4 accepted as normal");
    header[32]=28;payload.resize((12*8+6*4+3*2+1)*4);save(header,payload);
    check(prepare({path.wstring()}).format==Format::Rgba8&&prepare({path.wstring()}).error.empty(),"RGBA DDS fallback rejected");
    std::atomic<bool> cancelled{true};check(prepare({path.wstring()},&cancelled).levels.empty(),"DDS ignored cancellation");
}
void fixture(const std::wstring& file,unsigned width,unsigned height){
    Gdiplus::Bitmap image(width,height,PixelFormat32bppARGB);
    Gdiplus::Rect region(0,0,width,height);Gdiplus::BitmapData bits{};
    check(image.LockBits(&region,Gdiplus::ImageLockModeWrite,PixelFormat32bppARGB,&bits)==Gdiplus::Ok,"fixture lock");
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        auto* pixel=static_cast<unsigned char*>(bits.Scan0)+ptrdiff_t(y)*bits.Stride+x*4;
        pixel[0]=(x*13+y*7)%256;pixel[1]=(x+y*19)%256;pixel[2]=(x*3+y)%256;
        pixel[3]=(x+y)%5?255:0;
    }
    image.UnlockBits(&bits);
    CLSID png={0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};
    check(image.Save(file.c_str(),&png,nullptr)==Gdiplus::Ok,"fixture save");
}
}
int main(){
    ULONG_PTR token=0;Gdiplus::GdiplusStartupInput startup;
    if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok)return 1;
    wchar_t temp[MAX_PATH]{};GetTempPathW(MAX_PATH,temp);
    auto folder=std::filesystem::path(temp)/(L"MiniCityLoading-"+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(folder);
    int exitCode=0;
    try{
        using namespace dx11::texture;
        ddsChecks(folder);
        auto image=(folder/L"fixture.png").wstring();fixture(image,513,257);
        std::vector<Request> requests;
        for(unsigned i=0;i<16;++i)requests.push_back({image,Kind(i%4)});
        std::vector<Prepared> reference;
        LoadingStats serial,parallel;
        auto mainThread=std::this_thread::get_id();
        check(prepareBatch(requests,1,[&](size_t index,const Prepared& result){
            check(std::this_thread::get_id()==mainThread&&index==reference.size(),"serial publication");
            check(!result.levels.empty(),"serial texture failed");reference.push_back(result);return true;
        },[](size_t,size_t){return true;},serial),"serial batch failed");
        check(prepareBatch(requests,4,[&](size_t index,const Prepared& result){
            check(std::this_thread::get_id()==mainThread,"worker published renderer data");
            check(equal(result,reference.at(index)),"parallel mip bytes differ from serial");return true;
        },[&](size_t,size_t){check(std::this_thread::get_id()==mainThread,"worker reported progress");return true;},parallel),"parallel batch failed");
        check(serial.peakActive==1&&serial.peakPending==1,"serial reference used workers");
        check(parallel.peakActive>1&&parallel.peakActive<=4&&parallel.peakPending<=4&&
              parallel.completed==requests.size(),"parallel concurrency or bounds failed");
        // Cancel while workers are generating cutout mipmaps; return only after join.
        std::vector<Request> cutouts(16,Request{image,Kind::MaskedColor});
        unsigned polls=0,published=0;
        check(!prepareBatch(cutouts,4,[&](size_t,const Prepared&){++published;return true;},
            [&](size_t,size_t){return ++polls<3;},parallel),"cancellation ignored");
        check(parallel.completed<cutouts.size(),"cancelled batch consumed all work");
        check(!prepareBatch(requests,4,[](size_t,const Prepared&){return true;},
            [](size_t,size_t){return false;},parallel)&&parallel.completed==0,"early cancel ignored");
        std::atomic<bool> cancelled{true};
        check(prepare(requests[0],&cancelled).levels.empty(),"preparation ignored cancellation");
        auto malformed=folder/L"broken.png";{std::ofstream file(malformed);file<<"invalid image";}
        for(auto bad: {Request{(folder/L"missing.png").wstring()},Request{malformed.wstring()}}){
            check(!prepare(bad).error.empty(),"bad texture accepted");
            check(!prepareBatch({bad},4,[](size_t,const Prepared& p){return p.error.empty();},
                [](size_t,size_t){return true;},parallel),"bad batch succeeded");
        }
        auto oversized=(folder/L"oversized.png").wstring();fixture(oversized,8193,1);
        check(!prepare({oversized}).error.empty(),"oversized texture accepted");
        check(!prepareBatch(requests,4,[](size_t,const Prepared&)->bool{throw std::runtime_error("consumer failure");},
            [](size_t,size_t){return true;},parallel),"consumer exception escaped");
        check(prepareBatch({requests[0]},4,[&](size_t,const Prepared& result){return equal(result,reference[0]);},
            [](size_t,size_t){return true;},parallel),"reopening after cancellation failed");
        cpu::Pool pool(2);
        auto failure=pool.submit([]()->int{throw std::runtime_error("job failure");});
        bool caught=false;try{failure.get();}catch(const std::runtime_error&){caught=true;}
        check(caught&&pool.submit([]{return 42;}).get()==42,"worker exception recovery failed");
        std::printf("Texture loading checks passed: BC7/BC5/BC4/RGBA DDS, rejected corrupt DDS, exact mip bytes, bounded concurrency, cancellation, failures, reopening; serial %.3f s\n",serial.wallSeconds);
    }catch(const std::exception& error){std::printf("Texture loading checks failed: %s\n",error.what());exitCode=1;}
    // Remove only the exact known fixture files, never a recursively computed path.
    for(const wchar_t* name:{L"fixture.png",L"fixture.dds",L"broken.png",L"oversized.png"})std::filesystem::remove(folder/name);
    std::filesystem::remove(folder);
    Gdiplus::GdiplusShutdown(token);return exitCode;
}
