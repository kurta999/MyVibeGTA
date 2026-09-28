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

namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool equal(const dx11::texture::Prepared& a,const dx11::texture::Prepared& b){
    if(!a.error.empty()||!b.error.empty()||a.levels.size()!=b.levels.size())return false;
    for(size_t i=0;i<a.levels.size();++i)
        if(a.levels[i].width!=b.levels[i].width||a.levels[i].height!=b.levels[i].height||
           a.levels[i].pixels!=b.levels[i].pixels)return false;
    return true;
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
        std::printf("Texture loading checks passed: exact mip bytes, bounded concurrency, cancellation, failures, reopening; serial %.3f s\n",serial.wallSeconds);
    }catch(const std::exception& error){std::printf("Texture loading checks failed: %s\n",error.what());exitCode=1;}
    // Remove only the exact known fixture files, never a recursively computed path.
    for(const wchar_t* name:{L"fixture.png",L"broken.png",L"oversized.png"})std::filesystem::remove(folder/name);
    std::filesystem::remove(folder);
    Gdiplus::GdiplusShutdown(token);return exitCode;
}
