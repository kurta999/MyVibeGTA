#include "startup.h"
#include "logging.h"
#include "resource.h"
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>

namespace startup {
namespace {
using Clock=std::chrono::steady_clock;
Session* active=nullptr; // Only accessed by the startup/game thread.
constexpr UINT finishMessage=WM_APP+1;
constexpr char windowClass[]="MiniCity3DLoading";
void fill(HDC dc,const RECT& rect,COLORREF color){
    HBRUSH brush=CreateSolidBrush(color);FillRect(dc,&rect,brush);DeleteObject(brush);
}
void text(HDC dc,int x,int y,int width,int height,const char* value,COLORREF color){
    RECT rect{x,y,x+width,y+height};SetTextColor(dc,color);
    DrawTextA(dc,value,-1,&rect,DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
}
}
struct Session::State {
    std::mutex mutex;
    std::condition_variable ready;
    std::thread thread;
    HWND window=nullptr;
    bool created=false,finished=false;
    std::atomic<bool> cancel{false};
    int percent=0;
    std::string stage="Starting Mini City 3D";
    Clock::time_point started=Clock::now(),stageStarted=started;
    HFONT titleFont=nullptr,bodyFont=nullptr,smallFont=nullptr;

    void draw(HWND hwnd,HDC dc){
        RECT area{};GetClientRect(hwnd,&area);
        HDC buffer=CreateCompatibleDC(dc);
        HBITMAP bitmap=CreateCompatibleBitmap(dc,area.right,area.bottom);
        auto oldBitmap=SelectObject(buffer,bitmap);
        fill(buffer,area,RGB(14,23,35));SetBkMode(buffer,TRANSPARENT);
        int progress;std::string label;
        {std::lock_guard<std::mutex> lock(mutex);progress=percent;label=stage;}
        const int width=area.right;
        HICON icon=static_cast<HICON>(LoadImageA(GetModuleHandleA(nullptr),
            MAKEINTRESOURCEA(IDI_MINICITY),IMAGE_ICON,64,64,LR_SHARED));
        if(icon)DrawIconEx(buffer,32,28,icon,64,64,0,nullptr,DI_NORMAL);
        auto oldFont=SelectObject(buffer,titleFont);
        text(buffer,116,32,width-148,42,"MINI CITY 3D",RGB(241,247,252));
        SelectObject(buffer,bodyFont);
        text(buffer,116,72,width-148,25,"Your city is getting ready",RGB(151,173,193));
        text(buffer,32,125,width-64,28,cancel?"Cancelling after the current step...":label.c_str(),
            RGB(232,240,247));
        RECT track{32,167,width-32,181};fill(buffer,track,RGB(37,53,69));
        RECT bar=track;bar.right=bar.left+(track.right-track.left)*progress/100;
        if(bar.right>bar.left){
            fill(buffer,bar,RGB(52,207,188));
            // Animate within completed work; never invent additional progress.
            int span=bar.right-bar.left;
            int offset=int(GetTickCount64()/12)%std::max(1,span+50)-50;
            RECT shimmer{std::max(bar.left,bar.left+offset),bar.top,
                std::min(bar.right,bar.left+offset+50),bar.bottom};
            if(shimmer.right>shimmer.left)fill(buffer,shimmer,RGB(117,234,216));
        }
        double elapsed=std::chrono::duration<double>(Clock::now()-started).count();
        char detail[96]{};std::snprintf(detail,sizeof(detail),"%d%%   |   %.0f seconds elapsed",progress,elapsed);
        SelectObject(buffer,smallFont);
        text(buffer,32,197,width-64,24,detail,RGB(151,173,193));
        text(buffer,32,235,width-64,24,"Loading graphics, city assets and your saved progress",RGB(151,173,193));
        BitBlt(dc,0,0,area.right,area.bottom,buffer,0,0,SRCCOPY);
        SelectObject(buffer,oldFont);SelectObject(buffer,oldBitmap);
        DeleteObject(bitmap);DeleteDC(buffer);
    }
    void paint(HWND hwnd){
        PAINTSTRUCT ps{};HDC dc=BeginPaint(hwnd,&ps);
        draw(hwnd,dc);EndPaint(hwnd,&ps);
    }
    static LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
        auto self=reinterpret_cast<State*>(GetWindowLongPtrA(hwnd,GWLP_USERDATA));
        if(msg==WM_NCCREATE){
            self=static_cast<State*>(reinterpret_cast<CREATESTRUCTA*>(lp)->lpCreateParams);
            SetWindowLongPtrA(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(self)switch(msg){
        case WM_PAINT:self->paint(hwnd);return 0;
        case WM_PRINTCLIENT:self->draw(hwnd,reinterpret_cast<HDC>(wp));return 0;
        case WM_ERASEBKGND:return 1;
        case WM_TIMER:InvalidateRect(hwnd,nullptr,FALSE);return 0;
        case WM_CLOSE:self->cancel=true;InvalidateRect(hwnd,nullptr,FALSE);return 0;
        case finishMessage:DestroyWindow(hwnd);return 0;
        case WM_DESTROY:KillTimer(hwnd,1);PostQuitMessage(0);return 0;
        }
        return DefWindowProcA(hwnd,msg,wp,lp);
    }
    void run(){
        HINSTANCE instance=GetModuleHandleA(nullptr);
        WNDCLASSA wc{};wc.lpfnWndProc=windowProc;wc.hInstance=instance;
        wc.hCursor=LoadCursor(nullptr,IDC_ARROW);
        wc.hIcon=LoadIconA(instance,MAKEINTRESOURCEA(IDI_MINICITY));
        wc.lpszClassName=windowClass;
        const bool registered=RegisterClassA(&wc)!=0;
        const bool available=registered||GetLastError()==ERROR_CLASS_ALREADY_EXISTS;
        titleFont=CreateFontA(-30,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
        bodyFont=CreateFontA(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
        smallFont=CreateFontA(-15,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
        RECT bounds{0,0,580,280};constexpr DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
        AdjustWindowRect(&bounds,style,FALSE);
        int width=bounds.right-bounds.left,height=bounds.bottom-bounds.top;
        POINT cursor{};GetCursorPos(&cursor);MONITORINFO monitor{sizeof(monitor)};
        GetMonitorInfoA(MonitorFromPoint(cursor,MONITOR_DEFAULTTOPRIMARY),&monitor);
        const RECT& work=monitor.rcWork;
        HWND hwnd=available?CreateWindowExA(0,windowClass,"Mini City 3D - Loading",style,
            work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,
            width,height,nullptr,nullptr,instance,this):nullptr;
        if(hwnd){
            SendMessageA(hwnd,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(
                LoadImageA(instance,MAKEINTRESOURCEA(IDI_MINICITY),IMAGE_ICON,
                    GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED)));
            ShowWindow(hwnd,SW_SHOWNORMAL);UpdateWindow(hwnd);SetTimer(hwnd,1,100,nullptr);
        }
        {std::lock_guard<std::mutex> lock(mutex);window=hwnd;created=true;}
        ready.notify_one();
        if(hwnd){MSG msg{};while(GetMessageA(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}}
        DeleteObject(titleFont);DeleteObject(bodyFont);DeleteObject(smallFont);
        if(registered)UnregisterClassA(windowClass,instance);
    }
};
Session::Session(bool visible):state(std::make_unique<State>()){
    active=this;
    if(visible){
        state->thread=std::thread([this]{state->run();});
        std::unique_lock<std::mutex> lock(state->mutex);
        state->ready.wait(lock,[this]{return state->created;});
        if(!state->window)logging::write("Loading window unavailable; continuing startup");
    }
}
Session::~Session(){finish();}
bool Session::cancelled() const{return state->cancel.load();}
bool report(int percent,const char* stage){
    if(!active)return true;
    auto& state=*active->state;
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto now=Clock::now();
    if(state.stage!=stage){
        char line[256]{};std::snprintf(line,sizeof(line),"Startup: %s took %.3f s",state.stage.c_str(),
            std::chrono::duration<double>(now-state.stageStarted).count());
        logging::write(line);state.stageStarted=now;state.stage=stage;
        logging::write(("Startup: "+state.stage).c_str());
    }
    state.percent=std::max(state.percent,std::clamp(percent,0,100));
    return !state.cancel;
}
void Session::finish(){
    if(state->finished)return;
    state->finished=true;
    char line[128]{};std::snprintf(line,sizeof(line),"Startup %s in %.3f s",
        state->cancel?"cancelled":state->percent==100?"ready":"stopped before ready",
        std::chrono::duration<double>(Clock::now()-state->started).count());
    logging::write(line);
    if(state->window)PostMessageA(state->window,finishMessage,0,0);
    if(state->thread.joinable())state->thread.join();
    if(active==this)active=nullptr;
}
}
