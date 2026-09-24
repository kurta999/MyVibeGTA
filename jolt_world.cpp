#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include "jolt_world.h"
#include <memory>
#include <vector>

namespace jolt_world {
namespace {
namespace Layer {
constexpr JPH::ObjectLayer staticBody=0,moving=1;
}
class PairFilter final:public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a,JPH::ObjectLayer b) const override {
        return a==Layer::moving||b==Layer::moving;
    }
};
class BroadPhase final:public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override{return 2;}
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return JPH::BroadPhaseLayer(layer==Layer::staticBody?0:1);
    }
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer==JPH::BroadPhaseLayer(0)?"static":"moving";
    }
};
class BroadFilter final:public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a,JPH::BroadPhaseLayer b) const override {
        return a==Layer::moving||b==JPH::BroadPhaseLayer(1);
    }
};
PairFilter pairFilter;
BroadPhase broadPhase;
BroadFilter broadFilter;
std::unique_ptr<JPH::PhysicsSystem> world;
std::unique_ptr<JPH::TempAllocatorImpl> allocator;
std::unique_ptr<JPH::JobSystemSingleThreaded> jobs;
std::vector<JPH::BodyID> propBodies;
std::vector<JPH::BodyID> staticBodies;
struct Ragdoll {
    std::vector<JPH::BodyID> bodies;
    std::vector<JPH::Ref<JPH::TwoBodyConstraint>> joints;
    float life=6;
    int style=0;
    game::Vec3 origin{},rest[6]{};
    float yaw=0;
};
std::vector<Ragdoll> ragdolls;
bool registered=false;
void addStatic(game::Vec3 center,game::Vec3 half){
    auto& bodies=world->GetBodyInterface();
    JPH::BodyCreationSettings settings(new JPH::BoxShape(JPH::Vec3(half.x,half.y,half.z)),
        JPH::RVec3(center.x,center.y,center.z),JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,Layer::staticBody);
    auto id=bodies.CreateAndAddBody(settings,JPH::EActivation::DontActivate);
    if(!id.IsInvalid())staticBodies.push_back(id);
}
}
void shutdown(){
    if(world){
        auto& bodies=world->GetBodyInterface();
        for(auto& ragdoll:ragdolls){
            for(auto& joint:ragdoll.joints)world->RemoveConstraint(joint.GetPtr());
            for(auto id:ragdoll.bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
        }
        for(auto id:propBodies)if(!id.IsInvalid()){bodies.RemoveBody(id);bodies.DestroyBody(id);}
        for(auto id:staticBodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
    }
    ragdolls.clear();game::ragdollParts.clear();
    propBodies.clear();staticBodies.clear();world.reset();jobs.reset();allocator.reset();
}
void reset(){
    shutdown();
    if(!registered){
        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance=new JPH::Factory();
        JPH::RegisterTypes();registered=true;
    }
    allocator=std::make_unique<JPH::TempAllocatorImpl>(4*1024*1024);
    jobs=std::make_unique<JPH::JobSystemSingleThreaded>(JPH::cMaxPhysicsJobs);
    world=std::make_unique<JPH::PhysicsSystem>();
    world->Init(2048,0,4096,2048,broadPhase,broadFilter,pairFilter);
    world->SetGravity(JPH::Vec3(0,-700,0));
    addStatic({game::WORLD_W*0.5f,-5,game::WORLD_D*0.5f},
        {game::WORLD_W*0.5f,5,game::WORLD_D*0.5f});
    for(const auto& building:game::buildings)
        addStatic({building.x+building.w*0.5f,building.h*0.5f,building.z+building.d*0.5f},
            {building.w*0.5f,building.h*0.5f,building.d*0.5f});
    auto& bodies=world->GetBodyInterface();
    for(const auto& prop:game::props){
        float half=prop.barrel?9.5f:11.5f;
        float halfHeight=prop.barrel?11.5f:11.0f;
        JPH::BodyCreationSettings settings(new JPH::BoxShape(JPH::Vec3(half,halfHeight,half)),
            JPH::RVec3(prop.p.x,prop.y+halfHeight,prop.p.z),JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,Layer::moving);
        settings.mFriction=0.85f;settings.mRestitution=0.12f;
        settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass=40.0f;
        auto id=bodies.CreateAndAddBody(settings,JPH::EActivation::Activate);
        propBodies.push_back(id);
    }
    world->OptimizeBroadPhase();
}
void impulse(std::size_t index,game::Vec3 value){
    if(!world||index>=propBodies.size()||propBodies[index].IsInvalid())return;
    world->GetBodyInterface().AddImpulse(propBodies[index],JPH::Vec3(value.x*40,value.y*40,value.z*40));
}
void remove(std::size_t index){
    if(!world||index>=propBodies.size()||propBodies[index].IsInvalid())return;
    auto& bodies=world->GetBodyInterface();
    bodies.RemoveBody(propBodies[index]);bodies.DestroyBody(propBodies[index]);
    propBodies[index]=JPH::BodyID();
}
void spawnRagdoll(const game::Ped& ped,game::Vec3 impulse){
    if(!world)return;
    if(ragdolls.size()>=12){
        auto& old=ragdolls.front();auto& bodies=world->GetBodyInterface();
        for(auto& joint:old.joints)world->RemoveConstraint(joint.GetPtr());
        for(auto id:old.bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
        ragdolls.erase(ragdolls.begin());
    }
    struct Part {float x,y,z,w,h,d;};
    const Part parts[]={
        {0,25,0,10,16,7}, {0,37,0,8,8,8},
        {-8,24,0,4,13,4}, {8,24,0,4,13,4},
        {-3,10,0,5,16,5}, {3,10,0,5,16,5}};
    Ragdoll ragdoll;ragdoll.style=ped.style;
    ragdoll.origin={ped.p.x,0,ped.p.z};ragdoll.yaw=game::PI/2-ped.angle;
    float co=std::cos(ragdoll.yaw),si=std::sin(ragdoll.yaw);
    auto rotated=[&](float x,float z){return game::Vec2{co*x+si*z,-si*x+co*z};};
    auto& bodies=world->GetBodyInterface();
    JPH::Body* created[6]{};
    for(int i=0;i<6;++i){
        const auto& part=parts[i];
        game::Vec2 offset=rotated(part.x,part.z);
        ragdoll.rest[i]={ped.p.x+offset.x,part.y,ped.p.z+offset.z};
        JPH::BodyCreationSettings settings(new JPH::BoxShape(JPH::Vec3(part.w/2,part.h/2,part.d/2)),
            JPH::RVec3(ragdoll.rest[i].x,part.y,ragdoll.rest[i].z),JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,Layer::moving);
        settings.mFriction=0.75f;settings.mRestitution=0.08f;
        settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass=i==0?18.0f:4.0f;
        created[i]=bodies.CreateBody(settings);
        if(!created[i])return;
        bodies.AddBody(created[i]->GetID(),JPH::EActivation::Activate);
        ragdoll.bodies.push_back(created[i]->GetID());
        float scale=i==0?1.15f:0.12f;
        bodies.AddImpulse(created[i]->GetID(),JPH::Vec3(impulse.x*scale,
            std::max(12.0f,impulse.y*0.2f),impulse.z*scale));
    }
    const int parent[]={0,0,0,0,0};
    const int child[]={1,2,3,4,5};
    const game::Vec3 anchors[]={
        {0,33,0},{-5,30,0},{5,30,0},{-3,17,0},{3,17,0}};
    for(int i=0;i<5;++i){
        JPH::DistanceConstraintSettings settings;
        game::Vec2 anchor=rotated(anchors[i].x,anchors[i].z);
        settings.mPoint1=settings.mPoint2=JPH::RVec3(ped.p.x+anchor.x,
            anchors[i].y,ped.p.z+anchor.z);
        settings.mMinDistance=0;settings.mMaxDistance=0.5f;
        JPH::Ref<JPH::TwoBodyConstraint> joint=settings.Create(*created[parent[i]],*created[child[i]]);
        world->AddConstraint(joint.GetPtr());ragdoll.joints.push_back(joint);
    }
    ragdolls.push_back(std::move(ragdoll));
}
void step(float dt){
    if(!world)return;
    world->Update(dt,1,allocator.get(),jobs.get());
    auto& bodies=world->GetBodyInterface();
    for(std::size_t i=0;i<propBodies.size()&&i<game::props.size();++i){
        if(propBodies[i].IsInvalid())continue;
        auto position=bodies.GetCenterOfMassPosition(propBodies[i]);
        auto velocity=bodies.GetLinearVelocity(propBodies[i]);
        auto& prop=game::props[i];
        prop.p={position.GetX(),position.GetZ()};
        prop.y=position.GetY()-(prop.barrel?11.5f:11.0f);
        prop.v={velocity.GetX(),velocity.GetZ()};prop.vy=velocity.GetY();
    }
    game::ragdollParts.clear();
    for(auto it=ragdolls.begin();it!=ragdolls.end();){
        it->life-=dt;
        if(it->life<=0){
            for(auto& joint:it->joints)world->RemoveConstraint(joint.GetPtr());
            for(auto id:it->bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
            it=ragdolls.erase(it);continue;
        }
        for(std::size_t i=0;i<it->bodies.size();++i){
            auto p=bodies.GetCenterOfMassPosition(it->bodies[i]);
            auto q=bodies.GetRotation(it->bodies[i]);
            game::ragdollParts.push_back({{p.GetX(),p.GetY(),p.GetZ()},it->rest[i],
                it->origin,q.GetX(),q.GetY(),q.GetZ(),q.GetW(),it->yaw,it->style,int(i)});
        }
        ++it;
    }
}
}
