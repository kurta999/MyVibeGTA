#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <GL/gl.h>
#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>
#include "textures.h"

namespace textures {
namespace {
GLuint tiles[32]{};
ULONG_PTR token=0;
bool loaded=false;
bool loadAtlas(const std::wstring& path,int offset){
    Gdiplus::Bitmap image(path.c_str());
    if(image.GetLastStatus()!=Gdiplus::Ok)return false;
    const int width=int(image.GetWidth()),height=int(image.GetHeight());
    if(width<512||height<512)return false;
    Gdiplus::Rect rect(0,0,width,height);
    Gdiplus::BitmapData bits{};
    if(image.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&bits)!=Gdiplus::Ok)return false;
    const int tileW=width/4,tileH=height/4,outSize=256;
    std::vector<unsigned char> pixels(outSize*outSize*4);
    for(int tile=0;tile<16;++tile){
        int col=tile%4,row=tile/4;
        for(int y=0;y<outSize;++y){
            int srcY=row*tileH+(tileH-1-y*tileH/outSize);
            const auto* src=reinterpret_cast<const unsigned char*>(bits.Scan0)+srcY*bits.Stride;
            for(int x=0;x<outSize;++x){
                int srcX=col*tileW+x*tileW/outSize;
                const unsigned char* p=src+srcX*4;
                unsigned char* q=pixels.data()+(y*outSize+x)*4;
                q[0]=p[2];q[1]=p[1];q[2]=p[0];q[3]=p[3];
            }
        }
        glBindTexture(GL_TEXTURE_2D,tiles[offset+tile]);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,outSize,outSize,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    }
    image.UnlockBits(&bits);
    return true;
}
}
bool load(){
    Gdiplus::GdiplusStartupInput startup;
    if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok)return false;
    wchar_t exe[MAX_PATH]{};
    if(!GetModuleFileNameW(nullptr,exe,MAX_PATH))return false;
    wchar_t* slash=std::wcsrchr(exe,L'\\');
    if(!slash)return false;
    *slash=0;
    std::wstring base=std::wstring(exe)+L"\\assets\\";
    glGenTextures(32,tiles);
    if(!loadAtlas(base+L"texture_atlas.png",0)||!loadAtlas(base+L"character_atlas.png",16)){
        glDeleteTextures(32,tiles);return false;
    }
    loaded=true;
    return true;
}
void bind(int tile){if(loaded&&tile>=0&&tile<32){glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,tiles[tile]);}}
void unbind(){glDisable(GL_TEXTURE_2D);}
void shutdown(){if(loaded)glDeleteTextures(32,tiles);loaded=false;if(token)Gdiplus::GdiplusShutdown(token);token=0;}
bool ready(){return loaded;}
}
