#include "../src/dx12_visibility.h"
#include <cstdio>
#include <stdexcept>

void dx12VisibilityScenarios(){
    using namespace DirectX;
    auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    XMFLOAT4X4 matrix;
    XMStoreFloat4x4(&matrix,XMMatrixLookAtRH(XMVectorZero(),XMVectorSet(0,0,1,1),XMVectorSet(0,1,0,0))*XMMatrixPerspectiveFovRH(XM_PIDIV2,1,2,1000));
    dx12::ShadowReceivers overhead(matrix,{0,0,0},{0,1,0});
    check(overhead.reaches({0,500,20,1},100,0),"off-screen overhead caster must be retained");
    check(!overhead.reaches({500,10,20,1},100,0),"lateral caster cannot reach receivers");
    check(!overhead.reaches({0,10,-20,1},100,0),"caster behind eye moving down cannot reach receivers");
    check(!overhead.reaches({0,10,20,1},10,0),"near cascade radial receiver bound");
    dx12::ShadowReceivers rearSun(matrix,{0,0,0},{0,0,-1});
    check(rearSun.reaches({0,0,-20,1},100,0),"caster behind eye casting forward must be retained");
    unsigned seed=0x63ce9841;
    auto random=[&]{seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return float(seed&0xffffff)/float(0x1000000);};
    unsigned tested=0;
    for(int camera=0;camera<4;++camera){
        XMFLOAT3 eye{camera*1400.0f,30+camera*500.0f,-800.0f*camera};
        XMVECTOR origin=XMLoadFloat3(&eye),forward=XMVector3Normalize(XMVectorSet(.3f+camera*.7f,.15f-camera*.15f,1,0));
        XMVECTOR right=XMVector3Normalize(XMVector3Cross(forward,XMVectorSet(0,1,0,0))),up=XMVector3Cross(right,forward);
        const float fov=.7f+camera*.3f,aspect=.7f+camera*.6f,tangent=std::tan(fov*.5f);
        XMStoreFloat4x4(&matrix,XMMatrixLookAtRH(origin,XMVectorAdd(origin,forward),XMVectorSet(0,1,0,0))*XMMatrixPerspectiveFovRH(fov,aspect,2,5000));
        for(XMFLOAT3 sun:std::array<XMFLOAT3,4>{{{.7f,.7f,.3f},{-.9f,.02f,.4f},{0,1,0},{.1f,-.5f,-.8f}}}){
            dx12::ShadowReceivers receivers(matrix,eye,sun);XMVECTOR light=XMVector3Normalize(XMLoadFloat3(&sun));
            for(int i=0;i<2000;++i){
                float depth=2.1f+random()*4997;
                XMVECTOR point=origin+forward*depth+right*((random()*2-1)*depth*tangent*aspect)+up*((random()*2-1)*depth*tangent);
                float radius=.1f+random()*80;
                XMVECTOR offset=XMVector3Normalize(XMVectorSet(random()-.5f,random()-.5f,random()-.5f,0))*(random()*radius);
                XMFLOAT3 caster;XMStoreFloat3(&caster,point+light*(random()*5000)+offset);
                float range=XMVectorGetX(XMVector3Length(point-origin));
                check(receivers.reaches({caster.x,caster.y,caster.z,radius},range+.1f,.1f),"swept sphere must keep a known visible receiver, including cascade edges");
                ++tested;
            }
        }
    }
    std::printf("PASS shadow receiver culling: %u known receiver/caster pairs, off-screen/behind-eye casters, low sun, camera rotations, aspect ratios, radial cascade bounds\n",tested);
}
