#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include "excavation.h"
#include "terrain.h"
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include "masonry.h"
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
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
#include "builder.h"
#include "scenery_edits.h"
#include <map>
#include <set>
#include "regions.h"
#include "physics.h"
#include "ai.h"
#include "wildlife.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <stdexcept>
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
std::vector<std::uint64_t> treeRevisions;
struct RockBody {JPH::BodyID id;std::uint64_t revision=0;};
std::map<std::string,RockBody> rockBodies;
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
std::vector<std::string> pedCharacterIds;
bool currentPedCharacter(std::size_t index){
    return index<game::peds.size()&&index<pedCharacters.size()&&pedCharacters[index]&&
        index<pedCharacterIds.size()&&pedCharacterIds[index]==game::peds[index].id&&game::peds[index].elevationKnown;
}
void rememberPed(std::size_t index){
    if(!currentPedCharacter(index))return;auto& ped=game::peds[index];auto p=pedCharacters[index]->GetPosition();
    if(game::len(game::Vec2{float(p.GetX()),float(p.GetZ())}-ped.p)>=25)return;
    ped.elevation=float(p.GetY());ped.elevationVelocity=pedCharacters[index]->GetLinearVelocity().GetY();ped.elevationAt=ped.p;
}
std::vector<JPH::BodyID> staticBodies;
std::vector<JPH::BodyID> terrainBodies;
struct TerrainChunk {JPH::BodyID id;std::uint64_t revision=0;};
std::map<std::pair<int,int>,TerrainChunk> terrainChunks;
std::uint64_t terrainMaskRevision=~std::uint64_t(0),terrainBaseRevision=~std::uint64_t(0);
void syncTerrainColliders();
std::vector<JPH::BodyID> buildingBodies;
std::vector<JPH::BodyID> builderBodies;
std::uint64_t builderRevision=~std::uint64_t(0);
int builderCellX=-1,builderCellZ=-1;
void syncBuilderColliders(game::Vec2 focus){
    if(!world)return;int cx=int(std::floor(focus.x/400)),cz=int(std::floor(focus.z/400));
    if(builderRevision==builder::revision()&&cx==builderCellX&&cz==builderCellZ)return;
    for(auto& id:builderBodies)destroyBody(id);builderBodies.clear();
    builderRevision=builder::revision();builderCellX=cx;builderCellZ=cz;
    if(!builder::active())return;
    // One compound per local chunk keeps body counts bounded as structures grow.
    std::map<std::pair<int,int>,JPH::StaticCompoundShapeSettings> chunks;
    for(const auto& block:builder::blocks()){
        // Cached focus can move across a 400-unit cell on both axes. Include
        // that diagonal drift plus a block/capsule margin around builder AI.
        auto low=builder::cellLow(block.first);if(game::len(game::Vec2{low.x,low.z}-focus)>1900)continue;
        auto center=low+game::Vec3{20,20,20};
        chunks[{block.first.x/16,block.first.z/16}].AddShape(JPH::Vec3(center.x,center.y,center.z),JPH::Quat::sIdentity(),new JPH::BoxShape(JPH::Vec3(20,20,20),.05f));
    }
    for(auto& chunk:chunks){auto shape=chunk.second.Create();if(shape.HasError())continue;
        JPH::BodyCreationSettings settings(shape.Get(),JPH::RVec3::sZero(),JPH::Quat::sIdentity(),JPH::EMotionType::Static,Layer::staticBody);
        settings.mFriction=.9f;auto id=world->GetBodyInterface().CreateAndAddBody(settings,JPH::EActivation::DontActivate);if(!id.IsInvalid())builderBodies.push_back(id);}
}
int streamedCellX=-1,streamedCellZ=-1;
struct Ragdoll {
    std::string pedId;
    std::vector<JPH::BodyID> bodies;
    std::vector<JPH::Ref<JPH::TwoBodyConstraint>> joints;
    float life=6;
    int style=0;
    game::Vec3 origin{},rest[6]{};
    float yaw=0;
    JPH::Ref<JPH::TwoBodyConstraint> pin;
    game::Vec3 pinPoint{};
};
std::vector<Ragdoll> ragdolls;
SceneryRecovery recoveryStats{};
struct RagdollGeometry {float x,y,z,w,h,d;};
constexpr RagdollGeometry ragdollGeometry[]={
    {0,25,0,10,16,7},{0,37,0,8,8,8},{-8,24,0,4,13,4},
    {8,24,0,4,13,4},{-3,10,0,5,16,5},{3,10,0,5,16,5}};
