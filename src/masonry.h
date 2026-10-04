#pragma once
#include "game.h"
// Original masonry geometry shared by DX11 visuals and Jolt convex colliders.
// Coordinates are centered unit dimensions; spawn size supplies world units.
namespace masonry {
enum Kind {Brick,BrokenBrick,Block,Concrete,Slab,Count};
inline const char* name(int kind){const char* names[]={"debris/brick","debris/broken-brick","debris/block","debris/concrete","debris/slab"};return names[std::clamp(kind,0,int(Count)-1)];}
struct Triangle {game::Vec3 a,b,c;game::Color color;};
struct Geometry {std::vector<Triangle> triangles;std::vector<game::Vec3> hull;};
inline Geometry build(int kind){
    using namespace game;Geometry g;unsigned noise=unsigned(kind+1)*7919;
    auto grain=[&](){noise=noise*1664525u+1013904223u;return float((noise>>16)&255)/255;};
    auto triangle=[&](Vec3 a,Vec3 b,Vec3 c,Color col){
        std::swap(b,c); // outward winding for the clockwise X/Z contours
        // Small flat triangles give clay pores and concrete aggregate their own
        // color without stretching a building facade over the broken surface.
        Vec3 ab=(a+b)*.5f,bc=(b+c)*.5f,ca=(c+a)*.5f;
        for(auto t:{Triangle{a,ab,ca,col},Triangle{ab,b,bc,col},Triangle{ca,bc,c,col},Triangle{ab,bc,ca,col}}){
            float f=.82f+grain()*.30f;t.color={col.r*f,col.g*f,col.b*f};g.triangles.push_back(t);
        }
    };
    auto quad=[&](Vec3 a,Vec3 b,Vec3 c,Vec3 d,Color col){triangle(a,b,c,col);triangle(a,c,d,col);};
    auto box=[&](Vec3 lo,Vec3 hi,Color col){
        Vec3 a{lo.x,lo.y,lo.z},b{hi.x,lo.y,lo.z},c{hi.x,lo.y,hi.z},d{lo.x,lo.y,hi.z};
        Vec3 e{lo.x,hi.y,lo.z},f{hi.x,hi.y,lo.z},h{hi.x,hi.y,hi.z},i{lo.x,hi.y,hi.z};
        quad(a,d,c,b,col);quad(e,f,h,i,col);quad(a,b,f,e,col);quad(b,c,h,f,col);quad(c,d,i,h,col);quad(d,a,e,i,col);
        for(auto p:{a,b,c,d,e,f,h,i})g.hull.push_back(p);
    };
    Color clay=rgb(169,72,43),cement=rgb(145,142,130);
    if(kind==Block){
        // Two open cores, full depth walls and a center web.
        box({-.5f,-.5f,-.5f},{.5f,.5f,-.30f},cement);
        box({-.5f,-.5f,.30f},{.5f,.5f,.5f},cement);
        for(float x:{-.5f,-.065f,.37f})box({x,-.5f,-.30f},{x+.13f,.5f,.30f},cement);
        return g;
    }
    // Beveled clay edges; unequal fracture faces for broken brick/concrete.
    Vec2 ring[]={{-.39f,-.5f},{.36f,-.5f},{.5f,-.35f},{.5f,.34f},{.35f,.5f},{-.37f,.5f},{-.5f,.36f},{-.5f,-.35f}};
    if(kind==BrokenBrick||kind>=Concrete){
        ring[1]={.18f,-.42f};ring[2]={.43f,-.21f};ring[3]={.32f,.31f};ring[4]={.13f,.5f};ring[6]={-.46f,.16f};
    }
    Color col=kind>=Concrete?cement:clay;
    Vec3 low[8],high[8];float bottom=kind==Slab?-.32f:-.5f;
    for(int n=0;n<8;++n){
        low[n]={ring[n].x,bottom,ring[n].z};
        high[n]={ring[n].x*.91f,.5f-(kind>=Concrete?float(n%3)*.095f:kind==BrokenBrick?float(n%3)*.065f:0),ring[n].z*.91f};
        g.hull.push_back(low[n]);g.hull.push_back(high[n]);
    }
    for(int n=0;n<8;++n){int next=(n+1)%8;
        quad(low[n],low[next],high[next],high[n],col);
        if(n>0&&n<7){triangle(low[0],low[next],low[n],col);triangle(high[0],high[n],high[next],col);}
    }
    if(kind==Brick||kind==BrokenBrick){
        // A ragged remnant of mortar on a bedding face.
        triangle({-.35f,.502f,-.32f},{-.04f,.502f,-.29f},{-.29f,.502f,.29f},rgb(187,175,151));
    }
    if(kind==Slab){
        // Exposed steel rods through the broken edge; short and still attached.
        for(float z:{-.21f,.22f})box({-.50f,-.22f,z-.028f},{.50f,-.14f,z+.028f},rgb(99,65,43));
    }
    return g;
}
}
