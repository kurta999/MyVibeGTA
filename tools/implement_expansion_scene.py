from pathlib import Path
root=Path(__file__).resolve().parents[1]
def edit(file,a,b):
 p=root/file;s=p.read_text();assert a in s,(file,a[:90]);p.write_text(s.replace(a,b))
edit('src/vehicle_systems.cpp','r-physics::vehicleRestHeight(v.kind)','r')
edit('src/dx11_scene.cpp','#include "jolt_world.h"','#include "jolt_world.h"\n#include "destruction.h"\n#include "vehicle_systems.h"')
edit('src/dx11_scene.cpp','        if(b.id.rfind("outpost-",0)==0){', '''        if(b.damaged){
            for(const auto& box:destruction::boxes(b)){
                Vec3 size=box.high-box.low;
                model("primitive/box",{(box.low.x+box.high.x)*.5f,box.low.y,(box.low.z+box.high.z)*.5f},size,0,b.c);
                // Keep readable facade detail on surviving wall slabs.
                for(float h=box.low.y+12;h<box.high.y-8;h+=30)
                    for(float x=box.low.x+12;x<box.high.x-8;x+=28)
                        if(box.low.z<=b.z+.5f)model("primitive/box",{x,h,box.low.z-.3f},{8,10,.5f},0,game::rgb(65,89,102));
            }
            continue;
        }
        if(b.id.rfind("outpost-",0)==0){''')
edit('src/dx11_scene.cpp','void vehicles(){','''void drawRigidPart(Vec3 position,Vec3 size,Color tint,int shape,const RagdollPart& rotation);
void vehicles(){''')
edit('src/dx11_scene.cpp','        if(!close(v.p,650))continue;', '        if(v.exploded||!close(v.p,650))continue;')
edit('src/dx11_scene.cpp','        if(v.kind==game::Kind::Car||v.kind==game::Kind::SportCar){', '''        if(int(v.kind)>=5){
            RagdollPart rotation{};
            if(v.kind==game::Kind::Airplane){rotation.qx=v.qx;rotation.qy=v.qy;rotation.qz=v.qz;rotation.qw=v.qw;}
            else{rotation.qy=std::sin(yaw*.5f);rotation.qw=std::cos(yaw*.5f);}
            for(const auto& part:vehicle_systems::parts(v)){
                // Part centers and dimensions are shared with physical explosion fragments.
                Vec3 offset;
                Vec3 q{rotation.qx,rotation.qy,rotation.qz};
                Vec3 c{q.y*part.center.z-q.z*part.center.y,q.z*part.center.x-q.x*part.center.z,q.x*part.center.y-q.y*part.center.x};
                Vec3 d{q.y*c.z-q.z*c.y,q.z*c.x-q.x*c.z,q.x*c.y-q.y*c.x};
                offset=part.center+(c*rotation.qw+d)*2;
                drawRigidPart(Vec3{v.p.x,v.rideHeight,v.p.z}+offset,part.size,part.color,part.shape,rotation);
            }
            if(v.kind!=game::Kind::Trailer){
                float seat=v.kind==game::Kind::Skateboard?4:v.kind==game::Kind::Bicycle?11:v.kind==game::Kind::Tank?14:v.kind==game::Kind::Airplane?4:20;
                driver(v.p-facing*(v.kind==game::Kind::Truck?-31.0f:8.0f),seat,{14,34,14},v.kind==game::Kind::Bicycle);
            }
            continue;
        }
        if(v.kind==game::Kind::Car||v.kind==game::Kind::SportCar){''')
edit('src/dx11_scene.cpp','void ragdollMesh(const RagdollPart* bodies){', '''void drawRigidPart(Vec3 position,Vec3 size,Color tint,int shape,const RagdollPart& rotation){
    const Mesh* source=mesh(shape==2?"primitive/sphere":shape==1?"primitive/cylinder":"primitive/box");
    if(!source)return;
    Vec3 center{(source->minX+source->maxX)*.5f,(source->minY+source->maxY)*.5f,(source->minZ+source->maxZ)*.5f};
    Vec3 scale{size.x/(source->maxX-source->minX),size.y/(source->maxY-source->minY),size.z/(source->maxZ-source->minZ)};
    std::size_t count=source->indices.empty()?source->vertices.size():source->indices.size();
    for(std::size_t i=0;i<count;++i){auto v=source->vertices[source->indices.empty()?i:source->indices[i]];
        Vec3 local{(v.x-center.x)*scale.x,(v.y-center.y)*scale.y,(v.z-center.z)*scale.z};
        Vec3 p=position+rotateBy(rotation,local),n=game::norm(rotateBy(rotation,{v.nx/scale.x,v.ny/scale.y,v.nz/scale.z}));
        buckets[0].push_back(vertex(p,n,v.u,v.v,tint));
    }
}
void ragdollMesh(const RagdollPart* bodies){''')
edit('src/dx11_scene.cpp','        const Mesh* source=mesh(fragment.foliage?', '''        if(fragment.category){
            RagdollPart rotation{};rotation.qx=fragment.qx;rotation.qy=fragment.qy;rotation.qz=fragment.qz;rotation.qw=fragment.qw;
            drawRigidPart(fragment.p,fragment.size,fragment.color,fragment.shape,rotation);continue;
        }
        const Mesh* source=mesh(fragment.foliage?''')
edit('src/jolt_world.cpp','JPH::BodyLockWrite a(world->GetBodyLockInterface(),truckId)', 'JPH::BodyLockWrite a(world->GetBodyLockInterfaceNoLock(),truckId)')
edit('src/jolt_world.cpp','JPH::BodyLockWrite b(world->GetBodyLockInterface(),vehicleBodies[n])', 'JPH::BodyLockWrite b(world->GetBodyLockInterfaceNoLock(),vehicleBodies[n])')
edit('src/game.cpp','for(const auto& b:buildings)if(inside(b,point,12))return false;', 'for(const auto& b:buildings)if(destruction::contains(b,{point.x,v.rideHeight+17,point.z},12))return false;')
edit('src/renderer_dx11.cpp','bufferW=bufferW;RECT client{};', 'RECT client{};')
