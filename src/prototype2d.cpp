#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <iterator>
#include <vector>

namespace {
constexpr float PI = 3.14159265f;
constexpr float WORLD_W = 2400.0f;
constexpr float WORLD_H = 2000.0f;
constexpr float SHORE = 1690.0f;
constexpr float ROAD_W = 110.0f;

struct Vec { float x = 0, y = 0; };
Vec operator+(Vec a, Vec b) { return {a.x+b.x, a.y+b.y}; }
Vec operator-(Vec a, Vec b) { return {a.x-b.x, a.y-b.y}; }
Vec operator*(Vec a, float s) { return {a.x*s, a.y*s}; }
float length(Vec a) { return std::sqrt(a.x*a.x+a.y*a.y); }
Vec unit(Vec a) { float l=length(a); return l>0.001f ? a*(1.0f/l) : Vec{}; }
float rnd(float lo, float hi) { return lo+(hi-lo)*(float(std::rand())/RAND_MAX); }
int irnd(int hi) { return std::rand()%hi; }

struct Rect { float x,y,w,h; };
struct Ped { Vec p, target; float speed; bool alive=true; float respawn=0; COLORREF color; };
enum class Kind { Car, Bike, Boat };
struct Vehicle { Vec p; float angle=0, speed=0; Kind kind; COLORREF color; };
struct Bullet { Vec p,v; float life; int damage; };

HWND windowHandle = nullptr;
HDC backDC = nullptr;
HBITMAP backBitmap = nullptr, oldBitmap = nullptr;
int screenW=1100, screenH=720;
bool keys[256]{};
bool mouseDown=false;
POINT mouseScreen{};
Vec camera{};
Vec player{300,250};
float health=100, fireCooldown=0, invulnerable=0;
int weapon=0, occupied=-1;
std::vector<Rect> buildings;
std::vector<Ped> peds;
std::vector<Vehicle> vehicles;
std::vector<Bullet> bullets;

bool pointIn(Rect r, Vec p, float pad=0) {
    return p.x>r.x-pad && p.x<r.x+r.w+pad && p.y>r.y-pad && p.y<r.y+r.h+pad;
}
bool solid(Vec p, float radius) {
    if(p.x<radius || p.x>WORLD_W-radius || p.y<radius || p.y>SHORE-radius) return true;
    for(const auto& b:buildings) if(pointIn(b,p,radius)) return true;
    return false;
}
bool boatValid(Vec p, float radius) {
    return p.x>=radius && p.x<=WORLD_W-radius && p.y>=SHORE+radius && p.y<=WORLD_H-radius;
}
bool usable(Vec p, Kind kind, float radius) { return kind==Kind::Boat ? boatValid(p,radius) : !solid(p,radius); }
Vec randomWalkable() {
    for(int i=0;i<1000;++i) {
        Vec p{rnd(35,WORLD_W-35),rnd(35,SHORE-35)};
        if(!solid(p,18)) return p;
    }
    return {300,250};
}
void reset() {
    buildings.clear(); peds.clear(); vehicles.clear(); bullets.clear();
    player={300,250}; health=100; weapon=0; occupied=-1; fireCooldown=0; invulnerable=0;
    // Buildings fill the blocks between a regular grid of streets.
    for(int row=0;row<4;++row) for(int col=0;col<5;++col) {
        float x=300.0f+col*450.0f+ROAD_W/2+23;
        float y=250.0f+row*390.0f+ROAD_W/2+23;
        float right=300.0f+(col+1)*450.0f-ROAD_W/2-23;
        float bottom=250.0f+(row+1)*390.0f-ROAD_W/2-23;
        if(bottom>SHORE-35) bottom=SHORE-35;
        if(right>x+50 && bottom>y+50) buildings.push_back({x,y,right-x,bottom-y});
    }
    // A few buildings west of the main street make the map feel less square.
    for(int row=0;row<4;++row) buildings.push_back({45.0f,330.0f+row*390.0f,160.0f,235.0f});
    for(int i=0;i<80;++i) {
        Vec p=randomWalkable();
        peds.push_back({p,p,rnd(26,55),true,0,RGB(75+irnd(170),75+irnd(170),75+irnd(170))});
    }
    for(int i=0;i<17;++i) {
        int lane=irnd(5);
        Vec p{300.0f+lane*450.0f+rnd(-23,23),rnd(90,SHORE-90)};
        vehicles.push_back({p,rnd(-PI,PI),0,i%5==0?Kind::Bike:Kind::Car,
            RGB(80+irnd(165),75+irnd(130),75+irnd(165))});
    }
    vehicles.push_back({{345,250},0,0,Kind::Car,RGB(230,92,78)});
    vehicles.push_back({{300,335},PI/2,0,Kind::Bike,RGB(245,205,67)});
    vehicles.push_back({{300,1740},-PI/2,0,Kind::Boat,RGB(240,222,182)});
    vehicles.push_back({{800,1740},-PI/2,0,Kind::Boat,RGB(100,210,225)});
    vehicles.push_back({{1490,1740},-PI/2,0,Kind::Boat,RGB(245,145,91)});
}

void moveWithCollision(Vec& p, Vec delta, float radius, Kind kind) {
    Vec trial{p.x+delta.x,p.y};
    if(usable(trial,kind,radius)) p.x=trial.x;
    trial={p.x,p.y+delta.y};
    if(usable(trial,kind,radius)) p.y=trial.y;
}
void enterOrExit() {
    if(occupied>=0) {
        auto& v=vehicles[occupied];
        Vec right{-std::sin(v.angle),std::cos(v.angle)};
        for(int side : {1,-1}) {
            Vec out=v.p+right*(side*(v.kind==Kind::Boat?75.0f:36.0f));
            if(!solid(out,12)) { player=out; occupied=-1; return; }
        }
        // Boats can be left from the dock when close enough to shore.
        if(v.kind==Kind::Boat && v.p.y<SHORE+105) {
            Vec dock{v.p.x,SHORE-22};
            if(!solid(dock,12)) { player=dock; occupied=-1; }
        }
        return;
    }
    float closest=85; int best=-1;
    for(int i=0;i<(int)vehicles.size();++i) {
        float d=length(vehicles[i].p-player);
        if(d<closest) { closest=d; best=i; }
    }
    if(best>=0) { occupied=best; player=vehicles[best].p; }
}
void shoot(Vec aim) {
    if(occupied>=0 || health<=0 || fireCooldown>0) return;
    Vec dir=unit(aim-player);
    if(length(dir)<0.1f) return;
    int shots=weapon==2?7:1;
    for(int i=0;i<shots;++i) {
        float spread=weapon==2 ? (i-3)*0.095f : weapon==1 ? rnd(-0.035f,0.035f) : 0;
        float a=std::atan2(dir.y,dir.x)+spread;
        Vec d{std::cos(a),std::sin(a)};
        bullets.push_back({player+d*18,d*(weapon==1?900.0f:760.0f),weapon==2?0.33f:0.8f,
                           weapon==2?22:weapon==1?28:45});
    }
    fireCooldown=weapon==1?0.105f:weapon==2?0.52f:0.34f;
}
void update(float dt) {
    dt=std::min(dt,0.05f);
    fireCooldown=std::max(0.0f,fireCooldown-dt);
    invulnerable=std::max(0.0f,invulnerable-dt);
    if(keys['1']) weapon=0;
    if(keys['2']) weapon=1;
    if(keys['3']) weapon=2;
    if(health>0) {
        if(occupied<0) {
            Vec input{float(keys['D'])-float(keys['A']),float(keys['S'])-float(keys['W'])};
            input=unit(input);
            moveWithCollision(player,input*((keys[VK_SHIFT]?245.0f:165.0f)*dt),11,Kind::Car);
            if(mouseDown) shoot({mouseScreen.x+camera.x,mouseScreen.y+camera.y});
        } else {
            auto& v=vehicles[occupied];
            float acceleration=v.kind==Kind::Boat?155.0f:v.kind==Kind::Bike?290.0f:240.0f;
            float maxSpeed=v.kind==Kind::Boat?185.0f:v.kind==Kind::Bike?370.0f:310.0f;
            if(keys['W']) v.speed+=acceleration*dt;
            if(keys['S']) v.speed-=acceleration*dt;
            if(!keys['W']&&!keys['S']) v.speed*=std::pow(0.5f,dt*2.0f);
            v.speed=std::clamp(v.speed,-maxSpeed*0.45f,maxSpeed);
            float turn=float(keys['D'])-float(keys['A']);
            if(std::abs(v.speed)>5) v.angle+=turn*dt*2.5f*(v.speed>=0?1.0f:-1.0f);
            Vec before=v.p;
            moveWithCollision(v.p,{std::cos(v.angle)*v.speed*dt,std::sin(v.angle)*v.speed*dt},
                              v.kind==Kind::Boat?20.0f:18.0f,v.kind);
            if(length(v.p-before)<std::abs(v.speed*dt)*0.3f) v.speed*=0.25f;
            player=v.p;
        }
    }
    for(auto& ped:peds) {
        if(!ped.alive) {
            ped.respawn-=dt;
            if(ped.respawn<=0) { ped.p=randomWalkable(); ped.target=ped.p; ped.alive=true; }
            continue;
        }
        if(length(ped.target-ped.p)<7 || irnd(3000)==0) {
            for(int i=0;i<15;++i) {
                Vec next=ped.p+Vec{rnd(-170,170),rnd(-170,170)};
                if(!solid(next,12)) { ped.target=next; break; }
            }
        }
        Vec step=unit(ped.target-ped.p)*ped.speed*dt;
        Vec prior=ped.p;
        moveWithCollision(ped.p,step,9,Kind::Car);
        if(length(prior-ped.p)<0.1f) ped.target=ped.p;
        if(occupied>=0 && std::abs(vehicles[occupied].speed)>55 &&
           length(ped.p-player)<(vehicles[occupied].kind==Kind::Bike?18.0f:25.0f)) {
            ped.alive=false; ped.respawn=6.0f;
        }
    }
    for(auto& bullet:bullets) {
        if(bullet.life<=0) continue;
        Vec next=bullet.p+bullet.v*dt;
        // Substeps keep fast bullets from passing through a thin pedestrian.
        for(int n=1;n<=4 && bullet.life>0;++n) {
            Vec sample=bullet.p+(next-bullet.p)*(n/4.0f);
            if(solid(sample,2)) { bullet.life=0; break; }
            for(auto& ped:peds) if(ped.alive && length(ped.p-sample)<11) {
                ped.alive=false; ped.respawn=6.0f; bullet.life=0; break;
            }
        }
        bullet.p=next; bullet.life-=dt;
    }
    bullets.erase(std::remove_if(bullets.begin(),bullets.end(),[](const Bullet& b){return b.life<=0;}),bullets.end());
    // Traffic collisions can hurt the player on foot.
    if(occupied<0 && health>0 && invulnerable<=0) for(const auto& v:vehicles) {
        if(std::abs(v.speed)>70 && length(v.p-player)<24) {
            health=std::max(0.0f,health-30); invulnerable=1.0f; break;
        }
    }
    camera={std::clamp(player.x-screenW/2.0f,0.0f,std::max(0.0f,WORLD_W-screenW)),
            std::clamp(player.y-screenH/2.0f,0.0f,std::max(0.0f,WORLD_H-screenH))};
}

void fill(HDC dc,int x,int y,int w,int h,COLORREF color) {
    RECT r{x,y,x+w,y+h}; HBRUSH b=CreateSolidBrush(color); FillRect(dc,&r,b); DeleteObject(b);
}
void ellipse(HDC dc,int x,int y,int rx,int ry,COLORREF color) {
    HBRUSH b=CreateSolidBrush(color); HGDIOBJ old=SelectObject(dc,b);
    HPEN pen=CreatePen(PS_SOLID,1,color); HGDIOBJ oldPen=SelectObject(dc,pen);
    Ellipse(dc,x-rx,y-ry,x+rx,y+ry);
    SelectObject(dc,oldPen); SelectObject(dc,old); DeleteObject(pen); DeleteObject(b);
}
void line(HDC dc,int x1,int y1,int x2,int y2,COLORREF color,int width=1) {
    HPEN pen=CreatePen(PS_SOLID,width,color); HGDIOBJ old=SelectObject(dc,pen);
    MoveToEx(dc,x1,y1,nullptr); LineTo(dc,x2,y2); SelectObject(dc,old); DeleteObject(pen);
}
int sx(float x) { return int(std::round(x-camera.x)); }
int sy(float y) { return int(std::round(y-camera.y)); }
void worldFill(HDC dc,Rect r,COLORREF c) { fill(dc,sx(r.x),sy(r.y),int(r.w)+1,int(r.h)+1,c); }
void textAt(HDC dc,int x,int y,const char* s,COLORREF c) {
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,c); TextOutA(dc,x,y,s,int(std::strlen(s)));
}
void drawVehicle(HDC dc,const Vehicle& v) {
    int x=sx(v.p.x),y=sy(v.p.y);
    if(v.kind==Kind::Boat) {
        ellipse(dc,x,y,26,13,v.color);
        line(dc,x,y,x+int(std::cos(v.angle)*25),y+int(std::sin(v.angle)*25),RGB(70,80,85),3);
    } else if(v.kind==Kind::Bike) {
        Vec f{std::cos(v.angle),std::sin(v.angle)};
        ellipse(dc,x-int(f.x*11),y-int(f.y*11),5,5,RGB(20,23,28));
        ellipse(dc,x+int(f.x*11),y+int(f.y*11),5,5,RGB(20,23,28));
        line(dc,x-int(f.x*11),y-int(f.y*11),x+int(f.x*11),y+int(f.y*11),v.color,5);
    } else {
        Vec f{std::cos(v.angle),std::sin(v.angle)};
        Vec r{-f.y,f.x};
        POINT pts[4]{};
        Vec q[4]={v.p+f*22+r*12,v.p+f*22-r*12,v.p-f*22-r*12,v.p-f*22+r*12};
        for(int i=0;i<4;++i) pts[i]={sx(q[i].x),sy(q[i].y)};
        HBRUSH brush=CreateSolidBrush(v.color); HGDIOBJ old=SelectObject(dc,brush);
        HPEN pen=CreatePen(PS_SOLID,2,RGB(25,31,38)); HGDIOBJ oldPen=SelectObject(dc,pen);
        Polygon(dc,pts,4); SelectObject(dc,oldPen); SelectObject(dc,old); DeleteObject(pen); DeleteObject(brush);
        line(dc,x+int(f.x*4-r.x*9),y+int(f.y*4-r.y*9),x+int(f.x*4+r.x*9),y+int(f.y*4+r.y*9),RGB(170,215,235),3);
    }
}
void draw(HDC dc) {
    fill(dc,0,0,screenW,screenH,RGB(94,149,102));
    worldFill(dc,{0,SHORE,WORLD_W,WORLD_H-SHORE},RGB(53,122,170));
    // Pier near the first boat and a continuous waterside promenade.
    worldFill(dc,{0,SHORE-34,WORLD_W,34},RGB(188,174,145));
    for(float x: {300.0f,800.0f,1490.0f}) worldFill(dc,{x-20,SHORE-30,40,80},RGB(159,134,101));
    for(int col=0;col<5;++col) {
        float x=300.0f+col*450.0f;
        worldFill(dc,{x-ROAD_W/2,0,ROAD_W,SHORE-34},RGB(62,67,72));
        for(int y=0;y<SHORE-34;y+=58) worldFill(dc,{x-2.5f,float(y),5,29},RGB(211,197,121));
    }
    for(int row=0;row<4;++row) {
        float y=250.0f+row*390.0f;
        worldFill(dc,{0,y-ROAD_W/2,WORLD_W,ROAD_W},RGB(62,67,72));
        for(int x=0;x<WORLD_W;x+=58) worldFill(dc,{float(x),y-2.5f,29,5},RGB(211,197,121));
    }
    for(size_t i=0;i<buildings.size();++i) {
        const Rect& b=buildings[i];
        worldFill(dc,{b.x+7,b.y+9,b.w,b.h},RGB(40,70,53));
        worldFill(dc,b,i%3==0?RGB(152,136,116):i%3==1?RGB(129,145,150):RGB(171,159,130));
        worldFill(dc,{b.x+8,b.y+8,b.w-16,b.h-16},i%3==0?RGB(124,110,101):RGB(111,119,122));
        for(float wx=b.x+25;wx<b.x+b.w-10;wx+=42)
            worldFill(dc,{wx,b.y+13,17,9},RGB(92,164,187));
    }
    for(const auto& ped:peds) if(ped.alive) {
        int x=sx(ped.p.x),y=sy(ped.p.y);
        ellipse(dc,x+2,y+3,8,5,RGB(49,75,54));
        ellipse(dc,x,y,7,7,ped.color);
        ellipse(dc,x-2,y-2,2,2,RGB(226,194,158));
    }
    for(const auto& v:vehicles) drawVehicle(dc,v);
    if(occupied<0) {
        int x=sx(player.x),y=sy(player.y);
        if(invulnerable<=0 || int(invulnerable*10)%2==0) {
            ellipse(dc,x+2,y+4,11,6,RGB(45,68,49));
            ellipse(dc,x,y,10,10,RGB(235,208,81));
            Vec aim=unit(Vec{float(mouseScreen.x+camera.x),float(mouseScreen.y+camera.y)}-player);
            line(dc,x,y,x+int(aim.x*20),y+int(aim.y*20),RGB(31,36,40),4);
        }
    }
    for(const auto& b:bullets) ellipse(dc,sx(b.p.x),sy(b.p.y),3,3,RGB(255,236,113));
    // HUD remains in screen coordinates.
    fill(dc,12,12,510,92,RGB(28,34,41));
    char buf[160];
    const char* names[]={"Pistol","Rifle","Shotgun"};
    std::snprintf(buf,sizeof(buf),"HEALTH  %d       WEAPON  %s",int(health),names[weapon]);
    textAt(dc,23,20,buf,RGB(245,245,235));
    fill(dc,23,45,220,14,RGB(95,52,54));
    fill(dc,23,45,int(220*health/100),14,RGB(90,213,111));
    if(occupied>=0) {
        const char* kind=vehicles[occupied].kind==Kind::Boat?"BOAT":vehicles[occupied].kind==Kind::Bike?"BIKE":"CAR";
        std::snprintf(buf,sizeof(buf),"%s  |  E exit  |  WASD drive",kind);
    } else std::snprintf(buf,sizeof(buf),"WASD move  |  Mouse shoot  |  1-3 weapons  |  E enter");
    textAt(dc,23,70,buf,RGB(218,226,227));
    if(health<=0) {
        fill(dc,screenW/2-150,screenH/2-38,300,76,RGB(32,37,43));
        textAt(dc,screenW/2-69,screenH/2-21,"YOU DIED",RGB(252,120,110));
        textAt(dc,screenW/2-86,screenH/2+5,"Press R to restart",RGB(245,245,245));
    }
}

