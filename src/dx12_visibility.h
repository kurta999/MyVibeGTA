#pragma once
#include "dx11_assets.h"
#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace dx12 {
// A caster sphere swept away from the sun must reach the visible receiver
// volume. Expanded planes and radial bounds are conservative: off-screen
// casters remain when any part of their shadow can enter the camera frustum.
struct ShadowReceivers {
    std::array<DirectX::XMFLOAT4,6> planes{};
    DirectX::XMFLOAT3 eye{},direction{};
    ShadowReceivers(const DirectX::XMFLOAT4X4& matrix,DirectX::XMFLOAT3 camera,DirectX::XMFLOAT3 sun):eye(camera){
        using namespace DirectX;
        XMStoreFloat3(&direction,XMVectorNegate(XMVector3Normalize(XMLoadFloat3(&sun))));
        for(unsigned i=0;i<6;++i){XMFLOAT4 p;float* output=&p.x;
            for(unsigned row=0;row<4;++row){
                output[row]=i==4?matrix.m[row][2]:i==5?matrix.m[row][3]-matrix.m[row][2]:
                    matrix.m[row][3]+(i%2?-1.0f:1.0f)*matrix.m[row][i/2];
            }
            float size=std::sqrt(p.x*p.x+p.y*p.y+p.z*p.z);
            planes[i]={p.x/size,p.y/size,p.z/size,p.w/size};
        }
    }
    bool reaches(const dx11::BoundingSphere& sphere,float maxRange,float padding) const {
        double first=0,last=std::numeric_limits<double>::infinity();
        const double radius=sphere.radius+padding;
        for(const auto& p:planes){
            double distance=double(p.x)*sphere.x+double(p.y)*sphere.y+double(p.z)*sphere.z+p.w+radius;
            double speed=double(p.x)*direction.x+double(p.y)*direction.y+double(p.z)*direction.z;
            if(std::abs(speed)<1e-8){if(distance<0)return false;}
            else if(speed>0)first=std::max(first,-distance/speed);
            else last=std::min(last,-distance/speed);
            if(first>last)return false;
        }
        if(std::isfinite(maxRange)){
            double x=sphere.x-eye.x,y=sphere.y-eye.y,z=sphere.z-eye.z;
            double b=x*direction.x+y*direction.y+z*direction.z;
            double a=double(direction.x)*direction.x+double(direction.y)*direction.y+double(direction.z)*direction.z;
            double limit=maxRange+radius,c=x*x+y*y+z*z-limit*limit;
            double discriminant=b*b-a*c;if(discriminant<0)return false;
            double root=std::sqrt(discriminant);
            first=std::max(first,(-b-root)/a);last=std::min(last,(-b+root)/a);
        }
        return first<=last;
    }
};
}
