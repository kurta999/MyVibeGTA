#include "startup.h"
#include "resource.h"
#include <windows.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
#include <vector>

namespace {
bool check(bool passed,const char* message){
    if(!passed)std::fprintf(stderr,"Startup smoke failed: %s\n",message);
    return passed;
}
bool capture(HWND hwnd,const char* path){
    RECT bounds{};GetWindowRect(hwnd,&bounds);
    const int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
    HDC dc=GetDC(hwnd),memory=CreateCompatibleDC(dc);
    HBITMAP bitmap=CreateCompatibleBitmap(dc,width,height);
    auto old=SelectObject(memory,bitmap);
    bool ok=PrintWindow(hwnd,memory,0)!=FALSE;
    SelectObject(memory,old);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    std::vector<unsigned char> pixels(size_t(width)*height*4);
    ok=ok&&GetDIBits(memory,bitmap,0,height,pixels.data(),&info,DIB_RGB_COLORS)!=0;
    BITMAPFILEHEADER header{};header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);
    header.bfSize=header.bfOffBits+DWORD(pixels.size());
    std::ofstream file(path,std::ios::binary);
    file.write(reinterpret_cast<const char*>(&header),sizeof(header));
    file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));
    file.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
    ok=ok&&bool(file);
    DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(hwnd,dc);return ok;
}
}
int main(int argc,char** argv){
    HINSTANCE module=GetModuleHandleA(nullptr);
    for(int size:{16,24,32,48,64,128,256}){
        HICON icon=static_cast<HICON>(LoadImageA(module,MAKEINTRESOURCEA(IDI_MINICITY),
            IMAGE_ICON,size,size,0));
        if(!check(icon!=nullptr,"embedded icon could not be loaded"))return 1;
        DestroyIcon(icon);
    }
    {startup::Session hidden(false);
        if(!check(FindWindowA("MiniCity3DLoading",nullptr)==nullptr,"headless launch showed a loading window"))return 1;
        startup::report(100,"Ready");hidden.finish();hidden.finish();
    }
    {startup::Session loading(true);
        HWND hwnd=FindWindowA("MiniCity3DLoading",nullptr);
        if(!check(hwnd&&IsWindowVisible(hwnd),"loading window was not immediately visible"))return 1;
        if(!check(SendMessageA(hwnd,WM_GETICON,ICON_SMALL,0)!=0&&
            GetClassLongPtrA(hwnd,GCLP_HICON)!=0,"loading window icons missing"))return 1;
        startup::report(65,"Uploading regional graphics");
        // The startup thread does no message pumping while it is busy.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        DWORD_PTR result=0;
        if(!check(SendMessageTimeoutA(hwnd,WM_NULL,0,0,SMTO_ABORTIFHUNG,1000,&result)!=0,
            "loading message loop froze during startup work"))return 1;
        if(argc>1&&!check(capture(hwnd,argv[1]),"loading preview capture failed"))return 1;
        SendMessageTimeoutA(hwnd,WM_CLOSE,0,0,SMTO_ABORTIFHUNG,1000,&result);
        if(!check(loading.cancelled()&&!startup::report(80,"Loading HDR lighting"),
            "close request did not cancel startup"))return 1;
        loading.finish();
        if(!check(!IsWindow(hwnd),"cancelled loading window was not destroyed"))return 1;
    }
    // Reopening catches stale window classes, handles, and joinable threads.
    {startup::Session loading(true);startup::report(100,"Ready");loading.finish();}
    if(!check(FindWindowA("MiniCity3DLoading",nullptr)==nullptr,"finished loading window leaked"))return 1;
    std::puts("Startup window responsiveness, cancellation, lifecycle and seven icon sizes passed");
    return 0;
}