void resizeBuffer() {
    RECT area{}; GetClientRect(windowHandle,&area);
    screenW=std::max(1,int(area.right)); screenH=std::max(1,int(area.bottom));
    HDC dc=GetDC(windowHandle);
    if(!backDC) backDC=CreateCompatibleDC(dc);
    if(backBitmap) { SelectObject(backDC,oldBitmap); DeleteObject(backBitmap); }
    backBitmap=CreateCompatibleBitmap(dc,screenW,screenH);
    oldBitmap=(HBITMAP)SelectObject(backDC,backBitmap);
    ReleaseDC(windowHandle,dc);
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
    switch(message) {
    case WM_SIZE: if(backDC) resizeBuffer(); return 0;
    case WM_KEYDOWN:
        if(wParam<256) keys[wParam]=true;
        if(wParam==VK_ESCAPE) DestroyWindow(hwnd);
        if(wParam=='E' && !(lParam&(1<<30)) && health>0) enterOrExit();
        if(wParam=='R' && health<=0) reset();
        return 0;
    case WM_KEYUP: if(wParam<256) keys[wParam]=false; return 0;
    case WM_LBUTTONDOWN: mouseDown=true; SetCapture(hwnd); return 0;
    case WM_LBUTTONUP: mouseDown=false; ReleaseCapture(); return 0;
    case WM_MOUSEMOVE: mouseScreen={GET_X_LPARAM(lParam),GET_Y_LPARAM(lParam)}; return 0;
    case WM_KILLFOCUS: std::fill(std::begin(keys),std::end(keys),false); mouseDown=false; return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(hwnd,&ps);
        if(backDC) { draw(backDC); BitBlt(dc,0,0,screenW,screenH,backDC,0,0,SRCCOPY); }
        EndPaint(hwnd,&ps); return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd,message,wParam,lParam);
}
} // namespace