struct RecoveryProbe {JPH::RefConst<JPH::Shape> shape;JPH::RMat44 transform;};
std::vector<RecoveryProbe> bodyProbes(const std::vector<JPH::BodyID>& ids){
    std::vector<RecoveryProbe> result;for(auto id:ids){JPH::BodyLockRead lock(world->GetBodyLockInterface(),id);
        if(lock.Succeeded())result.push_back({lock.GetBody().GetShape(),lock.GetBody().GetCenterOfMassTransform()});}return result;
}
JPH::AABox recoveryBounds(const std::vector<RecoveryProbe>& probes){
    JPH::AABox bounds;for(const auto& probe:probes)bounds.Encapsulate(probe.shape->GetWorldSpaceBounds(probe.transform,JPH::Vec3::sOne()));return bounds;
}
bool recoveryClear(const std::vector<RecoveryProbe>& probes,game::Vec3 delta){
    JPH::CollideShapeSettings settings;settings.mBackFaceMode=JPH::EBackFaceMode::CollideWithBackFaces;
    for(const auto& probe:probes){auto transform=probe.transform.PostTranslated(JPH::RVec3(delta.x,delta.y,delta.z));auto p=transform.GetTranslation();
        // A one-sided heightfield cannot report a shape wholly buried below it.
        if(!regions::waterAt({p.GetX(),p.GetZ()})&&terrain::contains({p.GetX(),p.GetY(),p.GetZ()}))return false;
        JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> hits;
        world->GetNarrowPhaseQuery().CollideShape(probe.shape,JPH::Vec3::sOne(),transform,settings,p,hits,
            JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0)),JPH::SpecifiedObjectLayerFilter(Layer::staticBody));
        if(hits.HadHit()&&hits.mHit.mPenetrationDepth>.35f)return false;
    }return true;
}
bool recoveryOffset(const std::vector<RecoveryProbe>& probes,game::Vec3& result,bool groundOnly=false){
    result={};if(probes.empty())return true;auto bounds=recoveryBounds(probes);auto center=bounds.GetCenter();float lift=0;
    auto clear=[&](game::Vec3 delta){
        if(groundOnly){JPH::AABox candidate(bounds.mMin+JPH::Vec3(delta.x,delta.y,delta.z),bounds.mMax+JPH::Vec3(delta.x,delta.y,delta.z));
            // Legacy corpse animations have no saved altitude. Their escape
            // search may cross streaming boundaries, so include global edits.
            auto intersects=[&](game::Vec3 low,game::Vec3 high){return candidate.Overlaps(JPH::AABox(JPH::Vec3(low.x,low.y,low.z),JPH::Vec3(high.x,high.y,high.z)));};
            for(const auto& building:game::buildings)for(const auto& box:destruction::boxes(building))if(intersects(box.low,box.high))return false;
            if(builder::active())for(const auto& block:builder::blocks()){auto low=builder::cellLow(block.first);if(intersects(low,low+game::Vec3{40,40,40}))return false;}}
        return recoveryClear(probes,delta);
    };
    if(clear({}))return true;
    for(const auto& probe:probes){auto p=probe.transform.GetTranslation();if(!regions::waterAt({p.GetX(),p.GetZ()})&&terrain::contains({p.GetX(),p.GetY(),p.GetZ()}))
        lift=std::max(lift,terrain::baseHeight({p.GetX(),p.GetZ()})-bounds.mMin.GetY()+.5f);}
    if(!groundOnly&&lift>0&&clear({0,lift,0})){result={0,lift,0};return true;}
    // Bounded local search keeps the actor near its current position. Query all
    // parts together so a ragdoll keeps its pose and internal joint anchors.
    for(int ring=1;ring<=(groundOnly?100:8);++ring)for(int side=0;side<16;++side){float angle=side*game::PI/8;
        float dx=std::cos(angle)*ring*40,dz=std::sin(angle)*ring*40;
        if(bounds.mMin.GetX()+dx<1||bounds.mMin.GetZ()+dz<1||bounds.mMax.GetX()+dx>regions::WIDTH-1||bounds.mMax.GetZ()+dz>regions::DEPTH-1||
            regions::waterAt({center.GetX()+dx,center.GetZ()+dz}))continue;
        float floor=-10000;for(float x:{bounds.mMin.GetX(),center.GetX(),bounds.mMax.GetX()})for(float z:{bounds.mMin.GetZ(),center.GetZ(),bounds.mMax.GetZ()})
            floor=std::max(floor,terrain::height({x+dx,z+dz}));
        game::Vec3 delta{dx,floor+.5f-bounds.mMin.GetY(),dz};if(clear(delta)){result=delta;return true;}
    }
    // An enclosed actor can be lifted onto the structure rather than deleted.
    if(!groundOnly)for(int step=1;step<=96;++step){game::Vec3 delta{0,lift+step*40,0};if(clear(delta)){result=delta;return true;}}
    return false;
}
float corpseDrop(const std::vector<RecoveryProbe>& probes,game::Vec3 delta){
    constexpr float distance=4000;float fraction=1;bool found=false;
    for(const auto& probe:probes){auto transform=probe.transform.PostTranslated(JPH::RVec3(delta.x,delta.y,delta.z));
        JPH::RShapeCast cast(probe.shape,JPH::Vec3::sOne(),transform,JPH::Vec3(0,-distance,0));JPH::ShapeCastSettings settings;
        JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> hits;
        // A settled corpse can also rest on an ordinary crate or vehicle.
        // Include their dynamic bodies rather than dropping through them.
        world->GetNarrowPhaseQuery().CastShape(cast,settings,transform.GetTranslation(),hits);
        if(hits.HadHit()){found=true;fraction=std::min(fraction,hits.mHit.mFraction);}}
    return found?std::max(0.0f,fraction*distance-.35f):0;
}
void publishRagdollPoses(){
    game::ragdollParts.clear();auto& bodies=world->GetBodyInterface();
    for(const auto& ragdoll:ragdolls){if(!ragdoll.bodies.empty())for(auto& ped:game::peds)if(ped.id==ragdoll.pedId&&!ped.alive){
            auto p=bodies.GetCenterOfMassPosition(ragdoll.bodies[0]);ped.p={p.GetX(),p.GetZ()};break;}
        for(std::size_t i=0;i<ragdoll.bodies.size();++i){auto p=bodies.GetCenterOfMassPosition(ragdoll.bodies[i]);auto q=bodies.GetRotation(ragdoll.bodies[i]);
            game::ragdollParts.push_back({{p.GetX(),p.GetY(),p.GetZ()},ragdoll.rest[i],ragdoll.origin,q.GetX(),q.GetY(),q.GetZ(),q.GetW(),ragdoll.yaw,ragdoll.style,int(i)});}}
    for(const auto& snapshot:game::corpseSnapshots)game::ragdollParts.insert(game::ragdollParts.end(),snapshot.parts.begin(),snapshot.parts.end());
}
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
void syncTerrainColliders(){
    if(!world)return;
    bool baseChanged=terrainMaskRevision!=excavation::maskRevision()||terrainBaseRevision!=terrain::revision();
    if(baseChanged){
        std::vector<JPH::BodyID> fresh;
        try{
            auto box=[&](game::Vec3 center,game::Vec3 half){auto id=createStatic(center,half);
                if(id.IsInvalid())throw std::runtime_error("No body available for editable terrain");fresh.push_back(id);};
            if(builder::active()&&!excavation::patches().empty()){
                for(float z=0;z<game::SHORE;z+=200)for(float x=0;x<game::WORLD_W;x+=200){
                    if(excavation::converted({int(x/200),int(z/200)}))continue;
                    float w=std::min(200.0f,game::WORLD_W-x),d=std::min(200.0f,game::SHORE-z);
                    box({x+w*.5f,-5,z+d*.5f},{w*.5f,5,d*.5f});
                }
            }else box({game::WORLD_W*.5f,-5,game::SHORE*.5f},{game::WORLD_W*.5f,5,game::SHORE*.5f});
            box({game::WORLD_W*.5f,-20,(game::SHORE+game::WORLD_D)*.5f},{game::WORLD_W*.5f,5,(game::WORLD_D-game::SHORE)*.5f});
            box({1200,-5,(game::SHORE+game::WORLD_D)*.5f},{60,5,(game::WORLD_D-game::SHORE)*.5f});
            auto samples=terrain::heights();
            for(unsigned z=0;z<terrain::samples;++z)for(unsigned x=0;x<terrain::samples;++x)
                if(regions::waterAt({x*50.0f,z*50.0f}))samples[z*terrain::samples+x]=-15;
            if(builder::active())for(const auto& patch:excavation::patches())
                for(int z=patch.first.z*4;z<=patch.first.z*4+4;++z)for(int x=patch.first.x*4;x<=patch.first.x*4+4;++x)
                    samples[z*terrain::samples+x]=JPH::HeightFieldShapeConstants::cNoCollisionValue;
            JPH::HeightFieldShapeSettings surface(samples.data(),JPH::Vec3::sZero(),JPH::Vec3(50,1,50),terrain::samples);
            surface.mBlockSize=4;surface.mBitsPerSample=16;auto shape=surface.Create();
            if(shape.HasError())throw std::runtime_error(shape.GetError().c_str());
            JPH::BodyCreationSettings settings(shape.Get(),JPH::RVec3::sZero(),JPH::Quat::sIdentity(),JPH::EMotionType::Static,Layer::staticBody);
            settings.mFriction=.85f;auto id=world->GetBodyInterface().CreateAndAddBody(settings,JPH::EActivation::DontActivate);
            if(id.IsInvalid())throw std::runtime_error("No heightfield body available");fresh.push_back(id);
        }catch(...){for(auto& id:fresh)destroyBody(id);throw;}
        for(auto& id:terrainBodies)destroyBody(id);terrainBodies=std::move(fresh);
        terrainMaskRevision=excavation::maskRevision();terrainBaseRevision=terrain::revision();
    }
    std::map<std::pair<int,int>,std::vector<excavation::Patch>> groups;
    if(builder::active())for(const auto& patch:excavation::patches())groups[{patch.first.x/4,patch.first.z/4}].push_back(patch.first);
    auto wake=[&](std::pair<int,int> key){world->GetBodyInterface().ActivateBodiesInAABox(
        JPH::AABox(JPH::Vec3(key.first*800.0f-50,-450,key.second*800.0f-50),JPH::Vec3((key.first+1)*800.0f+50,1500,(key.second+1)*800.0f+50)),
        world->GetDefaultBroadPhaseLayerFilter(Layer::moving),world->GetDefaultLayerFilter(Layer::moving));};
    for(auto it=terrainChunks.begin();it!=terrainChunks.end();)if(!groups.count(it->first)){destroyBody(it->second.id);wake(it->first);it=terrainChunks.erase(it);}else ++it;
    for(const auto& group:groups){std::uint64_t revision=0;for(auto patch:group.second)revision=std::max(revision,excavation::patches().at(patch));
        auto& chunk=terrainChunks[group.first];if(!baseChanged&&chunk.revision==revision)continue;
        JPH::TriangleList triangles;
        for(auto patch:group.second){auto vertices=excavation::collisionTriangles(patch);
            for(std::size_t i=0;i<vertices.size();i+=3){auto a=vertices[i],b=vertices[i+1],c=vertices[i+2];
                triangles.emplace_back(JPH::Float3(a.x,a.y,a.z),JPH::Float3(b.x,b.y,b.z),JPH::Float3(c.x,c.y,c.z));}}
        JPH::MeshShapeSettings surface(triangles);surface.mBuildQuality=JPH::MeshShapeSettings::EBuildQuality::FavorBuildSpeed;auto shape=surface.Create();
        if(shape.HasError())throw std::runtime_error(shape.GetError().c_str());
        JPH::BodyCreationSettings settings(shape.Get(),JPH::RVec3::sZero(),JPH::Quat::sIdentity(),JPH::EMotionType::Static,Layer::staticBody);
        settings.mFriction=.9f;auto id=world->GetBodyInterface().CreateAndAddBody(settings,JPH::EActivation::DontActivate);
        if(id.IsInvalid())throw std::runtime_error("No excavation chunk body available");
        destroyBody(chunk.id);chunk.id=id;chunk.revision=revision;wake(group.first);
    }
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
        if(!game::peds[index].alive||game::len(game::peds[index].p-focus)>(builder::active()?1200.0f:650.0f)){
            rememberPed(index);pedCharacters[index]=nullptr;}
    const float radius=builder::active()?1900.0f:1200.0f;
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
JPH::Ref<JPH::CharacterVirtual> makePedCharacter(game::Ped& ped){
    if(!world||!pedestrianSettings)return nullptr;
    auto point=ped.p;bool known=ped.elevationKnown&&game::len(point-ped.elevationAt)<25;
    float height=known?ped.elevation:terrain::height(point),vertical=known?ped.elevationVelocity:0;
    // An offscreen actor may return after its old layer has disappeared. Repair
    // a buried/overlapping pose against the currently streamed world, while
    // retaining a valid underground floor and in-progress fall.
    if(!pedestrianClear({point.x,height,point.z})){
        auto floors=pedestrianFloors(point);float nearest=1e30f,requested=height;
        for(float floor:floors)if(std::abs(floor-requested)<nearest){nearest=std::abs(floor-requested);height=floor;vertical=0;}
        if(nearest==1e30f&&terrain::contains({point.x,height+1,point.z})){height=terrain::height(point);vertical=0;}
    }
    auto character=new JPH::CharacterVirtual(pedestrianSettings,
        JPH::RVec3(point.x,height,point.z),JPH::Quat::sIdentity(),
        Layer::moving,world.get());
    character->SetLinearVelocity(JPH::Vec3(0,vertical,0));
    ped.elevation=height;ped.elevationVelocity=vertical;ped.elevationAt=point;ped.elevationKnown=true;
    return character;
}
void restorePedCharacters(game::Vec2 focus){
    if(!world||!pedestrianSettings)return;
    pedCharacters.resize(game::peds.size());pedCharacterIds.resize(game::peds.size());
    float radius=builder::active()?1200.0f:500.0f;
    for(std::size_t index=0;index<game::peds.size();++index){
        auto& ped=game::peds[index];
        if(!ped.alive||ped.drivingVehicle>=0||game::len(ped.p-focus)>radius)continue;
        if(currentPedCharacter(index))continue;
        // All incoming static collision must exist before restoring a pose.
        // Recreate before rendering, even if this actor has not moved yet.
        pedCharacters[index]=makePedCharacter(ped);pedCharacterIds[index]=ped.id;
    }
}
}
void shutdown(){
    if(world){
        for(auto& id:terrainBodies)destroyBody(id);
        for(auto& chunk:terrainChunks)destroyBody(chunk.second.id);
        for(auto& id:builderBodies)destroyBody(id);
        auto& bodies=world->GetBodyInterface();
        playerCharacter=nullptr;
        pedCharacters.clear();
        pedCharacterIds.clear();
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
        for(auto& rock:rockBodies)destroyBody(rock.second.id);
        for(auto& id:animalBodies)destroyBody(id);
        for(auto& fragment:fragmentBodies)destroyBody(fragment.id);
    }
    ragdolls.clear();game::ragdollParts.clear();game::corpseSnapshots.clear();
    propBodies.clear();vehicleBodies.clear();vehicleConstraints.clear();vehicleSynced.clear();
    staticBodies.clear();buildingBodies.clear();streamedCellX=streamedCellZ=-1;
    terrainBodies.clear();terrainChunks.clear();terrainMaskRevision=terrainBaseRevision=~std::uint64_t(0);
    builderBodies.clear();builderRevision=~std::uint64_t(0);builderCellX=builderCellZ=-1;
    treeBodies.clear();treeRevisions.clear();rockBodies.clear();animalBodies.clear();animalBodySpecies.clear();
    treeImpacts.clear();animalImpacts.clear();fragmentBodies.clear();fragmentVisuals.clear();
    world.reset();jobs.reset();allocator.reset();
    recoveryStats={};
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
    syncTerrainColliders();
    addStatic({7800,-20,regions::DEPTH*0.5f},{200,5,regions::DEPTH*0.5f});
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
            JPH::RVec3(vehicle.p.x,terrain::height(vehicle.p)+physics::vehicleRestHeight(vehicle.kind),vehicle.p.z),
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
    game::playerY=std::max(game::playerY,terrain::height(game::player));
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
    pedCharacterIds.resize(game::peds.size());
    for(std::size_t index=0;index<game::peds.size();++index)
        if(game::peds[index].alive&&game::peds[index].drivingVehicle<0&&game::len(game::peds[index].p-game::player)<(builder::active()?1200.0f:500.0f)){
            pedCharacters[index]=makePedCharacter(game::peds[index]);pedCharacterIds[index]=game::peds[index].id;}
    world->OptimizeBroadPhase();
}
void addPed(){
    if(!world||pedCharacters.size()>=game::peds.size())return;
    std::size_t previous=pedCharacters.size();
    pedCharacters.resize(game::peds.size());
    pedCharacterIds.resize(game::peds.size());
    for(std::size_t index=previous;index<game::peds.size();++index)
        if(game::peds[index].alive&&game::peds[index].drivingVehicle<0&&game::len(game::peds[index].p-game::player)<(builder::active()?1200.0f:500.0f)){
            pedCharacters[index]=makePedCharacter(game::peds[index]);pedCharacterIds[index]=game::peds[index].id;}
}
void moveCharacter(game::Vec2 horizontal,bool jump,float dt){
    if(!playerCharacter||!world)return;
    syncTerrainColliders();
    syncBuilderColliders(game::player);
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
        game::playerY=std::max(terrain::height(playable),game::playerY);
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
        if(jump&&supported)vertical=builder::active()?260.0f:230.0f;
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
        playerCharacter->SetPosition(JPH::RVec3(safeX,float(position.GetY()),safeZ));
        position=playerCharacter->GetPosition();
    }
    if(!swimming&&game::occupied<0){
        game::Vec2 candidate{float(position.GetX()),float(position.GetZ())};
        for(auto& ped:game::peds){
            if(!ped.alive||ped.drivingVehicle>=0||
               std::abs(float(position.GetY())-pedHeight(ped))>=25||
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
    game::playerY=swimming?float(position.GetY()):float(position.GetY());
    game::playerVerticalSpeed=playerCharacter->GetLinearVelocity().GetY();
    game::grounded=!swimming&&playerCharacter->IsSupported();
    game::swimming=swimming;
    if(!swimming&&!supported&&game::grounded&&vertical<-380)
        game::applyDamage((std::abs(vertical)-380)*0.06f);
}
bool staticAnchor(game::Vec3 origin,game::Vec3 direction,float range,game::Vec3& point){
    if(!world||range<=0)return false;
    syncTerrainColliders();syncBuilderColliders({origin.x,origin.z});
    syncBuildingColliders({origin.x,origin.z});syncSceneryColliders({origin.x,origin.z});
    JPH::RRayCast ray(JPH::RVec3(origin.x,origin.y,origin.z),
        JPH::Vec3(direction.x*range,direction.y*range,direction.z*range));
    JPH::RayCastResult hit;
    if(!world->GetNarrowPhaseQuery().CastRay(ray,hit))return false;
    JPH::BodyLockRead lock(world->GetBodyLockInterface(),hit.mBodyID);
    if(!lock.Succeeded()||!lock.GetBody().IsStatic())return false;
    auto position=ray.GetPointOnRay(hit.mFraction);
    point={float(position.GetX()),float(position.GetY()),float(position.GetZ())};
    return point.y>5||(builder::active()&&!excavation::cells().empty()&&point.y<terrain::baseHeight({point.x,point.z})-.1f);
}
void moveGrappleCharacter(game::Vec3 velocity,float dt){
    if(!world||!playerCharacter)return;
    syncTerrainColliders();syncBuilderColliders(game::player);
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
    game::playerY=float(position.GetY());
    game::playerVerticalSpeed=playerCharacter->GetLinearVelocity().GetY();
    game::grounded=playerCharacter->IsSupported();game::swimming=false;
}
void teleportCharacter(game::Vec2 position,float height){
    if(!playerCharacter)return;
    syncTerrainColliders();syncBuilderColliders(position);
    syncBuildingColliders(position);
    syncSceneryColliders(position);
    restorePedCharacters(position);
    height=regions::waterAt(position)?height:std::max(height,excavation::floorBelow({position.x,height+.1f,position.z}));
    playerCharacter->SetPosition(JPH::RVec3(position.x,height,position.z));
    playerCharacter->SetLinearVelocity(JPH::Vec3::sZero());
}
float pedHeight(std::size_t index){
    if(index>=game::peds.size())return 0;
    const auto& ped=game::peds[index];
    if(ped.drivingVehicle>=0&&std::size_t(ped.drivingVehicle)<game::vehicles.size())return game::vehicles[ped.drivingVehicle].rideHeight;
    if(currentPedCharacter(index)&&ped.drivingVehicle<0){
        auto position=pedCharacters[index]->GetPosition();
        if(game::len(game::Vec2{float(position.GetX()),float(position.GetZ())}-ped.p)<25)return float(position.GetY());
    }
    if(ped.elevationKnown&&game::len(ped.p-ped.elevationAt)<25)return ped.elevation;
    return terrain::height(ped.p);
}
float pedHeight(const game::Ped& ped){
    for(std::size_t n=0;n<game::peds.size();++n)if(&game::peds[n]==&ped)return pedHeight(n);
    if(ped.elevationKnown&&game::len(ped.p-ped.elevationAt)<25)return ped.elevation;
    return terrain::height(ped.p);
}
void teleportPed(std::size_t index,game::Vec2 position,float height){
    if(index>=game::peds.size())return;
    auto& ped=game::peds[index];ped.p=position;ped.elevation=height;ped.elevationVelocity=0;ped.elevationAt=position;ped.elevationKnown=true;
    if(currentPedCharacter(index)){
        pedCharacters[index]->SetPosition(JPH::RVec3(position.x,height,position.z));
        pedCharacters[index]->SetLinearVelocity(JPH::Vec3::sZero());
    }
}
void preparePedNavigation(){
    if(!world)return;
    syncTerrainColliders();syncBuilderColliders(game::player);
    syncBuildingColliders(game::player);syncSceneryColliders(game::player);
    restorePedCharacters(game::player);
}
bool pedestrianClear(game::Vec3 feet,bool includeDynamic){
    if(!world||!pedestrianSettings)return false;
    // The capsule's bottom is one unit above its origin. A settled character
    // can land exactly on the voxel boundary; allow the same small contact
    // tolerance as the shape query rather than classifying roundoff as burial.
    if(terrain::contains(feet+game::Vec3{0,1.05f,0}))return false;
    auto center=JPH::RVec3(feet.x,feet.y+16,feet.z);
    JPH::CollideShapeSettings settings;settings.mBackFaceMode=JPH::EBackFaceMode::CollideWithBackFaces;
    JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> hits;
    if(includeDynamic)world->GetNarrowPhaseQuery().CollideShape(pedestrianSettings->mShape,JPH::Vec3::sOne(),
        JPH::RMat44::sTranslation(center),settings,center,hits);
    else world->GetNarrowPhaseQuery().CollideShape(pedestrianSettings->mShape,JPH::Vec3::sOne(),
        JPH::RMat44::sTranslation(center),settings,center,hits,
        JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0)),JPH::SpecifiedObjectLayerFilter(Layer::staticBody));
    return !hits.HadHit()||hits.mHit.mPenetrationDepth<.2f;
}
bool standingCharacterClear(game::Vec3 feet){
    if(!world||!standingShape||terrain::contains(feet+game::Vec3{0,.3f,0}))return false;
    auto center=JPH::RVec3(feet.x,feet.y+18,feet.z);JPH::CollideShapeSettings settings;
    settings.mBackFaceMode=JPH::EBackFaceMode::CollideWithBackFaces;
    JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> hits;
    world->GetNarrowPhaseQuery().CollideShape(standingShape,JPH::Vec3::sOne(),JPH::RMat44::sTranslation(center),settings,center,hits,
        JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0)),JPH::SpecifiedObjectLayerFilter(Layer::staticBody));
    return !hits.HadHit()||hits.mHit.mPenetrationDepth<.2f;
}
std::vector<float> pedestrianFloors(game::Vec2 point,unsigned* queryCount,unsigned queryLimit){
    std::vector<float> floors;if(!world||regions::waterAt(point))return floors;
    auto takeQuery=[&](){if(queryCount){if(*queryCount>=queryLimit)return false;++*queryCount;}return true;};
    std::vector<float> tested;
    // Exact voxel seams can fall between both triangles' ray edge tests.
    // Sample both sides by a tiny amount, project onto the original column,
    // then validate the full capsule there. Footprint checks still reject gaps.
    auto seam=[](float value){return std::abs(value-40*std::round(value/40))<.001f;};
    int samples=seam(point.x)||seam(point.z)?3:1;
    for(int sample=0;sample<samples;++sample){
    if(!takeQuery())break;float offset=sample==0?0:sample==1?.02f:-.02f;
    JPH::RRayCast ray(JPH::RVec3(point.x+offset,3500,point.z+offset*.65f),JPH::Vec3(0,-3950,0));
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> hits;JPH::RayCastSettings settings;
    world->GetNarrowPhaseQuery().CastRay(ray,settings,hits,
        JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0)),JPH::SpecifiedObjectLayerFilter(Layer::staticBody));
    for(const auto& hit:hits.mHits){
        auto p=ray.GetPointOnRay(hit.mFraction);JPH::Vec3 normal;
        {JPH::BodyLockRead lock(world->GetBodyLockInterface(),hit.mBodyID);
            if(!lock.Succeeded())continue;normal=lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,p);}
        if(normal.GetY()<std::cos(game::PI*.28f))continue;
        // On slopes a vertical capsule's bottom sphere sits above the center
        // ray's plane. Match its radius/half-height/offset rather than treating
        // every otherwise-walkable incline as a penetrating capsule.
        float plane=float(p.GetY())+(normal.GetX()*offset+normal.GetZ()*offset*.65f)/normal.GetY();
        float feet=plane+std::max(0.0f,8/normal.GetY()-9)+.05f;
        bool duplicate=false;for(float floor:tested)duplicate|=std::abs(floor-feet)<.3f;
        if(duplicate)continue;tested.push_back(feet);if(!takeQuery())break;
        if(pedestrianClear({point.x,feet,point.z}))floors.push_back(feet);
    }
    }
    std::sort(floors.begin(),floors.end());return floors;
}
void movePed(std::size_t index,game::Vec2 horizontal,float dt){
    if(!world||index>=pedCharacters.size()||index>=game::peds.size())return;
    if(builder::active()&&game::len(game::peds[index].p-game::player)>1200)return;
    syncTerrainColliders();syncBuilderColliders(game::player);
    auto& character=pedCharacters[index];
    auto& ped=game::peds[index];
    if(!currentPedCharacter(index)){character=makePedCharacter(ped);pedCharacterIds[index]=ped.id;}
    if(!character)return;
    auto position=character->GetPosition();
    bool teleported=game::len(game::Vec2{float(position.GetX()),float(position.GetZ())}-ped.p)>25;
    if(teleported){float height=ped.elevationKnown&&game::len(ped.p-ped.elevationAt)<25?ped.elevation:terrain::height(ped.p);
        character->SetPosition(JPH::RVec3(ped.p.x,height,ped.p.z));}
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
    if(game::health>0&&game::occupied<0&&std::abs(game::playerY-float(position.GetY()))<25&&
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
           std::abs(float(position.GetY())-pedHeight(other))>=25||
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
    rememberPed(index);
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
    const float sceneryRadius=builder::active()?1400.0f:1100.0f;
    if(treeBodies.size()!=game::trees.size()){
        for(auto& id:treeBodies)destroyBody(id);
        treeBodies.assign(game::trees.size(),JPH::BodyID());
        treeRevisions.assign(game::trees.size(),0);
    }
    // Streaming bounds both the broad phase and physics body count in groves.
    for(std::size_t i=0;i<treeBodies.size();++i){
        const auto& tree=game::trees[i];auto& id=treeBodies[i];
        bool nearby=!tree.destroyed&&game::len(tree.p-focus)<sceneryRadius;
        if(!nearby){destroyBody(id);treeRevisions[i]=0;continue;}
        float radius=treeRadius(tree),halfHeight=std::max(8.0f,tree.height*tree.scale*0.3f);
        auto revision=scenery_edits::revision(scenery_edits::treeId(i));
        if(treeRevisions[i]!=revision){
            JPH::RefConst<JPH::Shape> shape=new JPH::CylinderShape(halfHeight,radius);
            if(scenery_edits::edited(scenery_edits::treeId(i))){
                JPH::StaticCompoundShapeSettings compound;unsigned count=0;
                for(auto box:scenery_edits::trunkPieces(i)){
                    // Clip the original circular trunk, rather than replacing it with box corners.
                    std::vector<game::Vec2> polygon;for(int n=0;n<24;++n){float angle=n*game::PI/12;polygon.push_back({tree.p.x+std::cos(angle)*radius,tree.p.z+std::sin(angle)*radius});}
                    auto clip=[&](int axis,float plane,bool lower){std::vector<game::Vec2> output;if(polygon.empty())return;
                        auto distance=[&](game::Vec2 p){return lower?(axis?p.z:p.x)-plane:plane-(axis?p.z:p.x);};
                        auto previous=polygon.back();float before=distance(previous);for(auto current:polygon){float after=distance(current);
                            if((before>=0)!=(after>=0))output.push_back(previous+(current-previous)*(before/(before-after)));
                            if(after>=0)output.push_back(current);previous=current;before=after;}polygon=std::move(output);};
                    clip(0,box.low.x,true);clip(0,box.high.x,false);clip(1,box.low.z,true);clip(1,box.high.z,false);if(polygon.size()<3)continue;
                    auto center=(box.low+box.high)*.5f;JPH::Array<JPH::Vec3> points;
                    for(auto p:polygon)for(float y:{box.low.y,box.high.y})points.push_back(JPH::Vec3(p.x-center.x,y-center.y,p.z-center.z));
                    JPH::ConvexHullShapeSettings hull(points,.01f);auto result=hull.Create();if(result.HasError())throw std::runtime_error(result.GetError().c_str());
                    compound.AddShape(JPH::Vec3(center.x-tree.p.x,center.y-terrain::baseHeight(tree.p)-halfHeight,center.z-tree.p.z),JPH::Quat::sIdentity(),result.Get());++count;
                }
                if(!count){destroyBody(id);treeRevisions[i]=revision;continue;}
                auto result=compound.Create();if(result.HasError())throw std::runtime_error(result.GetError().c_str());shape=result.Get();
            }
            JPH::BodyCreationSettings settings(shape,
                JPH::RVec3(tree.p.x,terrain::baseHeight(tree.p)+halfHeight,tree.p.z),JPH::Quat::sIdentity(),
                JPH::EMotionType::Static,Layer::staticBody);
            settings.mFriction=0.8f;settings.mUserData=treeTag|i;
            auto fresh=bodies.CreateAndAddBody(settings,JPH::EActivation::DontActivate);
            if(fresh.IsInvalid())throw std::runtime_error("No edited tree body available");
            destroyBody(id);id=fresh;treeRevisions[i]=revision;
        }
    }
    std::set<std::string> nearbyRocks;
    if(builder::active())for(const auto& object:scenery_edits::nearby(focus,sceneryRadius))if(object.kind==scenery_edits::Kind::Rock){
        if(game::len(game::Vec2{object.position.x,object.position.z}-focus)>sceneryRadius+object.size.x)continue;
        nearbyRocks.insert(object.id);auto& body=rockBodies[object.id];auto revision=scenery_edits::revision(object.id);if(body.revision==revision)continue;
        auto vertices=scenery_edits::collisionTriangles(object);JPH::TriangleList triangles;
        for(std::size_t n=0;n+2<vertices.size();n+=3){auto a=vertices[n],b=vertices[n+1],c=vertices[n+2];triangles.emplace_back(JPH::Float3(a.x,a.y,a.z),JPH::Float3(b.x,b.y,b.z),JPH::Float3(c.x,c.y,c.z));}
        if(triangles.empty()){destroyBody(body.id);body.revision=revision;continue;}
        JPH::MeshShapeSettings surface(triangles);surface.mBuildQuality=JPH::MeshShapeSettings::EBuildQuality::FavorBuildSpeed;auto shape=surface.Create();
        if(shape.HasError())throw std::runtime_error(shape.GetError().c_str());
        JPH::BodyCreationSettings settings(shape.Get(),JPH::RVec3::sZero(),JPH::Quat::sIdentity(),JPH::EMotionType::Static,Layer::staticBody);settings.mFriction=.85f;
        auto fresh=bodies.CreateAndAddBody(settings,JPH::EActivation::DontActivate);if(fresh.IsInvalid())throw std::runtime_error("No scanned rock body available");
        destroyBody(body.id);body.id=fresh;body.revision=revision;
    }
    for(auto it=rockBodies.begin();it!=rockBodies.end();)if(!nearbyRocks.count(it->first)){destroyBody(it->second.id);it=rockBodies.erase(it);}else ++it;
    if(animalBodies.size()!=wildlife::animals.size()){
        for(auto& id:animalBodies)destroyBody(id);
        animalBodies.assign(wildlife::animals.size(),JPH::BodyID());
        animalBodySpecies.assign(wildlife::animals.size(),-1);
    }
    for(std::size_t i=0;i<animalBodies.size();++i){
        const auto& a=wildlife::animals[i];auto& id=animalBodies[i];
        if(a.health<=0||a.carried||game::len(a.p-focus)>1000){destroyBody(id);continue;}
        const auto& species=wildlife::species()[a.species];
        JPH::RVec3 target(a.p.x,wildlife::originHeight(a)+species.height*0.5f,a.p.z);
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
            terrain::baseHeight(tree.p)+(piece<4?2+height*(fraction+0.125f):height*(0.65f+fraction*0.3f)),
            tree.p.z+(piece<4?0:std::sin(angle)*radius*2)};
        if(scenery_edits::removed(scenery_edits::treeId(index),builder::cellAt(position)))continue;
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
        bodies.SetPosition(id,JPH::RVec3(vehicle.p.x,terrain::height(vehicle.p)+physics::vehicleRestHeight(vehicle.kind),vehicle.p.z),JPH::EActivation::Activate);
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
    vehicle.rideHeight=altitude==0?terrain::height(position):altitude;vehicle.verticalSpeed=0;
    vehicle.qx=vehicle.qz=0;vehicle.qy=std::sin((game::PI/2-angle)*.5f);vehicle.qw=std::cos((game::PI/2-angle)*.5f);
    if(!world||index>=vehicleBodies.size()||vehicleBodies[index].IsInvalid())return;
    auto& bodies=world->GetBodyInterface();
    float height=physics::vehicleRestHeight(vehicle.kind)+vehicle.rideHeight;
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
bool spawnRagdoll(const game::Ped& ped,game::Vec3 impulse,
    const game::Vec2* pinAnchor,bool fallen){
    if(!world)return false;
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
    Ragdoll ragdoll;ragdoll.style=ped.style;ragdoll.pedId=ped.id;
    if(pinAnchor)ragdoll.life=std::min(15.0f,ped.respawn);
    ragdoll.origin={ped.p.x,pedHeight(ped),ped.p.z};ragdoll.yaw=game::PI/2-ped.angle;
    float co=std::cos(ragdoll.yaw),si=std::sin(ragdoll.yaw);
    auto rotated=[&](float x,float z){return game::Vec2{co*x+si*z,-si*x+co*z};};
    auto facing=game::forward(ped.angle);
    JPH::Quat fallRotation=JPH::Quat::sRotation(JPH::Vec3(-facing.z,0,facing.x),-game::PI*.5f);
    auto posePoint=[&](game::Vec3 point){
        if(!fallen)return point;
        auto offset=fallRotation*JPH::Vec3(point.x-ragdoll.origin.x,point.y-ragdoll.origin.y-20,point.z-ragdoll.origin.z);
        return ragdoll.origin+game::Vec3{offset.GetX(),6+offset.GetY(),offset.GetZ()};
    };
    auto& bodies=world->GetBodyInterface();
    JPH::Body* created[6]{};
    for(int i=0;i<6;++i){
        const auto& part=ragdollGeometry[i];
        game::Vec2 offset=rotated(part.x,part.z);
        ragdoll.rest[i]={ped.p.x+offset.x,ragdoll.origin.y+part.y,ped.p.z+offset.z};
        auto point=posePoint(ragdoll.rest[i]);
        JPH::BodyCreationSettings settings(new JPH::BoxShape(JPH::Vec3(part.w/2,part.h/2,part.d/2)),
            JPH::RVec3(point.x,point.y,point.z),fallen?fallRotation:JPH::Quat::sIdentity(),
            JPH::EMotionType::Dynamic,Layer::moving);
        settings.mFriction=0.75f;settings.mRestitution=0.08f;
        settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass=i==0?18.0f:4.0f;
        created[i]=bodies.CreateBody(settings);
        if(!created[i]){for(auto id:ragdoll.bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}return false;}
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
        auto point=posePoint({ped.p.x+anchor.x,ragdoll.origin.y+anchors[i].y,ped.p.z+anchor.z});
        settings.mPoint1=settings.mPoint2=JPH::RVec3(point.x,point.y,point.z);
        settings.mMinDistance=0;settings.mMaxDistance=0.5f;
        JPH::Ref<JPH::TwoBodyConstraint> joint=settings.Create(*created[parent[i]],*created[child[i]]);
        world->AddConstraint(joint.GetPtr());ragdoll.joints.push_back(joint);
    }
    if(pinAnchor){
        JPH::DistanceConstraintSettings settings;
        settings.mPoint1=JPH::RVec3(pinAnchor->x,ragdoll.origin.y+25,pinAnchor->z);
        settings.mPoint2=JPH::RVec3(ped.p.x,ragdoll.origin.y+25,ped.p.z);
        settings.mMinDistance=0;settings.mMaxDistance=2;
        JPH::Ref<JPH::TwoBodyConstraint> joint=
            settings.Create(JPH::Body::sFixedToWorld,*created[0]);
        world->AddConstraint(joint.GetPtr());ragdoll.joints.push_back(joint);
        ragdoll.pin=joint;ragdoll.pinPoint={pinAnchor->x,ragdoll.origin.y+25,pinAnchor->z};
    }
    ragdolls.push_back(std::move(ragdoll));
    publishRagdollPoses();return true;
}
bool corpsePose(const game::Ped& ped,game::Vec3& low,game::Vec3& high,game::Vec3& contact){
    if(ped.carried){contact={ped.p.x,game::playerY+15,ped.p.z};low=contact-game::Vec3{12,5,18};high=contact+game::Vec3{12,5,18};return false;}
    if(world)for(const auto& ragdoll:ragdolls)if(ragdoll.pedId==ped.id&&!ragdoll.bodies.empty()){
        auto probes=bodyProbes(ragdoll.bodies);auto bounds=recoveryBounds(probes);
        low={bounds.mMin.GetX(),bounds.mMin.GetY(),bounds.mMin.GetZ()};high={bounds.mMax.GetX(),bounds.mMax.GetY(),bounds.mMax.GetZ()};
        auto point=world->GetBodyInterface().GetCenterOfMassPosition(ragdoll.bodies[0]);contact={point.GetX(),point.GetY(),point.GetZ()};return true;
    }
    for(const auto& snapshot:game::corpseSnapshots)if(snapshot.pedId==ped.id){
        JPH::AABox bounds;for(const auto& part:snapshot.parts){const auto& geometry=ragdollGeometry[part.part];JPH::BoxShape box(JPH::Vec3(geometry.w/2,geometry.h/2,geometry.d/2));
            bounds.Encapsulate(box.GetWorldSpaceBounds(JPH::RMat44::sRotationTranslation(JPH::Quat(part.qx,part.qy,part.qz,part.qw),JPH::RVec3(part.p.x,part.p.y,part.p.z)),JPH::Vec3::sOne()));}
        low={bounds.mMin.GetX(),bounds.mMin.GetY(),bounds.mMin.GetZ()};high={bounds.mMax.GetX(),bounds.mMax.GetY(),bounds.mMax.GetZ()};contact=snapshot.parts[0].p;return true;
    }
    float y=terrain::height(ped.p);contact={ped.p.x,y+5,ped.p.z};low={ped.p.x-12,y,ped.p.z-18};high={ped.p.x+12,y+10,ped.p.z+18};return false;
}
bool corpseDropClear(game::Vec3 feet,float angle){
    if(!world||terrain::contains(feet+game::Vec3{0,.3f,0}))return false;
    JPH::BoxShape shape(JPH::Vec3(12,7,23),.1f);auto center=JPH::RVec3(feet.x,feet.y+7.3f,feet.z);
    JPH::CollideShapeSettings settings;settings.mBackFaceMode=JPH::EBackFaceMode::CollideWithBackFaces;
    JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> hits;
    world->GetNarrowPhaseQuery().CollideShape(&shape,JPH::Vec3::sOne(),JPH::RMat44::sRotationTranslation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-angle),center),settings,center,hits);
    return !hits.HadHit()||hits.mHit.mPenetrationDepth<.2f;
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
namespace {
JPH::RefConst<JPH::Shape> animalShape(const wildlife::Animal& animal){
    const auto& s=wildlife::species()[animal.species];
    float h=animal.health>0?s.height:s.width,w=animal.health>0?s.width:s.height;
    JPH::RefConst<JPH::Shape> body=new JPH::BoxShape(JPH::Vec3(w*.5f,h*.5f,s.length*.45f),.2f);
    if(wildlife::riding()&& &animal==&wildlife::animals[wildlife::mountedIndex()]){
        JPH::StaticCompoundShapeSettings mounted;
        mounted.AddShape(JPH::Vec3::sZero(),JPH::Quat::sIdentity(),body);
        float riderY=s.height*(animal.species==1?1.0f:.94f)+1-18.5f;
        mounted.AddShape(JPH::Vec3(0,riderY+18-h*.5f-.3f,0),JPH::Quat::sIdentity(),standingShape);
        auto result=mounted.Create();if(result.HasError())throw std::runtime_error(result.GetError().c_str());return result.Get();
    }
    return body;
}
JPH::Quat animalRotation(float angle){return JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-angle);}
}
bool animalClear(const wildlife::Animal& animal,game::Vec3 feet,float angle){
    if(!world)return false;
    const auto& s=wildlife::species()[animal.species];
    if(terrain::contains(feet+game::Vec3{0,.4f,0}))return false;
    float h=animal.health>0?s.height:s.width;
    auto center=JPH::RVec3(feet.x,feet.y+h*.5f+.3f,feet.z);
    JPH::CollideShapeSettings settings;settings.mBackFaceMode=JPH::EBackFaceMode::CollideWithBackFaces;
    JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> hits;
    auto shape=animalShape(animal);
    center+=animalRotation(angle)*shape->GetCenterOfMass();
    world->GetNarrowPhaseQuery().CollideShape(shape,JPH::Vec3::sOne(),
        JPH::RMat44::sRotationTranslation(animalRotation(angle),center),settings,center,hits,
        JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0)),JPH::SpecifiedObjectLayerFilter(Layer::staticBody));
    return !hits.HadHit()||hits.mHit.mPenetrationDepth<.2f;
}
std::vector<float> animalFloors(const wildlife::Animal& animal,game::Vec2 point,float angle){
    std::vector<float> floors;if(!world)return floors;
    JPH::RRayCast ray(JPH::RVec3(point.x,3500,point.z),JPH::Vec3(0,-3950,0));
    JPH::AllHitCollisionCollector<JPH::CastRayCollector> hits;JPH::RayCastSettings settings;
    world->GetNarrowPhaseQuery().CastRay(ray,settings,hits,
        JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0)),JPH::SpecifiedObjectLayerFilter(Layer::staticBody));
    for(const auto& hit:hits.mHits){auto p=ray.GetPointOnRay(hit.mFraction);JPH::Vec3 normal;
        {JPH::BodyLockRead lock(world->GetBodyLockInterface(),hit.mBodyID);
            if(!lock.Succeeded())continue;normal=lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,p);}
        if(normal.GetY()<.7f)continue;
        float feet=float(p.GetY())+.05f;
        // A box on a slope needs its downhill corners above the surface.
        const auto& s=wildlife::species()[animal.species];auto f=game::forward(angle);game::Vec2 side{-f.z,f.x};
        feet+=(std::abs(normal.GetX()*f.x+normal.GetZ()*f.z)*s.length*.45f+
            std::abs(normal.GetX()*side.x+normal.GetZ()*side.z)*(animal.health>0?s.width:s.height)*.5f)/normal.GetY();
        if(animalClear(animal,{point.x,feet,point.z},angle))floors.push_back(feet);
    }
    std::sort(floors.begin(),floors.end());return floors;
}
void moveAnimal(wildlife::Animal& animal,game::Vec2 velocity,float dt){
    if(!world||!builder::active()||dt<=0)return;
    const auto& s=wildlife::species()[animal.species];
    float y=wildlife::originHeight(animal);
    JPH::CharacterVirtualSettings shape;shape.mShape=animalShape(animal);
    shape.mShapeOffset=JPH::Vec3(0,(animal.health>0?s.height:s.width)*.5f+.3f,0);shape.mMaxSlopeAngle=game::PI*.25f;
    JPH::CharacterVirtual character(&shape,JPH::RVec3(animal.p.x,y,animal.p.z),animalRotation(animal.angle),Layer::moving,world.get());
    float vertical=animal.supported&&animal.verticalVelocity<0?0:animal.verticalVelocity;
    vertical+=world->GetGravity().GetY()*dt;
    character.SetLinearVelocity(JPH::Vec3(velocity.x,vertical,velocity.z));
    JPH::CharacterVirtual::ExtendedUpdateSettings settings;settings.mWalkStairsStepUp=JPH::Vec3(0,5,0);
    settings.mStickToFloorStepDown=JPH::Vec3(0,-5,0);
    // Animal bodies continue receiving vehicle contacts. Their own movement
    // sweeps static geometry; wildlife handles vertical-aware actor avoidance.
    character.ExtendedUpdate(dt,world->GetGravity(),settings,
        JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0)),JPH::SpecifiedObjectLayerFilter(Layer::staticBody),{}, {},*allocator);
    auto p=character.GetPosition();animal.p={float(p.GetX()),float(p.GetZ())};
    animal.elevation=float(p.GetY());animal.verticalVelocity=character.GetLinearVelocity().GetY();
    animal.elevationAt=animal.p;animal.elevationKnown=animal.elevationMode=true;animal.supported=character.IsSupported();
}
const SceneryRecovery& lastSceneryRecovery(){return recoveryStats;}
SceneryRecovery reconcileLooseActors(){
    recoveryStats={};if(!world)return recoveryStats;auto& bodies=world->GetBodyInterface();
    // The player may have moved during capsule recovery. Match that new focus
    // before testing nearby loose actors against streamed scenery.
    syncTerrainColliders();syncBuildingColliders(game::player);syncSceneryColliders(game::player);syncBuilderColliders(game::player);
    auto nearby=[](JPH::Vec3Arg p){return game::len(game::Vec2{p.GetX(),p.GetZ()}-game::player)<=1400;};
    auto translate=[&](const std::vector<JPH::BodyID>& ids,game::Vec3 delta){for(auto id:ids){auto p=bodies.GetPosition(id);
        bodies.SetPosition(id,p+JPH::RVec3(delta.x,delta.y,delta.z),JPH::EActivation::Activate);
        bodies.SetLinearAndAngularVelocity(id,JPH::Vec3::sZero(),JPH::Vec3::sZero());}};
    for(std::size_t n=0;n<propBodies.size()&&n<game::props.size();++n){auto id=propBodies[n];if(id.IsInvalid()||!game::props[n].alive)continue;
        auto probes=bodyProbes({id});if(probes.empty()||!nearby(recoveryBounds(probes).GetCenter()))continue;game::Vec3 delta;
        if(!recoveryOffset(probes,delta)){++recoveryStats.unresolved;continue;}
        if(game::len(delta)>.001f){translate({id},delta);++recoveryStats.props;
            auto p=bodies.GetCenterOfMassPosition(id);auto& prop=game::props[n];prop.p={p.GetX(),p.GetZ()};prop.y=p.GetY()-(prop.barrel?11.5f:11);prop.v={};prop.vy=prop.spin=0;}
        // A formerly supported sleeping prop must fall when builder support is removed.
        bodies.ActivateBody(id);
    }
    std::set<std::string> represented;
    for(auto& ragdoll:ragdolls){represented.insert(ragdoll.pedId);auto probes=bodyProbes(ragdoll.bodies);
        if(probes.empty()||!nearby(recoveryBounds(probes).GetCenter()))continue;game::Vec3 delta;
        if(!recoveryOffset(probes,delta)){++recoveryStats.unresolved;continue;}bool moved=game::len(delta)>.001f;
        if(ragdoll.pin){JPH::SphereShape support(4);JPH::ClosestHitCollisionCollector<JPH::CollideShapeCollector> hits;
            auto point=ragdoll.pinPoint;auto p=JPH::RVec3(point.x,point.y,point.z);JPH::CollideShapeSettings settings;
            world->GetNarrowPhaseQuery().CollideShape(&support,JPH::Vec3::sOne(),JPH::RMat44::sTranslation(p),settings,p,hits,
                JPH::SpecifiedBroadPhaseLayerFilter(JPH::BroadPhaseLayer(0)),JPH::SpecifiedObjectLayerFilter(Layer::staticBody));
            if(moved||!hits.HadHit()){world->RemoveConstraint(ragdoll.pin.GetPtr());
                ragdoll.joints.erase(std::remove_if(ragdoll.joints.begin(),ragdoll.joints.end(),[&](const auto& joint){return joint.GetPtr()==ragdoll.pin.GetPtr();}),ragdoll.joints.end());
                ragdoll.pin=nullptr;++recoveryStats.releasedPins;for(auto& ped:game::peds)if(ped.id==ragdoll.pedId)ped.pinned=false;}}
        if(moved){translate(ragdoll.bodies,delta);ragdoll.origin=ragdoll.origin+delta;for(auto& rest:ragdoll.rest)rest=rest+delta;++recoveryStats.ragdolls;}
        for(auto id:ragdoll.bodies)bodies.ActivateBody(id);
    }
    for(auto& snapshot:game::corpseSnapshots){represented.insert(snapshot.pedId);std::vector<RecoveryProbe> probes;
        auto ped=std::find_if(game::peds.begin(),game::peds.end(),[&](const game::Ped& p){return p.id==snapshot.pedId;});
        if(ped!=game::peds.end()&&ped->carried)continue;
        for(const auto& part:snapshot.parts){const auto& geometry=ragdollGeometry[part.part];
            probes.push_back({new JPH::BoxShape(JPH::Vec3(geometry.w/2,geometry.h/2,geometry.d/2)),
                JPH::RMat44::sRotationTranslation(JPH::Quat(part.qx,part.qy,part.qz,part.qw),JPH::RVec3(part.p.x,part.p.y,part.p.z))});}
        if(!nearby(recoveryBounds(probes).GetCenter()))continue;game::Vec3 delta;
        if(!recoveryOffset(probes,delta)){++recoveryStats.unresolved;continue;}
        delta.y-=corpseDrop(probes,delta);
        if(game::len(delta)>.001f){for(auto& part:snapshot.parts){part.p=part.p+delta;part.rest=part.rest+delta;part.origin=part.origin+delta;}++recoveryStats.corpses;
            if(ped!=game::peds.end()){ped->p={snapshot.parts[0].p.x,snapshot.parts[0].p.z};ped->pinned=false;}}
    }
    // Corpses without an active or captured pose still use the existing ground
    // animation. Move their horizontal origin out of restored structures.
    for(auto& ped:game::peds)if(!ped.alive&&!ped.carried&&!represented.count(ped.id)&&game::len(ped.p-game::player)<=1400){
        std::vector<RecoveryProbe> probes{{new JPH::BoxShape(JPH::Vec3(12,5,18)),
            JPH::RMat44::sRotationTranslation(JPH::Quat::sRotation(JPH::Vec3::sAxisY(),game::PI/2-ped.angle),JPH::RVec3(ped.p.x,terrain::height(ped.p)+5,ped.p.z))}};
        game::Vec3 delta;if(!recoveryOffset(probes,delta,true)){++recoveryStats.unresolved;continue;}
        if(game::len(delta)>.001f){ped.p=ped.p+game::Vec2{delta.x,delta.z};ped.pinned=false;++recoveryStats.corpses;}
    }
    publishRagdollPoses();return recoveryStats;
}
void refreshScenery(){
    if(!world)return;
    syncTerrainColliders();
    for(auto& id:buildingBodies)destroyBody(id);buildingBodies.clear();streamedCellX=streamedCellZ=-1;
    for(auto& id:treeBodies)destroyBody(id);treeBodies.clear();treeRevisions.clear();
    for(auto& rock:rockBodies)destroyBody(rock.second.id);rockBodies.clear();
    for(auto& f:fragmentBodies)destroyBody(f.id);fragmentBodies.clear();fragmentVisuals.clear();
    builderRevision=~std::uint64_t(0);
    syncBuildingColliders(game::player);syncSceneryColliders(game::player);syncBuilderColliders(game::player);
    restorePedCharacters(game::player);
}
std::size_t activeBuilderColliderCount(){return builderBodies.size();}
void step(float dt){
    if(!world||dt<=0)return;
    syncTerrainColliders();
    syncBuilderColliders(game::player);
    syncBuildingColliders(game::occupied>=0&&
        std::size_t(game::occupied)<game::vehicles.size()?
        game::vehicles[game::occupied].p:game::player);
    syncSceneryColliders(game::player,dt);
    restorePedCharacters(game::player);
    for(std::size_t index=0;index<pedCharacters.size()&&index<game::peds.size();++index)
        if(!game::peds[index].alive||game::peds[index].drivingVehicle>=0){rememberPed(index);pedCharacters[index]=nullptr;}
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
        vehicle.rideHeight=float(position.GetY())-restHeight;
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
    for(auto it=ragdolls.begin();it!=ragdolls.end();){
        it->life-=dt;
        if(it->life<=0){
            captureCorpsePose(*it);
            for(auto& joint:it->joints)world->RemoveConstraint(joint.GetPtr());
            for(auto id:it->bodies){bodies.RemoveBody(id);bodies.DestroyBody(id);}
            it=ragdolls.erase(it);continue;
        }
        ++it;
    }
    publishRagdollPoses();
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
