#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Vehicle/VehicleConstraint.h>
#include <Jolt/Physics/Vehicle/WheeledVehicleController.h>
#include "jolt_world.h"
#include "regions.h"
#include "physics.h"
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
std::vector<JPH::Ref<JPH::VehicleConstraint>> vehicleConstraints;
std::vector<game::Vec2> vehicleSynced;
JPH::Ref<JPH::CharacterVirtual> playerCharacter;
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
            id=createStatic({building.x+building.w*0.5f,building.h*0.5f,
                building.z+building.d*0.5f},
                {building.w*0.5f,building.h*0.5f,building.d*0.5f});
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
    ragdolls.clear();game::ragdollParts.clear();
    propBodies.clear();vehicleBodies.clear();vehicleConstraints.clear();vehicleSynced.clear();
    staticBodies.clear();buildingBodies.clear();streamedCellX=streamedCellZ=-1;
    world.reset();jobs.reset();allocator.reset();
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
    syncBuildingColliders(game::player);
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
        bool bike=vehicle.kind==game::Kind::Bike;
        float halfW=bike?5.0f:boat?12.0f:13.0f;
        float halfL=vehicle.kind==game::Kind::Bike?13.0f:24.0f;
        float halfH=boat?10.0f:bike?4.0f:5.0f;
        float mass=tuning.mass;
        JPH::BodyCreationSettings settings(new JPH::BoxShape(JPH::Vec3(halfW,halfH,halfL)),
            JPH::RVec3(vehicle.p.x,boat?halfH:bike?13.0f:14.0f,vehicle.p.z),
            JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-vehicle.angle),
            JPH::EMotionType::Dynamic,Layer::moving);
        // Wheels provide ground contact; the chassis should not drag on the road.
        settings.mFriction=0.02f;settings.mRestitution=0.04f;
        settings.mMotionQuality=JPH::EMotionQuality::LinearCast;
        settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass=mass;
        JPH::Body* body=bodies.CreateBody(settings);
        auto id=body?body->GetID():JPH::BodyID();
        if(body)bodies.AddBody(id,JPH::EActivation::Activate);
        vehicleBodies.push_back(id);vehicleSynced.push_back(vehicle.p);
        JPH::Ref<JPH::VehicleConstraint> constraint;
        if(body&&!boat){
            JPH::VehicleConstraintSettings wheels;
            wheels.mMaxPitchRollAngle=bike?0.48f:0.62f;
            float radius=tuning.wheelRadius;
            float width=bike?2.5f:3.5f;
            float wheelX=bike?2.7f:halfW*0.78f;
            float wheelZ=bike?halfL*0.72f:halfL*0.76f;
            for(int axle=0;axle<2;++axle)for(int side=0;side<2;++side){
                JPH::WheelSettingsWV* wheel=new JPH::WheelSettingsWV();
                wheel->mPosition=JPH::Vec3(side==0?wheelX:-wheelX,
                    -halfH*0.85f,axle==0?wheelZ:-wheelZ);
                wheel->mRadius=radius;wheel->mWidth=width;
                wheel->mSuspensionMinLength=1.5f;
                wheel->mSuspensionMaxLength=bike?5.0f:5.5f;
                wheel->mSuspensionSpring.mFrequency=tuning.suspensionFrequency;
                wheel->mSuspensionSpring.mDamping=tuning.suspensionDamping;
                wheel->mMaxSteerAngle=axle==0?tuning.steerAngle:0.0f;
                wheel->mMaxHandBrakeTorque=axle==0?0.0f:tuning.handbrakeTorque;
                wheel->mMaxBrakeTorque=tuning.brakeTorque;
                wheel->mInertia=bike?65.0f:220.0f;
                wheels.mWheels.push_back(wheel);
            }
            JPH::WheeledVehicleControllerSettings* controller=
                new JPH::WheeledVehicleControllerSettings();
            controller->mEngine.mMaxTorque=tuning.engineTorque;
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
    characterSettings->mShape=new JPH::CapsuleShape(8.0f,10.0f);
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
        if(game::peds[index].alive&&game::len(game::peds[index].p-game::player)<500)
            pedCharacters[index]=makePedCharacter(game::peds[index].p);
    world->OptimizeBroadPhase();
}
void addPed(){
    if(!world||pedCharacters.size()>=game::peds.size())return;
    std::size_t previous=pedCharacters.size();
    pedCharacters.resize(game::peds.size());
    for(std::size_t index=previous;index<game::peds.size();++index)
        if(game::peds[index].alive&&game::len(game::peds[index].p-game::player)<500)
            pedCharacters[index]=makePedCharacter(game::peds[index].p);
}
void moveCharacter(game::Vec2 horizontal,bool jump,float dt){
    if(!playerCharacter||!world)return;
    syncBuildingColliders(game::player);
    auto position=playerCharacter->GetPosition();
    if(game::len(game::Vec2{float(position.GetX()),float(position.GetZ())}-game::player)>8||
       std::abs(float(position.GetY())-game::playerY)>45)
        playerCharacter->SetPosition(JPH::RVec3(game::player.x,game::playerY,game::player.z));
    position=playerCharacter->GetPosition();
    bool swimming=regions::waterAt(game::player+horizontal*dt);
    bool supported=playerCharacter->IsSupported();
    float vertical=playerCharacter->GetLinearVelocity().GetY();
    if(swimming)vertical=std::clamp(-float(position.GetY())*6.0f,-50.0f,50.0f);
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
    game::player={float(position.GetX()),float(position.GetZ())};
    game::playerY=swimming?float(position.GetY()):std::max(0.0f,float(position.GetY()));
    game::playerVerticalSpeed=playerCharacter->GetLinearVelocity().GetY();
    game::grounded=!swimming&&playerCharacter->IsSupported();
    game::swimming=swimming;
    if(!swimming&&!supported&&game::grounded&&vertical<-380)
        game::applyDamage((std::abs(vertical)-380)*0.06f);
}
void teleportCharacter(game::Vec2 position,float height){
    if(!playerCharacter)return;
    syncBuildingColliders(position);
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
    ped.p={float(position.GetX()),float(position.GetZ())};
}
void driveVehicle(std::size_t index,float throttle,float steering,float){
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
        controller->GetEngine().mMaxTorque=torque*power;
        bool braking=throttle*vehicle.speed<-5.0f;
        controller->SetDriverInput(braking?0.0f:throttle,steering,
            braking?1.0f:std::abs(throttle)<0.01f?0.03f:0.0f,
            game::keys[VK_SPACE]?1.0f:0.0f);
        if(std::abs(throttle)>0.01f||std::abs(steering)>0.01f) bodies.ActivateBody(id);
    }
}
void teleportVehicle(std::size_t index,game::Vec2 position,float angle){
    if(index>=game::vehicles.size())return;
    auto& vehicle=game::vehicles[index];
    vehicle.p=position;vehicle.angle=angle;
    vehicle.velocity={};vehicle.speed=0;vehicle.yawRate=0;
    if(!world||index>=vehicleBodies.size()||vehicleBodies[index].IsInvalid())return;
    auto& bodies=world->GetBodyInterface();
    float height=vehicle.kind==game::Kind::Boat?10.0f:
        vehicle.kind==game::Kind::Bike?13.0f:14.0f;
    bodies.SetPosition(vehicleBodies[index],
        JPH::RVec3(position.x,height,position.z),JPH::EActivation::Activate);
    bodies.SetRotation(vehicleBodies[index],
        JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-angle),
        JPH::EActivation::Activate);
    bodies.SetLinearVelocity(vehicleBodies[index],JPH::Vec3::sZero());
    bodies.SetAngularVelocity(vehicleBodies[index],JPH::Vec3::sZero());
    vehicleSynced[index]=position;
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
    if(ragdolls.size()>=12){
        auto& old=ragdolls.front();auto& bodies=world->GetBodyInterface();
        for(auto& ped:game::peds)
            if(ped.id==old.pedId&&ped.pinned)ped.corpseVisualDelay=0;
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
    ragdolls.clear();game::ragdollParts.clear();
}
void step(float dt){
    if(!world)return;
    syncBuildingColliders(game::occupied>=0&&
        std::size_t(game::occupied)<game::vehicles.size()?
        game::vehicles[game::occupied].p:game::player);
    for(std::size_t index=0;index<pedCharacters.size()&&index<game::peds.size();++index)
        if(!game::peds[index].alive)pedCharacters[index]=nullptr;
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
    world->Update(dt,1,allocator.get(),jobs.get());
    for(std::size_t i=0;i<vehicleBodies.size()&&i<game::vehicles.size();++i){
        if(vehicleBodies[i].IsInvalid())continue;
        auto position=bodies.GetCenterOfMassPosition(vehicleBodies[i]);
        auto velocity=bodies.GetLinearVelocity(vehicleBodies[i]);
        auto& vehicle=game::vehicles[i];
        game::Vec2 resolved{velocity.GetX(),velocity.GetZ()};
        float collisionSpeed=game::len(vehicle.velocity-resolved);
        if(collisionSpeed>60&&!vehicle.exploded&&vehicle.collisionCooldown<=0){
            game::damageVehicle(int(i),(collisionSpeed-60)*0.18f*
                physics::tuning(vehicle.kind).collisionDamageScale);
            vehicle.collisionCooldown=0.35f;
            if(collisionSpeed>135)
                if(int(i)==game::occupied)game::applyDamage((collisionSpeed-135)*0.045f);
        }
        vehicle.p={position.GetX(),position.GetZ()};
        vehicleSynced[i]=vehicle.p;
        vehicle.velocity=resolved;
        float restHeight=vehicle.kind==game::Kind::Boat?10.0f:
            vehicle.kind==game::Kind::Bike?13.0f:14.0f;
        vehicle.rideHeight=std::clamp(float(position.GetY())-restHeight,-6.0f,12.0f);
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
}
}