int WINAPI WinMain(HINSTANCE instance,HINSTANCE,LPSTR,int show) {
    std::srand(unsigned(std::time(nullptr)));
    WNDCLASSA wc{}; wc.lpfnWndProc=windowProc; wc.hInstance=instance;
    wc.lpszClassName="MiniCityWindow"; wc.hCursor=LoadCursor(nullptr,IDC_CROSS);
    wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    if(!RegisterClassA(&wc)) return 1;
    windowHandle=CreateWindowExA(0,wc.lpszClassName,"Mini City - C++ Sandbox",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1120,760,nullptr,nullptr,instance,nullptr);
    if(!windowHandle) return 1;
    resizeBuffer(); reset(); ShowWindow(windowHandle,show);
    LARGE_INTEGER frequency{},last{},now{};
    QueryPerformanceFrequency(&frequency); QueryPerformanceCounter(&last);
    bool running=true;
    while(running) {
        MSG msg{};
        while(PeekMessageA(&msg,nullptr,0,0,PM_REMOVE)) {
            if(msg.message==WM_QUIT) { running=false; break; }
            TranslateMessage(&msg); DispatchMessageA(&msg);
        }
        if(!running) break;
        QueryPerformanceCounter(&now);
        float dt=float(double(now.QuadPart-last.QuadPart)/double(frequency.QuadPart));
        last=now;
        update(dt);
        InvalidateRect(windowHandle,nullptr,FALSE);
        Sleep(1);
    }
    if(backDC) { SelectObject(backDC,oldBitmap); DeleteObject(backBitmap); DeleteDC(backDC); }
    return 0;
}
