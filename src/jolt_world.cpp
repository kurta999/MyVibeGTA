#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include "masonry.h"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include "destruction.h"
#include "vehicle_systems.h"
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include "jolt_world.h"
#include "regions.h"
#include "physics.h"
#include "ai.h"
#include "wildlife.h"
#include <algorithm>
#include <cmath>
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
std::vector<JPH::BodyID> vehicleBodies;
std::vector<JPH::BodyID> treeBodies,animalBodies;
std::vector<int> animalBodySpecies;
constexpr JPH::uint64 treeTag=JPH::uint64(1)<<32,animalTag=JPH::uint64(2)<<32;
struct SceneryImpact {float speed=0;int vehicle=-1;game::Vec2 direction{};};
std::vector<SceneryImpact> treeImpacts,animalImpacts;
struct FragmentBody {JPH::BodyID id;TreeFragment visual;float life=12;};
std::vector<FragmentBody> fragmentBodies;
std::vector<TreeFragment> fragmentVisuals;
void destroyBody(JPH::BodyID& id);
void syncSceneryColliders(game::Vec2 focus,float dt=0);
// The physics jobs are single threaded. Queue contacts and apply gameplay
// damage after Update, when the physics world is no longer locked.
struct VehicleImpact {float speed=0;bool playerCaused=false;};
std::vector<VehicleImpact> vehicleImpacts;
class TrafficContacts final:public JPH::ContactListener {
    void record(const JPH::Body& a,const JPH::Body& b,const JPH::ContactManifold& contact){
        // Tire grip, steering and brakes can change velocity sharply without
        // hitting anything. Only closing speed along a real contact normal is
        // an impact; tangential sliding and separation must not cause damage.
        JPH::RVec3 point=contact.mRelativeContactPointsOn1.empty()?
            a.GetCenterOfMassPosition():contact.GetWorldSpaceContactPointOn1(0);
        float speed=(a.GetPointVelocity(point)-b.GetPointVelocity(point)).Dot(contact.mWorldSpaceNormal);
        if(speed<=12)return;
        int first=-1,second=-1;
        for(int i=0;i<int(vehicleBodies.size());++i){
            if(vehicleBodies[i]==a.GetID())first=i;
            if(vehicleBodies[i]==b.GetID())second=i;
        }
        auto collect=[&](int index,int other){
            if(index<0||index>=int(vehicleImpacts.size()))return;
            auto& impact=vehicleImpacts[index];
            impact.speed=std::max(impact.speed,speed);
            if(other>=0&&other==game::occupied&&other<int(game::vehicles.size())&&
               std::abs(game::vehicles[other].speed)>=10)impact.playerCaused=true;
        };
        collect(first,second);collect(second,first);
        auto scenery=[&](const JPH::Body& target,int vehicle){
            if(vehicle<0)return;
            auto tag=target.GetUserData()&0xffffffff00000000ULL;
            auto index=std::size_t(target.GetUserData()&0xffffffffULL);
            auto* impacts=tag==treeTag?&treeImpacts:tag==animalTag?&animalImpacts:nullptr;
            if(!impacts||index>=impacts->size()||speed<=(*impacts)[index].speed)return;
            auto v=(vehicleBodies[vehicle]==a.GetID()?a:b).GetLinearVelocity();
            (*impacts)[index]={speed,vehicle,game::norm(game::Vec2{v.GetX(),v.GetZ()})};
        };
        scenery(a,second);scenery(b,first);
    }
public:
    void OnContactAdded(const JPH::Body& a,const JPH::Body& b,
        const JPH::ContactManifold& contact,JPH::ContactSettings&) override{record(a,b,contact);}
    void OnContactPersisted(const JPH::Body& a,const JPH::Body& b,
        const JPH::ContactManifold& contact,JPH::ContactSettings&) override{record(a,b,contact);}
};
TrafficContacts trafficContacts;
std::vector<JPH::Ref<JPH::VehicleConstraint>> vehicleConstraints;
struct Hitch {std::size_t truck,trailer;JPH::Ref<JPH::TwoBodyConstraint> joint;};
std::vector<Hitch> hitches;
std::vector<game::Vec2> vehicleSynced;
JPH::Ref<JPH::CharacterVirtual> playerCharacter;
JPH::Ref<JPH::CapsuleShape> standingShape,crouchingShape;
bool playerCrouched=false;
JPH::Ref<JPH::CharacterVirtualSettings> pedestrianSettings;
std::vector<JPH::Ref<JPH::CharacterVirtual>> pedCharacters;
std::vector<JPH::BodyID> staticBodies;
std::vector<JPH::BodyID> buildingBodies;
int streamedCellX=-1,streamedCellZ=-1;
struct Ragdoll {
    std::string pedId;
    std::vector<JPH::BodyID> bodies;
    std::vector<JPH::Ref<JPH::TwoBodyConstraint>> joints;
    float life=6;
    int style=0;
    game::Vec3 origin{},rest[6]{};
    float yaw=0;
};
std::vector<Ragdoll> ragdolls;
bool registered=false;
void captureCorpsePose(const Ragdoll& ragdoll){
    if(ragdoll.bodies.size()!=6)return;
    auto ped=std::find_if(game::peds.begin(),game::peds.end(),
        [&](const game::Ped& candidate){return candidate.id==ragdoll.pedId;});
    if(ped==game::peds.end()||ped->alive||ped->carried)return;
    game::CorpseSnapshot snapshot{};
    snapshot.pedId=ragdoll.pedId;
    auto& bodies=world->GetBodyInterface();
    for(std::size_t i=0;i<snapshot.parts.size();++i){
        auto p=bodies.GetCenterOfMassPosition(ragdoll.bodies[i]);
        auto q=bodies.GetRotation(ragdoll.bodies[i]);
        snapshot.parts[i]={{p.GetX(),p.GetY(),p.GetZ()},ragdoll.rest[i],
            ragdoll.origin,q.GetX(),q.GetY(),q.GetZ(),q.GetW(),
            ragdoll.yaw,ragdoll.style,int(i)};
    }
    ped->p={snapshot.parts[0].p.x,snapshot.parts[0].p.z};
    ped->pinned=false;
    ped->corpseVisualDelay=ped->respawn;
    game::corpseSnapshots.erase(std::remove_if(game::corpseSnapshots.begin(),
        game::corpseSnapshots.end(),[&](const game::CorpseSnapshot& old){
            return old.pedId==snapshot.pedId;
        }),game::corpseSnapshots.end());
    game::corpseSnapshots.push_back(std::move(snapshot));
    if(game::corpseSnapshots.size()>24){
        const std::string evicted=game::corpseSnapshots.front().pedId;
        for(auto& other:game::peds)if(other.id==evicted&&!other.alive)
            other.corpseVisualDelay=0;
        game::corpseSnapshots.erase(game::corpseSnapshots.begin());
    }
}
JPH::BodyID createStatic(game::Vec3 center,game::Vec3 half){
    auto& bodies=world->GetBodyInterface();
    JPH::BodyCreationSettings settings(new JPH::BoxShape(JPH::Vec3(half.x,half.y,half.z)),
        JPH::RVec3(center.x,center.y,center.z),JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,Layer::staticBody);
    settings.mFriction=0.9f;
    return bodies.CreateAndAddBody(settings,JPH::EActivation::DontActivate);
}
void addStatic(game::Vec3 center,game::Vec3 half){
    auto id=createStatic(center,half);
    if(!id.IsInvalid())staticBodies.push_back(id);
}
JPH::BodyID createBuilding(const game::Building& b){
    if(!b.damaged)return createStatic({b.x+b.w*.5f,b.h*.5f,b.z+b.d*.5f},{b.w*.5f,b.h*.5f,b.d*.5f});
    JPH::StaticCompoundShapeSettings compound;
    for(const auto& piece:destruction::boxes(b)){
        auto center=(piece.low+piece.high)*.5f,half=(piece.high-piece.low)*.5f;
        compound.AddShape(JPH::Vec3(center.x,center.y,center.z),JPH::Quat::sIdentity(),
            new JPH::BoxShape(JPH::Vec3(half.x,half.y,half.z),0.1f));
    }
    if(compound.mSubShapes.empty())return {};
    auto shape=compound.Create();if(shape.HasError())return {};
    JPH::BodyCreationSettings setting(shape.Get(),JPH::RVec3::sZero(),JPH::Quat::sIdentity(),JPH::EMotionType::Static,Layer::staticBody);
    setting.mFriction=.9f;
    return world->GetBodyInterface().CreateAndAddBody(setting,JPH::EActivation::DontActivate);
}
void syncBuildingColliders(game::Vec2 focus){
    if(!world)return;
    auto& bodies=world->GetBodyInterface();
    if(buildingBodies.size()!=game::buildings.size()){
        for(auto id:buildingBodies)if(!id.IsInvalid()){
            bodies.RemoveBody(id);bodies.DestroyBody(id);
        }
        buildingBodies.assign(game::buildings.size(),JPH::BodyID());
        streamedCellX=streamedCellZ=-1;
    }
    int cellX=int(std::floor(focus.x/400)),cellZ=int(std::floor(focus.z/400));
    if(cellX==streamedCellX&&cellZ==streamedCellZ)return;
    streamedCellX=cellX;streamedCellZ=cellZ;
    for(std::size_t index=0;index<pedCharacters.size()&&index<game::peds.size();++index)
        if(!game::peds[index].alive||game::len(game::peds[index].p-focus)>650)
            pedCharacters[index]=nullptr;
    constexpr float radius=1200;
    for(std::size_t index=0;index<game::buildings.size();++index){
        const auto& building=game::buildings[index];
        float dx=std::max({building.x-focus.x,0.0f,
            focus.x-(building.x+building.w)});
        float dz=std::max({building.z-focus.z,0.0f,
            focus.z-(building.z+building.d)});
        bool nearby=dx*dx+dz*dz<radius*radius;
        auto& id=buildingBodies[index];
        if(nearby&&id.IsInvalid())
            id=createBuilding(building);
        else if(!nearby&&!id.IsInvalid()){
            bodies.RemoveBody(id);bodies.DestroyBody(id);id=JPH::BodyID();
        }
    }
}
JPH::Ref<JPH::CharacterVirtual> makePedCharacter(game::Vec2 point){
    if(!world||!pedestrianSettings)return nullptr;
    return new JPH::CharacterVirtual(pedestrianSettings,
        JPH::RVec3(point.x,0,point.z),JPH::Quat::sIdentity(),
        Layer::moving,world.get());
}
}
void shutdown(){
    if(world){
        auto& bodies=world->GetBodyInterface();
        playerCharacter=nullptr;
        pedCharacters.clear();
        pedestrianSettings=nullptr;
        for(auto& hitch:hitches)world->RemoveConstraint(hitch.joint.GetPtr());
        hitches.clear();
        for(auto& constraint:vehicleConstraints)if(constraint){
            world->RemoveStepListener(constraint.GetPtr());
            world->RemoveConstraint(constraint.GetPtr());
        }
        for(auto id:vehicleBodies)if(!id.IsInvalid()){bodies.RemoveBody(id);bodies.DestroyBody(id);}
        for(auto& ragdoll:ragdolls){
            for(auto& joint:ragdoll.joints)world->RemoveConstraint(joint.GetPtr());
            for(auto id:ragdoll.bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
        }
        for(auto id:propBodies)if(!id.IsInvalid()){bodies.RemoveBody(id);bodies.DestroyBody(id);}
        for(auto id:staticBodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
        for(auto id:buildingBodies)if(!id.IsInvalid()){
            bodies.RemoveBody(id);bodies.DestroyBody(id);
        }
    }
    if(world){
        for(auto& id:treeBodies)destroyBody(id);
        for(auto& id:animalBodies)destroyBody(id);
        for(auto& fragment:fragmentBodies)destroyBody(fragment.id);
    }
    ragdolls.clear();game::ragdollParts.clear();game::corpseSnapshots.clear();
    propBodies.clear();vehicleBodies.clear();vehicleConstraints.clear();vehicleSynced.clear();
    staticBodies.clear();buildingBodies.clear();streamedCellX=streamedCellZ=-1;
    treeBodies.clear();animalBodies.clear();animalBodySpecies.clear();
    treeImpacts.clear();animalImpacts.clear();fragmentBodies.clear();fragmentVisuals.clear();
    world.reset();jobs.reset();allocator.reset();
    standingShape=nullptr;crouchingShape=nullptr;playerCrouched=false;
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
    world->Init(4096,0,8192,4096,broadPhase,broadFilter,pairFilter);
    world->SetContactListener(&trafficContacts);
    world->SetGravity(JPH::Vec3(0,-700,0));
    addStatic({game::WORLD_W*0.5f,-5,game::SHORE*0.5f},
        {game::WORLD_W*0.5f,5,game::SHORE*0.5f});
    addStatic({game::WORLD_W*0.5f,-20,(game::SHORE+game::WORLD_D)*0.5f},
        {game::WORLD_W*0.5f,5,(game::WORLD_D-game::SHORE)*0.5f});
    addStatic({1200,-5,(game::SHORE+game::WORLD_D)*0.5f},
        {60,5,(game::WORLD_D-game::SHORE)*0.5f});
    addStatic({game::WORLD_W*0.5f,-5,(game::WORLD_D+regions::DEPTH)*0.5f},
        {game::WORLD_W*0.5f,5,(regions::DEPTH-game::WORLD_D)*0.5f});
    addStatic({(game::WORLD_W+7600)*0.5f,-5,regions::DEPTH*0.5f},
        {(7600-game::WORLD_W)*0.5f,5,regions::DEPTH*0.5f});
    addStatic({7800,-20,regions::DEPTH*0.5f},{200,5,regions::DEPTH*0.5f});
    addStatic({(8000+regions::WIDTH)*0.5f,-5,regions::DEPTH*0.5f},
        {(regions::WIDTH-8000)*0.5f,5,regions::DEPTH*0.5f});
    addStatic({7800,-5,8500},{200,5,100});
    // Vehicles need the same world limit as the character. Without a collider
    // they can leave the physics floor and lose all wheel contact.
    constexpr float border=10.0f;
    addStatic({-border,95,regions::DEPTH*0.5f},
        {border,95,regions::DEPTH*0.5f+border});
    addStatic({regions::WIDTH+border,95,regions::DEPTH*0.5f},
        {border,95,regions::DEPTH*0.5f+border});
    addStatic({regions::WIDTH*0.5f,95,-border},
        {regions::WIDTH*0.5f+border,95,border});
    addStatic({regions::WIDTH*0.5f,95,regions::DEPTH+border},
        {regions::WIDTH*0.5f+border,95,border});
    syncBuildingColliders(game::player);
    syncSceneryColliders(game::player);
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
    for(const auto& vehicle:game::vehicles){
        const std::size_t vehicleIndex=vehicleBodies.size();
        const auto tuning=physics::tuning(vehicle.kind);
        bool boat=vehicle.kind==game::Kind::Boat;
        bool helicopter=vehicle.kind==game::Kind::Helicopter;
        bool bike=vehicle.kind==game::Kind::Bike||vehicle.kind==game::Kind::Bicycle;
        bool plane=vehicle.kind==game::Kind::Airplane;
        float scale=physics::vehicleScale(vehicle.kind);
        auto half=physics::chassisHalf(vehicle.kind);
        float halfW=half.x,halfH=half.y,halfL=half.z;
        float mass=tuning.mass;
        JPH::BodyCreationSettings settings(new JPH::BoxShape(JPH::Vec3(halfW,halfH,halfL)),
            JPH::RVec3(vehicle.p.x,physics::vehicleRestHeight(vehicle.kind),vehicle.p.z),
            JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-vehicle.angle),
            JPH::EMotionType::Dynamic,Layer::moving);
        auto& initialPose=game::vehicles[vehicleIndex];
        initialPose.qx=settings.mRotation.GetX();initialPose.qy=settings.mRotation.GetY();
        initialPose.qz=settings.mRotation.GetZ();initialPose.qw=settings.mRotation.GetW();
        // Wheels provide ground contact; the chassis should not drag on the road.
        settings.mFriction=helicopter?0.8f:0.02f;settings.mRestitution=0.04f;
        if(helicopter)settings.mAllowedDOFs=JPH::EAllowedDOFs::TranslationX|JPH::EAllowedDOFs::TranslationY|JPH::EAllowedDOFs::TranslationZ|JPH::EAllowedDOFs::RotationY;
        settings.mMotionQuality=JPH::EMotionQuality::LinearCast;
        settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass=mass;
        JPH::Body* body=bodies.CreateBody(settings);
        auto id=body?body->GetID():JPH::BodyID();
        if(body)bodies.AddBody(id,JPH::EActivation::Activate);
        vehicleBodies.push_back(id);vehicleSynced.push_back(vehicle.p);
        JPH::Ref<JPH::VehicleConstraint> constraint;
        if(body&&!boat&&!helicopter){
            JPH::VehicleConstraintSettings wheels;
            wheels.mMaxPitchRollAngle=plane?game::PI:bike?0.48f:0.62f;
            float radius=tuning.wheelRadius*scale;
            float width=(bike?2.5f:3.5f)*scale;
            float wheelX=plane?11.0f:bike?2.7f:halfW*0.78f;
            float wheelZ=plane?27.0f:bike?halfL*0.72f:halfL*0.76f;
            for(int axle=0;axle<2;++axle)for(int side=0;side<2;++side){
                JPH::WheelSettingsWV* wheel=new JPH::WheelSettingsWV();
                wheel->mPosition=JPH::Vec3(side==0?wheelX:-wheelX,
                    -halfH*0.85f,axle==0?wheelZ:-wheelZ);
                wheel->mRadius=radius;wheel->mWidth=width;
                wheel->mSuspensionMinLength=vehicle.kind==game::Kind::Skateboard?0.3f:1.5f*scale;
                wheel->mSuspensionMaxLength=vehicle.kind==game::Kind::Skateboard?1.0f:(bike?5.0f:5.5f)*scale;
                wheel->mSuspensionSpring.mFrequency=tuning.suspensionFrequency;
                wheel->mSuspensionSpring.mDamping=tuning.suspensionDamping;
                // Preserve the turning radius with the longer wheelbase.
                wheel->mMaxSteerAngle=axle==0?std::atan(std::tan(tuning.steerAngle)*scale):0.0f;
                wheel->mMaxHandBrakeTorque=axle==0?0.0f:tuning.handbrakeTorque*scale;
                wheel->mMaxBrakeTorque=tuning.brakeTorque*scale;
                wheel->mInertia=(bike?65.0f:220.0f)*scale*scale;
                wheels.mWheels.push_back(wheel);
            }
            JPH::WheeledVehicleControllerSettings* controller=
                new JPH::WheeledVehicleControllerSettings();
            controller->mEngine.mMaxTorque=tuning.engineTorque*scale;
            controller->mEngine.mInertia=bike?15.0f:60.0f;
            controller->mTransmission.mClutchStrength=bike?1200.0f:3500.0f;
            controller->mTransmission.mSwitchTime=0.16f;
            controller->mTransmission.mClutchReleaseTime=0.15f;
            controller->mDifferentials.resize(vehicle.kind==game::Kind::SportCar?2:1);
            controller->mDifferentials[0].mLeftWheel=bike?2:0;
            controller->mDifferentials[0].mRightWheel=bike?3:1;
            if(vehicle.kind==game::Kind::SportCar){
                controller->mDifferentials[1].mLeftWheel=2;
                controller->mDifferentials[1].mRightWheel=3;
                controller->mDifferentials[0].mEngineTorqueRatio=0.5f;
                controller->mDifferentials[1].mEngineTorqueRatio=0.5f;
            }
            wheels.mController=controller;
            if(!bike){
                wheels.mAntiRollBars.resize(2);
                wheels.mAntiRollBars[0].mLeftWheel=0;
                wheels.mAntiRollBars[0].mRightWheel=1;
                wheels.mAntiRollBars[1].mLeftWheel=2;
                wheels.mAntiRollBars[1].mRightWheel=3;
            }
            constraint=new JPH::VehicleConstraint(*body,wheels);
            auto* activeController=static_cast<JPH::WheeledVehicleController*>(
                constraint->GetController());
            activeController->SetTireMaxImpulseCallback(
                [vehicleIndex](JPH::uint,float& longitudinalImpulse,
                    float& lateralImpulse,float suspensionImpulse,
                    float longitudinalFriction,float lateralFriction,
                    float,float,float){
                    float grip=vehicleIndex<game::vehicles.size()?
                        physics::tractionAt(game::vehicles[vehicleIndex].p):1.0f;
                    longitudinalImpulse=longitudinalFriction*suspensionImpulse*grip;
                    lateralImpulse=lateralFriction*suspensionImpulse*grip;
                });
            constraint->SetVehicleCollisionTester(new JPH::VehicleCollisionTesterRay(Layer::moving));
            world->AddConstraint(constraint.GetPtr());
            world->AddStepListener(constraint.GetPtr());
        }
        vehicleConstraints.push_back(constraint);
    }
    JPH::Ref<JPH::CharacterVirtualSettings> characterSettings=new JPH::CharacterVirtualSettings();
    standingShape=new JPH::CapsuleShape(8.0f,10.0f);
    crouchingShape=new JPH::CapsuleShape(3.0f,10.0f);
    characterSettings->mShape=standingShape.GetPtr();
    characterSettings->mShapeOffset=JPH::Vec3(0,18,0);
    characterSettings->mMaxSlopeAngle=game::PI*0.28f;
    characterSettings->mSupportingVolume=JPH::Plane(JPH::Vec3::sAxisY(),-10.0f);
    characterSettings->mMaxStrength=250.0f;
    playerCharacter=new JPH::CharacterVirtual(characterSettings,
        JPH::RVec3(game::player.x,game::playerY,game::player.z),
        JPH::Quat::sIdentity(),0,world.get());
    pedestrianSettings=new JPH::CharacterVirtualSettings();
    pedestrianSettings->mShape=new JPH::CapsuleShape(7.0f,8.0f);
    pedestrianSettings->mShapeOffset=JPH::Vec3(0,16,0);
    pedestrianSettings->mMaxSlopeAngle=game::PI*0.28f;
    pedestrianSettings->mSupportingVolume=JPH::Plane(JPH::Vec3::sAxisY(),-9.0f);
    // Virtual pedestrians should not push back on a much heavier chassis.
    pedestrianSettings->mMaxStrength=0.0f;
    pedCharacters.resize(game::peds.size());
    for(std::size_t index=0;index<game::peds.size();++index)
        if(game::peds[index].alive&&game::peds[index].drivingVehicle<0&&game::len(game::peds[index].p-game::player)<500)
            pedCharacters[index]=makePedCharacter(game::peds[index].p);
    world->OptimizeBroadPhase();
}
void addPed(){
    if(!world||pedCharacters.size()>=game::peds.size())return;
    std::size_t previous=pedCharacters.size();
    pedCharacters.resize(game::peds.size());
    for(std::size_t index=previous;index<game::peds.size();++index)
        if(game::peds[index].alive&&game::peds[index].drivingVehicle<0&&game::len(game::peds[index].p-game::player)<500)
            pedCharacters[index]=makePedCharacter(game::peds[index].p);
}
void moveCharacter(game::Vec2 horizontal,bool jump,float dt){
    if(!playerCharacter||!world)return;
    syncSceneryColliders(game::player);
    game::Vec2 before=game::player;
    if(game::crouched!=playerCrouched){
        float oldOffset=playerCrouched?13.0f:18.0f;
        playerCharacter->SetShapeOffset(JPH::Vec3(0,game::crouched?13.0f:18.0f,0));
        const JPH::Shape* target=game::crouched?
            static_cast<const JPH::Shape*>(crouchingShape.GetPtr()):standingShape.GetPtr();
        if(playerCharacter->SetShape(target,0.1f,
            world->GetDefaultBroadPhaseLayerFilter(Layer::moving),
            world->GetDefaultLayerFilter(Layer::moving),{}, {},*allocator))
            playerCrouched=game::crouched;
        else{
            playerCharacter->SetShapeOffset(JPH::Vec3(0,oldOffset,0));
            game::crouched=playerCrouched;
        }
    }
    // The terrain outside the region is only a visual skirt. Keep the virtual
    // character over the physics floor, including when loading an old save
    // made after stepping off the edge.
    constexpr float edge=12.0f;
    game::Vec2 playable{
        std::clamp(game::player.x,edge,regions::WIDTH-edge),
        std::clamp(game::player.z,edge,regions::DEPTH-edge)};
    if(playable.x!=game::player.x||playable.z!=game::player.z){
        game::player=playable;
        game::playerY=std::max(0.0f,game::playerY);
        playerCharacter->SetPosition(JPH::RVec3(playable.x,game::playerY,playable.z));
        playerCharacter->SetLinearVelocity(JPH::Vec3::sZero());
    }
    syncBuildingColliders(game::player);
    auto position=playerCharacter->GetPosition();
    if(game::len(game::Vec2{float(position.GetX()),float(position.GetZ())}-game::player)>8||
       std::abs(float(position.GetY())-game::playerY)>45)
        playerCharacter->SetPosition(JPH::RVec3(game::player.x,game::playerY,game::player.z));
    position=playerCharacter->GetPosition();
    game::Vec2 nextHorizontal=game::player+horizontal*dt;
    bool nextWater=regions::waterAt(nextHorizontal);
    bool swimming=position.GetY()<10&&(nextWater||
        (position.GetY()<-2&&regions::waterAt(game::player)));
    bool supported=playerCharacter->IsSupported();
    float vertical=playerCharacter->GetLinearVelocity().GetY();
    if(swimming){
        // The district's flat ground body extends under the water. Swim against
        // a bounded water depth instead, retaining the chosen depth on release.
        vertical=(float(game::rightMouse)-float(game::leftMouse))*48.0f;
        float y=std::clamp(float(position.GetY())+vertical*dt,-80.0f,0.0f);
        if((y>=0&&vertical>0)||(y<=-80&&vertical<0))vertical=0;
        if(!nextWater)nextHorizontal=game::player;
        nextHorizontal.x=std::clamp(nextHorizontal.x,edge,regions::WIDTH-edge);
        nextHorizontal.z=std::clamp(nextHorizontal.z,edge,regions::DEPTH-edge);
        playerCharacter->SetPosition(JPH::RVec3(nextHorizontal.x,y,nextHorizontal.z));
        playerCharacter->SetLinearVelocity(JPH::Vec3(horizontal.x,vertical,horizontal.z));
        game::player=nextHorizontal;game::playerY=y;game::playerVerticalSpeed=vertical;
        game::grounded=false;game::swimming=true;return;
    }
    else{
        if(supported&&vertical<0)vertical=0;
        if(jump&&supported)vertical=230;
        vertical+=world->GetGravity().GetY()*dt;
    }
    playerCharacter->SetLinearVelocity(JPH::Vec3(horizontal.x,vertical,horizontal.z));
    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    settings.mWalkStairsStepUp=JPH::Vec3(0,5,0);
    settings.mStickToFloorStepDown=JPH::Vec3(0,-5,0);
    playerCharacter->ExtendedUpdate(dt,swimming?JPH::Vec3::sZero():world->GetGravity(),settings,
        world->GetDefaultBroadPhaseLayerFilter(Layer::moving),
        world->GetDefaultLayerFilter(Layer::moving),{}, {},*allocator);
    position=playerCharacter->GetPosition();
    float safeX=std::clamp(float(position.GetX()),edge,regions::WIDTH-edge);
    float safeZ=std::clamp(float(position.GetZ()),edge,regions::DEPTH-edge);
    if(safeX!=float(position.GetX())||safeZ!=float(position.GetZ())){
        playerCharacter->SetPosition(JPH::RVec3(safeX,std::max(0.0f,float(position.GetY())),safeZ));
        position=playerCharacter->GetPosition();
    }
    if(game::playerY<25&&!swimming&&game::occupied<0){
        game::Vec2 candidate{float(position.GetX()),float(position.GetZ())};
        for(auto& ped:game::peds){
            if(!ped.alive||ped.drivingVehicle>=0||
               std::abs(ped.p.x-candidate.x)>19||
               std::abs(ped.p.z-candidate.z)>19||
               game::len(candidate-ped.p)>=18.0f)continue;
            float previousDistance=game::len(before-ped.p);
            if(previousDistance<18.0f&&
               game::len(candidate-ped.p)>=previousDistance)continue;
            game::Vec2 away=game::norm(before-ped.p);
            if(game::len(away)<0.01f)away=game::norm(candidate-ped.p);
            if(game::len(away)<0.01f)away=game::forward(ped.angle);
            candidate=previousDistance<18.0f?before:ped.p+away*18.0f;
            ai::pedestrianContact(ped,game::len(horizontal),candidate);
        }
        if(game::len(candidate-game::Vec2{float(position.GetX()),float(position.GetZ())})>0.001f){
            playerCharacter->SetPosition(JPH::RVec3(candidate.x,position.GetY(),candidate.z));
            JPH::Vec3 velocity=playerCharacter->GetLinearVelocity();
            playerCharacter->SetLinearVelocity(JPH::Vec3(0,velocity.GetY(),0));
            position=playerCharacter->GetPosition();
        }
    }
    game::player={float(position.GetX()),float(position.GetZ())};
    game::playerY=swimming?float(position.GetY()):std::max(0.0f,float(position.GetY()));
    game::playerVerticalSpeed=playerCharacter->GetLinearVelocity().GetY();
    game::grounded=!swimming&&playerCharacter->IsSupported();
    game::swimming=swimming;
    if(!swimming&&!supported&&game::grounded&&vertical<-380)
        game::applyDamage((std::abs(vertical)-380)*0.06f);
}
bool staticAnchor(game::Vec3 origin,game::Vec3 direction,float range,game::Vec3& point){
    if(!world||range<=0)return false;
    syncBuildingColliders({origin.x,origin.z});syncSceneryColliders({origin.x,origin.z});
    JPH::RRayCast ray(JPH::RVec3(origin.x,origin.y,origin.z),
        JPH::Vec3(direction.x*range,direction.y*range,direction.z*range));
    JPH::RayCastResult hit;
    if(!world->GetNarrowPhaseQuery().CastRay(ray,hit))return false;
    JPH::BodyLockRead lock(world->GetBodyLockInterface(),hit.mBodyID);
    if(!lock.Succeeded()||!lock.GetBody().IsStatic())return false;
    auto position=ray.GetPointOnRay(hit.mFraction);
    point={float(position.GetX()),float(position.GetY()),float(position.GetZ())};
    return point.y>5; // Ground and water do not provide traversal anchors.
}
void moveGrappleCharacter(game::Vec3 velocity,float dt){
    if(!world||!playerCharacter)return;
    syncBuildingColliders(game::player);syncSceneryColliders(game::player);
    playerCharacter->SetLinearVelocity(JPH::Vec3(velocity.x,velocity.y,velocity.z));
    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    settings.mWalkStairsStepUp=JPH::Vec3::sZero();
    settings.mStickToFloorStepDown=JPH::Vec3::sZero();
    playerCharacter->ExtendedUpdate(dt,JPH::Vec3::sZero(),settings,
        world->GetDefaultBroadPhaseLayerFilter(Layer::moving),
        world->GetDefaultLayerFilter(Layer::moving),{}, {},*allocator);
    const auto position=playerCharacter->GetPosition();
    game::player={float(position.GetX()),float(position.GetZ())};
    game::playerY=std::max(0.0f,float(position.GetY()));
    game::playerVerticalSpeed=playerCharacter->GetLinearVelocity().GetY();
    game::grounded=playerCharacter->IsSupported();game::swimming=false;
}
void teleportCharacter(game::Vec2 position,float height){
    if(!playerCharacter)return;
    syncBuildingColliders(position);
    syncSceneryColliders(position);
    playerCharacter->SetPosition(JPH::RVec3(position.x,height,position.z));
    playerCharacter->SetLinearVelocity(JPH::Vec3::sZero());
}
void movePed(std::size_t index,game::Vec2 horizontal,float dt){
    if(!world||index>=pedCharacters.size()||index>=game::peds.size())return;
    auto& character=pedCharacters[index];
    auto& ped=game::peds[index];
    if(!character)character=makePedCharacter(ped.p);
    if(!character)return;
    auto position=character->GetPosition();
    bool teleported=game::len(game::Vec2{float(position.GetX()),float(position.GetZ())}-ped.p)>25;
    if(teleported)character->SetPosition(JPH::RVec3(ped.p.x,0,ped.p.z));
    float vertical=teleported?0.0f:character->GetLinearVelocity().GetY();
    if(character->IsSupported()&&vertical<0)vertical=0;
    vertical+=world->GetGravity().GetY()*dt;
    character->SetLinearVelocity(JPH::Vec3(horizontal.x,vertical,horizontal.z));
    JPH::CharacterVirtual::ExtendedUpdateSettings settings;
    settings.mWalkStairsStepUp=JPH::Vec3(0,5,0);
    settings.mStickToFloorStepDown=JPH::Vec3(0,-5,0);
    character->ExtendedUpdate(dt,world->GetGravity(),settings,
        world->GetDefaultBroadPhaseLayerFilter(Layer::moving),
        world->GetDefaultLayerFilter(Layer::moving),{}, {},*allocator);
    position=character->GetPosition();
    game::Vec2 candidate{float(position.GetX()),float(position.GetZ())};
    if(game::health>0&&game::occupied<0&&game::playerY<25&&
       game::len(candidate-game::player)<18.0f){
        float previousDistance=game::len(ped.p-game::player);
        if(previousDistance>=18.0f||
           game::len(candidate-game::player)<previousDistance){
        game::Vec2 away=game::norm(ped.p-game::player);
        if(game::len(away)<0.01f)away=game::forward(ped.angle);
        candidate=previousDistance<18.0f?ped.p:game::player+away*18.0f;
        ai::pedestrianContact(ped,game::len(horizontal),game::player);
        }
    }
    for(std::size_t other=0;other<game::peds.size();++other){
        if(other==index||!game::peds[other].alive||
           game::peds[other].drivingVehicle>=0||
           std::abs(game::peds[other].p.x-candidate.x)>17||
           std::abs(game::peds[other].p.z-candidate.z)>17||
           game::len(candidate-game::peds[other].p)>=16.0f)continue;
        float previousDistance=game::len(ped.p-game::peds[other].p);
        if(previousDistance<16.0f&&
           game::len(candidate-game::peds[other].p)>=previousDistance)continue;
        game::Vec2 away=game::norm(ped.p-game::peds[other].p);
        if(game::len(away)<0.01f)away=game::forward(ped.angle);
        candidate=previousDistance<16.0f?ped.p:
            game::peds[other].p+away*16.0f;
    }
    if(game::len(candidate-game::Vec2{float(position.GetX()),float(position.GetZ())})>0.001f)
        character->SetPosition(JPH::RVec3(candidate.x,position.GetY(),candidate.z));
    ped.p=candidate;
}
namespace {
void destroyBody(JPH::BodyID& id){
    if(id.IsInvalid())return;
    auto& bodies=world->GetBodyInterface();
    bodies.RemoveBody(id);bodies.DestroyBody(id);id=JPH::BodyID();
}
void syncSceneryColliders(game::Vec2 focus,float dt){
    if(!world)return;
    auto& bodies=world->GetBodyInterface();
    if(treeBodies.size()!=game::trees.size()){
        for(auto& id:treeBodies)destroyBody(id);
        treeBodies.assign(game::trees.size(),JPH::BodyID());
    }
    // Streaming bounds both the broad phase and physics body count in groves.
    for(std::size_t i=0;i<treeBodies.size();++i){
        const auto& tree=game::trees[i];auto& id=treeBodies[i];
        bool nearby=!tree.destroyed&&game::len(tree.p-focus)<1100;
        if(!nearby){destroyBody(id);continue;}
        float radius=treeRadius(tree),halfHeight=std::max(8.0f,tree.height*tree.scale*0.3f);
        if(id.IsInvalid()){
            JPH::BodyCreationSettings settings(new JPH::CylinderShape(halfHeight,radius),
                JPH::RVec3(tree.p.x,halfHeight,tree.p.z),JPH::Quat::sIdentity(),
                JPH::EMotionType::Static,Layer::staticBody);
            settings.mFriction=0.8f;settings.mUserData=treeTag|i;
            id=bodies.CreateAndAddBody(settings,JPH::EActivation::DontActivate);
        }
    }
    if(animalBodies.size()!=wildlife::animals.size()){
        for(auto& id:animalBodies)destroyBody(id);
        animalBodies.assign(wildlife::animals.size(),JPH::BodyID());
        animalBodySpecies.assign(wildlife::animals.size(),-1);
    }
    for(std::size_t i=0;i<animalBodies.size();++i){
        const auto& a=wildlife::animals[i];auto& id=animalBodies[i];
        if(a.health<=0||a.carried||game::len(a.p-focus)>1000){destroyBody(id);continue;}
        const auto& species=wildlife::species()[a.species];
        JPH::RVec3 target(a.p.x,species.height*0.5f,a.p.z);
        JPH::Quat rotation=JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-a.angle);
        if(!id.IsInvalid()&&animalBodySpecies[i]!=a.species)destroyBody(id);
        if(id.IsInvalid()){
            JPH::BodyCreationSettings settings(new JPH::BoxShape(
                JPH::Vec3(species.width*0.5f,species.height*0.5f,species.length*0.45f),0.2f),
                target,rotation,JPH::EMotionType::Kinematic,Layer::moving);
            settings.mFriction=0.5f;settings.mUserData=animalTag|i;
            id=bodies.CreateAndAddBody(settings,JPH::EActivation::Activate);
            animalBodySpecies[i]=a.species;
        }else if(dt>0)bodies.MoveKinematic(id,target,rotation,dt);
        else bodies.SetPositionAndRotation(id,target,rotation,JPH::EActivation::DontActivate);
    }
}
void breakTree(std::size_t index,const SceneryImpact& impact){
    auto& tree=game::trees[index];
    tree.health=0;tree.destroyed=true;tree.burning=false;
    destroyBody(treeBodies[index]);
    auto& bodies=world->GetBodyInterface();
    constexpr std::size_t limit=96;
    while(fragmentBodies.size()+11>limit){
        destroyBody(fragmentBodies.front().id);fragmentBodies.erase(fragmentBodies.begin());
    }
    float radius=treeRadius(tree),height=std::min(240.0f,tree.height*tree.scale);
    for(int piece=0;piece<11;++piece){
        bool foliage=piece>=8;
        float fraction=float(piece%4)/4;
        float angle=float(piece)*2.4f;
        game::Vec3 size=foliage?game::Vec3{radius*3, radius*2,radius*3}:
            piece<4?game::Vec3{radius*1.8f,height*0.23f,radius*1.8f}:
                    game::Vec3{radius*0.6f,height*0.16f,radius*0.6f};
        game::Vec3 position{tree.p.x+(piece<4?0:std::cos(angle)*radius*2),
            piece<4?2+height*(fraction+0.125f):height*(0.65f+fraction*0.3f),
            tree.p.z+(piece<4?0:std::sin(angle)*radius*2)};
        JPH::Quat rotation=piece<4?JPH::Quat::sIdentity():
            JPH::Quat::sRotation(JPH::Vec3(std::cos(angle),0,std::sin(angle)),0.9f);
        JPH::BodyCreationSettings settings(new JPH::BoxShape(
            JPH::Vec3(size.x*0.5f,size.y*0.5f,size.z*0.5f),0.2f),
            JPH::RVec3(position.x,position.y,position.z),rotation,
            JPH::EMotionType::Dynamic,Layer::moving);
        settings.mFriction=0.8f;settings.mRestitution=0.08f;
        settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass=foliage?2.0f:piece<4?45.0f:8.0f;
        settings.mLinearVelocity=JPH::Vec3(impact.direction.x*impact.speed*0.25f+std::cos(angle)*25,
            35+float(piece%3)*15,impact.direction.z*impact.speed*0.25f+std::sin(angle)*25);
        settings.mAngularVelocity=JPH::Vec3(impact.direction.z*2.5f,0.4f,-impact.direction.x*2.5f);
        auto id=bodies.CreateAndAddBody(settings,JPH::EActivation::Activate);
        if(!id.IsInvalid())fragmentBodies.push_back({id,{position,size,rotation.GetX(),
            rotation.GetY(),rotation.GetZ(),rotation.GetW(),foliage},12});
    }
}
}
void rebuildBuilding(std::size_t index){
    if(!world||index>=buildingBodies.size())return;
    bool active=!buildingBodies[index].IsInvalid();destroyBody(buildingBodies[index]);
    if(active)buildingBodies[index]=createBuilding(game::buildings[index]);
}
void spawnFragment(game::Vec3 p,game::Vec3 size,game::Vec3 velocity,game::Color tint,int shape,int masonryKind){
    if(!world)return;
    while(fragmentBodies.size()>=160){destroyBody(fragmentBodies.front().id);fragmentBodies.erase(fragmentBodies.begin());}
    JPH::RefConst<JPH::Shape> collider=new JPH::BoxShape(JPH::Vec3(size.x*.5f,size.y*.5f,size.z*.5f),.1f);
    if(masonryKind==masonry::Block){
        JPH::StaticCompoundShapeSettings block;
        auto wall=[&](game::Vec3 center,game::Vec3 extent){
            block.AddShape(JPH::Vec3(center.x*size.x,center.y*size.y,center.z*size.z),JPH::Quat::sIdentity(),
                new JPH::BoxShape(JPH::Vec3(extent.x*size.x,extent.y*size.y,extent.z*size.z),.03f));
        };
        wall({0,0,-.4f},{.5f,.5f,.1f});wall({0,0,.4f},{.5f,.5f,.1f});
        for(float x:{-.435f,0.0f,.435f})wall({x,0,0},{.065f,.5f,.3f});
        auto result=block.Create();if(result.IsValid())collider=result.Get();
    }else if(masonryKind>=0){
        auto geometry=masonry::build(masonryKind);JPH::Array<JPH::Vec3> points;
        for(auto point:geometry.hull)points.push_back(JPH::Vec3(point.x*size.x,point.y*size.y,point.z*size.z));
        JPH::ConvexHullShapeSettings hull(points,.05f);auto result=hull.Create();
        if(result.IsValid())collider=result.Get();
    }
    JPH::BodyCreationSettings settings(collider,
        JPH::RVec3(p.x,p.y,p.z),JPH::Quat::sIdentity(),JPH::EMotionType::Dynamic,Layer::moving);
    settings.mFriction=masonryKind>=0?.85f:.7f;settings.mRestitution=masonryKind>=0?.08f:.15f;settings.mMotionQuality=JPH::EMotionQuality::LinearCast;
    settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass=std::clamp(size.x*size.y*size.z*.01f,2.0f,180.0f);
    settings.mLinearVelocity=JPH::Vec3(velocity.x,velocity.y,velocity.z);
    settings.mAngularVelocity=JPH::Vec3(1.7f+velocity.z*.015f,.9f,2.1f-velocity.x*.012f);
    auto id=world->GetBodyInterface().CreateAndAddBody(settings,JPH::EActivation::Activate);
    TreeFragment visual;visual.p=p;visual.size=size;visual.category=1;visual.shape=shape;visual.color=tint;
    if(masonryKind>=0){visual.category=2;visual.mesh=masonry::name(masonryKind);}
    if(!id.IsInvalid())fragmentBodies.push_back({id,visual,masonryKind>=0?45.0f:18.0f});
}
void vehicleImpulse(std::size_t index,game::Vec3 impulse){
    if(world&&index<vehicleBodies.size()&&!vehicleBodies[index].IsInvalid())
        world->GetBodyInterface().AddImpulse(vehicleBodies[index],JPH::Vec3(impulse.x,impulse.y,impulse.z));
}
void detachHitch(std::size_t index){
    for(auto it=hitches.begin();it!=hitches.end();){
        if(it->truck==index||it->trailer==index){
            game::vehicles[it->truck].trailer=-1;game::vehicles[it->trailer].towVehicle=-1;
            world->RemoveConstraint(it->joint.GetPtr());it=hitches.erase(it);
        }else ++it;
    }
}
bool toggleTrailer(std::size_t index){
    using namespace game;
    if(!world||index>=vehicles.size()||vehicles[index].kind!=Kind::Truck||std::abs(vehicles[index].speed)>12||vehicles[index].exploded)return false;
    if(vehicles[index].trailer>=0){detachHitch(index);return true;}
    auto& bodies=world->GetBodyInterface();
    auto truckId=vehicleBodies[index];if(truckId.IsInvalid())return false;
    for(std::size_t n=0;n<vehicles.size();++n){auto& trailer=vehicles[n];
        if(trailer.kind!=Kind::Trailer||trailer.exploded||trailer.towVehicle>=0||vehicleBodies[n].IsInvalid())continue;
        auto truckPoint=bodies.GetCenterOfMassPosition(truckId)+bodies.GetRotation(truckId)*JPH::Vec3(0,0,-62);
        auto trailerPoint=bodies.GetCenterOfMassPosition(vehicleBodies[n])+bodies.GetRotation(vehicleBodies[n])*JPH::Vec3(0,0,76);
        if((truckPoint-trailerPoint).Length()>24||std::abs(trailer.speed)>12)continue;
        JPH::BodyLockWrite a(world->GetBodyLockInterfaceNoLock(),truckId);
        JPH::BodyLockWrite b(world->GetBodyLockInterfaceNoLock(),vehicleBodies[n]);
        if(!a.Succeeded()||!b.Succeeded())return false;
        JPH::PointConstraintSettings settings;settings.mPoint1=truckPoint;settings.mPoint2=trailerPoint;
        JPH::Ref<JPH::TwoBodyConstraint> joint=settings.Create(a.GetBody(),b.GetBody());
        world->AddConstraint(joint.GetPtr());hitches.push_back({index,n,joint});
        vehicles[index].trailer=int(n);trailer.towVehicle=int(index);
        bodies.ActivateBody(truckId);bodies.ActivateBody(vehicleBodies[n]);return true;
    }
    return false;
}
void breakVehicle(std::size_t index,bool fragments){
    if(!world||index>=vehicleBodies.size()||vehicleBodies[index].IsInvalid())return;
    auto& v=game::vehicles[index];auto& bodies=world->GetBodyInterface();auto id=vehicleBodies[index];
    auto rotation=bodies.GetRotation(id);auto velocity=bodies.GetLinearVelocity(id);
    v.qx=rotation.GetX();v.qy=rotation.GetY();v.qz=rotation.GetZ();v.qw=rotation.GetW();
    detachHitch(index);
    if(vehicleConstraints[index]){
        world->RemoveStepListener(vehicleConstraints[index].GetPtr());world->RemoveConstraint(vehicleConstraints[index].GetPtr());vehicleConstraints[index]=nullptr;
    }
    destroyBody(vehicleBodies[index]);
    if(fragments){int n=0;for(const auto& part:vehicle_systems::parts(v)){
        game::Vec3 p=vehicle_systems::partPosition(v,part);p.y=std::max(3.0f,p.y);
        float a=n++*2.39996f;
        spawnFragment(p,part.size,{velocity.GetX()+std::cos(a)*130,velocity.GetY()+120+float(n%4)*30,velocity.GetZ()+std::sin(a)*130},part.color,part.shape);
        if(!fragmentBodies.empty()){auto& f=fragmentBodies.back();f.visual.mesh=part.mesh;
            auto pose=vehicle_systems::partRotation(v,part);auto partRotation=JPH::Quat(pose.qx,pose.qy,pose.qz,pose.qw);
            bodies.SetRotation(f.id,partRotation,JPH::EActivation::Activate);
            f.visual.qx=pose.qx;f.visual.qy=pose.qy;f.visual.qz=pose.qz;f.visual.qw=pose.qw;}
    }}
}
void flyAirplane(std::size_t index,float throttle,float roll,float pitch,float rudder,float dt){
    using namespace game;
    if(!world||index>=vehicleBodies.size()||vehicleBodies[index].IsInvalid()||dt<=0)return;
    auto& v=vehicles[index];auto& bodies=world->GetBodyInterface();auto id=vehicleBodies[index];
    bool piloted=int(index)==occupied&&!v.exploded&&health>0;
    v.flightThrottle=piloted?std::clamp(v.flightThrottle+throttle*dt*.55f,0.0f,1.0f):std::max(0.0f,v.flightThrottle-dt*.3f);
    auto q=bodies.GetRotation(id);auto f=q*JPH::Vec3::sAxisZ(),up=q*JPH::Vec3::sAxisY(),side=q*JPH::Vec3::sAxisX();
    auto velocity=bodies.GetLinearVelocity(id);float speed=velocity.Length(),airspeed=std::max(0.0f,velocity.Dot(f));
    float aoa=std::atan2(-velocity.Dot(up),std::max(1.0f,airspeed));
    float cl=std::clamp(.65f+3.5f*aoa,-1.0f,1.8f);
    v.stalled=airspeed<140||std::abs(aoa)>.38f;
    if(std::abs(aoa)>.38f)cl*=.2f;
    float mass=physics::tuning(v.kind).mass,power=std::max(.3f,1-v.damage/130);
    // Forces act on the rigid body. Lift depends on airspeed squared and angle
    // of attack; stall loses lift, drag dissipates energy, thrust needs throttle.
    JPH::Vec3 acceleration=f*(v.flightThrottle*550*power)+up*(std::min(2000.0f,airspeed*airspeed*.022f*cl))-
        velocity*(.025f+speed*.0008f+std::abs(cl)*.00018f*speed)-side*(velocity.Dot(side)*1.4f);
    bodies.AddForce(id,acceleration*mass);
    float authority=std::clamp(airspeed/200,0.0f,1.0f);
    if(piloted){
        auto current=bodies.GetAngularVelocity(id);
        float sideslip=std::atan2(velocity.Dot(side),std::max(1.0f,airspeed));
        JPH::Vec3 target=side*(-pitch*.7f*authority)+f*(-roll*1.1f*authority)+
            JPH::Vec3::sAxisY()*((sideslip*1.5f-rudder*.4f)*authority);
        // Passive pitch damping lets a released stick trim toward the airflow.
        target+=side*(aoa*.7f*authority);
        bodies.SetAngularVelocity(id,current+(target-current)*std::min(1.0f,dt*4));
        if(vehicleConstraints[index])static_cast<JPH::WheeledVehicleController*>(vehicleConstraints[index]->GetController())->SetDriverInput(0,rudder,throttle<0&&v.rideHeight<3?1.0f:0.0f,0);
        syncBuildingColliders(v.p);syncSceneryColliders(v.p);bodies.ActivateBody(id);
    }
}
void flyHelicopter(std::size_t index,float throttle,float steering,float lift,float dt){
    if(!world||index>=vehicleBodies.size()||vehicleBodies[index].IsInvalid())return;
    auto& v=game::vehicles[index];if(v.kind!=game::Kind::Helicopter)return;
    auto& bodies=world->GetBodyInterface();auto id=vehicleBodies[index];
    if(int(index)==game::occupied){syncBuildingColliders(v.p);syncSceneryColliders(v.p);}
    bool powered=int(index)==game::occupied&&!v.exploded&&game::health>0;
    float rotorTarget=powered?1.0f:0.0f;
    v.rotorSpeed+=(rotorTarget-v.rotorSpeed)*std::min(1.0f,dt*2.5f);
    auto current=bodies.GetLinearVelocity(id);
    if(powered){
        bodies.ActivateBody(id);
        float authority=std::clamp((v.rotorSpeed-0.5f)*2,0.0f,1.0f);
        float power=std::max(0.4f,1.0f-v.damage/140);
        v.yawRate+=(steering*physics::tuning(v.kind).turnRate*authority-v.yawRate)*std::min(1.0f,dt*4);
        v.angle+=v.yawRate*dt;
        game::Vec2 facing=game::forward(v.angle);
        float target=throttle*(throttle>=0?physics::tuning(v.kind).maxSpeed:
            physics::tuning(v.kind).reverseSpeed)*power*authority;
        float response=std::min(1.0f,dt*physics::tuning(v.kind).grip);
        bodies.SetLinearVelocity(id,JPH::Vec3(
            current.GetX()+(facing.x*target-current.GetX())*response,current.GetY(),
            current.GetZ()+(facing.z*target-current.GetZ())*response));
        float desired=lift*150*power;
        if(v.rideHeight>1250)desired=std::min(desired,-50.0f);
        float acceleration=-world->GetGravity().GetY()*authority+
            std::clamp((desired-current.GetY())*4,-500.0f,500.0f)*authority;
        bodies.AddForce(id,JPH::Vec3(0,physics::tuning(v.kind).mass*acceleration,0));
        bodies.SetRotation(id,JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-v.angle),JPH::EActivation::Activate);
        bodies.SetAngularVelocity(id,JPH::Vec3::sZero());
    }
    v.flightPitch+=(throttle*0.16f*(powered?1:0)-v.flightPitch)*std::min(1.0f,dt*3);
    v.lean+=(-steering*0.2f*(powered?1:0)-v.lean)*std::min(1.0f,dt*3);
    v.rotorAngle=std::fmod(v.rotorAngle+v.rotorSpeed*42*dt,game::PI*2);
}
void driveVehicle(std::size_t index,float throttle,float steering,float,bool brake){
    if(!world||index>=vehicleBodies.size()||vehicleBodies[index].IsInvalid())return;
    auto& vehicle=game::vehicles[index];auto& bodies=world->GetBodyInterface();
    if(int(index)==game::occupied)syncBuildingColliders(vehicle.p);
    if(vehicle.exploded){throttle=0;steering=0;}
    auto id=vehicleBodies[index];
    auto position=bodies.GetCenterOfMassPosition(id);
    bool relocated=game::len(vehicle.p-vehicleSynced[index])>3.0f;
    if(relocated){
        bodies.SetPosition(id,JPH::RVec3(vehicle.p.x,position.GetY(),vehicle.p.z),JPH::EActivation::Activate);
        bodies.SetRotation(id,JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-vehicle.angle),
            JPH::EActivation::Activate);
        bodies.SetLinearVelocity(id,JPH::Vec3::sZero());
    }
    if(vehicle.kind==game::Kind::Boat){
        JPH::Vec3 velocity=bodies.GetLinearVelocity(id);
        bodies.SetLinearVelocity(id,JPH::Vec3(vehicle.velocity.x,velocity.GetY(),vehicle.velocity.z));
        bodies.SetRotation(id,JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-vehicle.angle),
            JPH::EActivation::Activate);
    }else if(index<vehicleConstraints.size()&&vehicleConstraints[index]){
        auto* controller=static_cast<JPH::WheeledVehicleController*>(
            vehicleConstraints[index]->GetController());
        float power=std::max(0.35f,1.0f-vehicle.damage/145.0f);
        float torque=physics::tuning(vehicle.kind).engineTorque;
        controller->GetEngine().mMaxTorque=torque*power*physics::vehicleScale(vehicle.kind);
        bool braking=brake||throttle*vehicle.speed<-5.0f;
        controller->SetDriverInput(braking?0.0f:throttle,steering,
            braking?1.0f:std::abs(throttle)<0.01f?0.03f:0.0f,
            int(index)==game::occupied&&game::keys[VK_SPACE]?1.0f:0.0f);
        if(brake||std::abs(throttle)>0.01f||std::abs(steering)>0.01f) bodies.ActivateBody(id);
    }
}
void teleportVehicle(std::size_t index,game::Vec2 position,float angle,float altitude){
    if(index>=game::vehicles.size())return;
    auto& vehicle=game::vehicles[index];
    vehicle.p=position;vehicle.angle=angle;
    vehicle.flightThrottle=0;vehicle.flightPitch=vehicle.flightRoll=0;
    vehicle.velocity={};vehicle.speed=0;vehicle.yawRate=0;
    vehicle.rideHeight=altitude;vehicle.verticalSpeed=0;
    vehicle.qx=vehicle.qz=0;vehicle.qy=std::sin((game::PI/2-angle)*.5f);vehicle.qw=std::cos((game::PI/2-angle)*.5f);
    if(!world||index>=vehicleBodies.size()||vehicleBodies[index].IsInvalid())return;
    auto& bodies=world->GetBodyInterface();
    float height=physics::vehicleRestHeight(vehicle.kind)+altitude;
    bodies.SetPosition(vehicleBodies[index],
        JPH::RVec3(position.x,height,position.z),JPH::EActivation::Activate);
    bodies.SetRotation(vehicleBodies[index],
        JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-angle),
        JPH::EActivation::Activate);
    bodies.SetLinearVelocity(vehicleBodies[index],JPH::Vec3::sZero());
    bodies.SetAngularVelocity(vehicleBodies[index],JPH::Vec3::sZero());
    vehicleSynced[index]=position;
}
void stopVehicle(std::size_t index){
    if(index>=game::vehicles.size())return;
    auto& vehicle=game::vehicles[index];
    vehicle.velocity={};vehicle.speed=0;vehicle.yawRate=0;
    driveVehicle(index,0,0,0,true);
    if(!world||index>=vehicleBodies.size()||vehicleBodies[index].IsInvalid())return;
    auto& bodies=world->GetBodyInterface();
    // Preserve suspension height and vertical velocity; stop planar motion only.
    float vertical=bodies.GetLinearVelocity(vehicleBodies[index]).GetY();
    bodies.SetLinearVelocity(vehicleBodies[index],JPH::Vec3(0,vertical,0));
    bodies.SetAngularVelocity(vehicleBodies[index],JPH::Vec3::sZero());
}
void coastVehicle(std::size_t index,float dt){
    if(!world||index>=vehicleBodies.size()||index>=game::vehicles.size()||
       vehicleBodies[index].IsInvalid())return;
    auto& vehicle=game::vehicles[index];
    auto& bodies=world->GetBodyInterface();
    if(!bodies.IsActive(vehicleBodies[index])&&game::len(vehicle.p-vehicleSynced[index])<=3)return;
    if(vehicle.kind==game::Kind::Helicopter){flyHelicopter(index,0,0,0,dt);return;}
    if(vehicle.kind==game::Kind::Airplane){flyAirplane(index,0,0,0,0,dt);return;}
    if(vehicle.kind==game::Kind::Trailer&&vehicle.towVehicle>=0){driveVehicle(index,0,0,dt);return;}
    if(vehicle.kind==game::Kind::Boat)physics::stepVehicle(vehicle,0,0,dt);
    // Driver inputs persist in Jolt. Release them and cut engine power, while
    // preserving chassis momentum and contacts. Gentle drag settles a rolling
    // empty car over several seconds rather than applying the parking brake.
    driveVehicle(index,0,0,dt);
    if(index<vehicleConstraints.size()&&vehicleConstraints[index])
        static_cast<JPH::WheeledVehicleController*>(vehicleConstraints[index]->GetController())->GetEngine().mMaxTorque=0;
    auto velocity=bodies.GetLinearVelocity(vehicleBodies[index]);
    float drag=std::exp(-0.65f*dt);
    float speed=std::hypot(velocity.GetX(),velocity.GetZ());
    if(speed<0.75f){
        if(index<vehicleConstraints.size()&&vehicleConstraints[index])
            static_cast<JPH::WheeledVehicleController*>(vehicleConstraints[index]->GetController())->SetDriverInput(0,0,1,1);
        bodies.SetLinearVelocity(vehicleBodies[index],JPH::Vec3(0,velocity.GetY(),0));
        return;
    }
    bodies.SetLinearVelocity(vehicleBodies[index],JPH::Vec3(
        velocity.GetX()*drag,velocity.GetY(),velocity.GetZ()*drag));
}
int wheelContactCount(std::size_t index){
    if(index>=vehicleConstraints.size()||!vehicleConstraints[index])return 0;
    int count=0;
    for(const auto* wheel:vehicleConstraints[index]->GetWheels())
        if(wheel->HasContact())++count;
    return count;
}
std::size_t activeBuildingColliderCount(){
    return std::count_if(buildingBodies.begin(),buildingBodies.end(),
        [](JPH::BodyID id){return !id.IsInvalid();});
}
float treeRadius(const game::Tree& tree){return 6.0f*std::clamp(tree.scale,0.25f,4.0f);}
const std::vector<TreeFragment>& treeFragments(){return fragmentVisuals;}
std::size_t activeTreeColliderCount(){
    return std::count_if(treeBodies.begin(),treeBodies.end(),[](JPH::BodyID id){return !id.IsInvalid();});
}
std::size_t activeAnimalColliderCount(){
    return std::count_if(animalBodies.begin(),animalBodies.end(),[](JPH::BodyID id){return !id.IsInvalid();});
}
std::size_t activePedCharacterCount(){
    return std::count_if(pedCharacters.begin(),pedCharacters.end(),
        [](const auto& character){return character!=nullptr;});
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
void spawnRagdoll(const game::Ped& ped,game::Vec3 impulse,
    const game::Vec2* pinAnchor){
    if(!world)return;
    game::corpseSnapshots.erase(std::remove_if(game::corpseSnapshots.begin(),
        game::corpseSnapshots.end(),[&](const game::CorpseSnapshot& old){
            return old.pedId==ped.id;
        }),game::corpseSnapshots.end());
    if(ragdolls.size()>=12){
        auto& old=ragdolls.front();auto& bodies=world->GetBodyInterface();
        captureCorpsePose(old);
        for(auto& joint:old.joints)world->RemoveConstraint(joint.GetPtr());
        for(auto id:old.bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
        ragdolls.erase(ragdolls.begin());
    }
    struct Part {float x,y,z,w,h,d;};
    const Part parts[]={
        {0,25,0,10,16,7}, {0,37,0,8,8,8},
        {-8,24,0,4,13,4}, {8,24,0,4,13,4},
        {-3,10,0,5,16,5}, {3,10,0,5,16,5}};
    Ragdoll ragdoll;ragdoll.style=ped.style;ragdoll.pedId=ped.id;
    if(pinAnchor)ragdoll.life=std::min(15.0f,ped.respawn);
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
    if(pinAnchor){
        JPH::DistanceConstraintSettings settings;
        settings.mPoint1=JPH::RVec3(pinAnchor->x,25,pinAnchor->z);
        settings.mPoint2=JPH::RVec3(ped.p.x,25,ped.p.z);
        settings.mMinDistance=0;settings.mMaxDistance=2;
        JPH::Ref<JPH::TwoBodyConstraint> joint=
            settings.Create(JPH::Body::sFixedToWorld,*created[0]);
        world->AddConstraint(joint.GetPtr());ragdoll.joints.push_back(joint);
    }
    ragdolls.push_back(std::move(ragdoll));
}
void removeRagdoll(const std::string& pedId){
    if(!world)return;
    game::corpseSnapshots.erase(std::remove_if(game::corpseSnapshots.begin(),
        game::corpseSnapshots.end(),[&](const game::CorpseSnapshot& snapshot){
            return snapshot.pedId==pedId;
        }),game::corpseSnapshots.end());
    auto& bodies=world->GetBodyInterface();
    for(auto it=ragdolls.begin();it!=ragdolls.end();){
        if(it->pedId!=pedId){++it;continue;}
        for(auto& joint:it->joints)world->RemoveConstraint(joint.GetPtr());
        for(auto id:it->bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
        it=ragdolls.erase(it);
    }
    game::ragdollParts.clear();
}
void clearRagdolls(){
    if(!world)return;
    auto& bodies=world->GetBodyInterface();
    for(auto& ragdoll:ragdolls){
        for(auto& joint:ragdoll.joints)world->RemoveConstraint(joint.GetPtr());
        for(auto id:ragdoll.bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
    }
    ragdolls.clear();game::ragdollParts.clear();game::corpseSnapshots.clear();
}
void step(float dt){
    if(!world||dt<=0)return;
    syncBuildingColliders(game::occupied>=0&&
        std::size_t(game::occupied)<game::vehicles.size()?
        game::vehicles[game::occupied].p:game::player);
    syncSceneryColliders(game::player,dt);
    for(std::size_t index=0;index<pedCharacters.size()&&index<game::peds.size();++index)
        if(!game::peds[index].alive||game::peds[index].drivingVehicle>=0)pedCharacters[index]=nullptr;
    auto& bodies=world->GetBodyInterface();
    for(std::size_t i=0;i<vehicleBodies.size()&&i<game::vehicles.size();++i){
        if(vehicleBodies[i].IsInvalid()||game::vehicles[i].kind!=game::Kind::Boat)continue;
        auto position=bodies.GetCenterOfMassPosition(vehicleBodies[i]);
        float y=position.GetY();
        if(y<10.0f){
            float velocity=bodies.GetLinearVelocity(vehicleBodies[i]).GetY();
            float acceleration=700+(10-y)*35-velocity*8;
            bodies.AddForce(vehicleBodies[i],JPH::Vec3(0,550*std::max(0.0f,acceleration),0));
        }
    }
    vehicleImpacts.assign(vehicleBodies.size(),{});
    treeImpacts.assign(treeBodies.size(),{});animalImpacts.assign(animalBodies.size(),{});
    world->Update(dt,1,allocator.get(),jobs.get());
    // Contact callbacks only collect closing speed. Body removal and gameplay
    // damage happen here after Jolt releases its locks.
    for(std::size_t i=0;i<treeImpacts.size();++i){
        const auto& impact=treeImpacts[i];
        if(impact.vehicle<0||game::trees[i].destroyed)continue;
        float mass=physics::tuning(game::vehicles[impact.vehicle].kind).mass;
        float severity=impact.speed*std::sqrt(mass/1100.0f);
        float threshold=150*std::sqrt(treeRadius(game::trees[i])/6)*
            (0.65f+0.35f*game::trees[i].health/100.0f);
        if(severity>=threshold)breakTree(i,impact);
    }
    for(std::size_t i=0;i<animalImpacts.size();++i){
        const auto& impact=animalImpacts[i];auto& animal=wildlife::animals[i];
        if(impact.vehicle<0||impact.speed<=35||animal.health<=0||animal.impactCooldown>0)continue;
        int damage=std::max(1,int((impact.speed-25)*
            std::sqrt(physics::tuning(game::vehicles[impact.vehicle].kind).mass/1100.0f)));
        animal.impactCooldown=0.6f;
        wildlife::hurt(int(i),damage,game::vehicles[impact.vehicle].p,impact.vehicle==game::occupied);
        if(animal.health<=0)destroyBody(animalBodies[i]);
    }
    for(std::size_t i=0;i<vehicleBodies.size()&&i<game::vehicles.size();++i){
        if(vehicleBodies[i].IsInvalid())continue;
        auto position=bodies.GetCenterOfMassPosition(vehicleBodies[i]);
        // Recover vehicles that were already beyond the border (for example
        // from an older save) before syncing their position to the game.
        if(position.GetX()<0||position.GetX()>regions::WIDTH||
           position.GetZ()<0||position.GetZ()>regions::DEPTH){
            game::Vec2 safe{
                std::clamp(float(position.GetX()),55.0f,regions::WIDTH-55.0f),
                std::clamp(float(position.GetZ()),55.0f,regions::DEPTH-55.0f)};
            teleportVehicle(i,safe,game::vehicles[i].angle);
            position=bodies.GetCenterOfMassPosition(vehicleBodies[i]);
        }
        auto velocity=bodies.GetLinearVelocity(vehicleBodies[i]);
        auto& vehicle=game::vehicles[i];
        game::Vec2 resolved{velocity.GetX(),velocity.GetZ()};
        const auto impact=vehicleImpacts[i];
        float threshold=impact.playerCaused?12.0f:60.0f;
        if(impact.speed>threshold&&!vehicle.exploded&&vehicle.collisionCooldown<=0){
            float amount=impact.playerCaused?std::max(1.0f,(impact.speed-12)*0.12f):
                (impact.speed-60)*0.18f*physics::tuning(vehicle.kind).collisionDamageScale;
            game::damageVehicle(int(i),amount,impact.playerCaused);
            vehicle.collisionCooldown=0.35f;
            if(vehicle.exploded)continue;
            if(impact.speed>135)
                if(int(i)==game::occupied)game::applyDamage((impact.speed-135)*0.045f);
        }
        vehicle.p={position.GetX(),position.GetZ()};
        vehicleSynced[i]=vehicle.p;
        vehicle.velocity=resolved;
        float restHeight=physics::vehicleRestHeight(vehicle.kind);
        vehicle.rideHeight=vehicle_systems::aircraft(vehicle.kind)?
            std::max(0.0f,float(position.GetY())-restHeight):
            std::clamp(float(position.GetY())-restHeight,-6.0f*physics::vehicleScale(vehicle.kind),12.0f*physics::vehicleScale(vehicle.kind));
        vehicle.verticalSpeed=velocity.GetY();
        auto rotation=bodies.GetRotation(vehicleBodies[i]);
        vehicle.qx=rotation.GetX();vehicle.qy=rotation.GetY();vehicle.qz=rotation.GetZ();vehicle.qw=rotation.GetW();
        if(vehicle.kind==game::Kind::Airplane){
            auto f=rotation*JPH::Vec3::sAxisZ(),up=rotation*JPH::Vec3::sAxisY();
            vehicle.flightPitch=std::asin(std::clamp(f.GetY(),-1.0f,1.0f));
            vehicle.flightRoll=std::atan2((rotation*JPH::Vec3::sAxisX()).GetY(),up.GetY());
        }
        if(vehicle.kind!=game::Kind::Boat){
            JPH::Vec3 forward=bodies.GetRotation(vehicleBodies[i])*JPH::Vec3::sAxisZ();
            vehicle.angle=std::atan2(forward.GetZ(),forward.GetX());
        }
        vehicle.speed=vehicle.velocity.x*std::cos(vehicle.angle)+
            vehicle.velocity.z*std::sin(vehicle.angle);
    }
    if(game::occupied>=0&&std::size_t(game::occupied)<game::vehicles.size())
        game::player=game::vehicles[game::occupied].p;
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
            captureCorpsePose(*it);
            for(auto& joint:it->joints)world->RemoveConstraint(joint.GetPtr());
            for(auto id:it->bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
            it=ragdolls.erase(it);continue;
        }
        if(!it->bodies.empty())for(auto& ped:game::peds)if(ped.id==it->pedId&&!ped.alive){
            auto p=bodies.GetCenterOfMassPosition(it->bodies[0]);
            ped.p={float(p.GetX()),float(p.GetZ())};
            break;
        }
        for(std::size_t i=0;i<it->bodies.size();++i){
            auto p=bodies.GetCenterOfMassPosition(it->bodies[i]);
            auto q=bodies.GetRotation(it->bodies[i]);
            game::ragdollParts.push_back({{p.GetX(),p.GetY(),p.GetZ()},it->rest[i],
                it->origin,q.GetX(),q.GetY(),q.GetZ(),q.GetW(),it->yaw,it->style,int(i)});
        }
        ++it;
    }
    for(const auto& snapshot:game::corpseSnapshots)
        game::ragdollParts.insert(game::ragdollParts.end(),
            snapshot.parts.begin(),snapshot.parts.end());
    fragmentVisuals.clear();
    for(auto it=fragmentBodies.begin();it!=fragmentBodies.end();){
        it->life-=dt;
        if(it->life<=0){destroyBody(it->id);it=fragmentBodies.erase(it);continue;}
        auto p=bodies.GetPosition(it->id);auto q=bodies.GetRotation(it->id);
        it->visual.p={p.GetX(),p.GetY(),p.GetZ()};
        it->visual.qx=q.GetX();it->visual.qy=q.GetY();it->visual.qz=q.GetZ();it->visual.qw=q.GetW();
        fragmentVisuals.push_back(it->visual);++it;
    }
}
}
