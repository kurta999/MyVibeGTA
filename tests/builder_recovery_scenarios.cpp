#include "../src/builder.h"
#include "builder_test_support.h"
#include "../src/excavation.h"
#include "../src/destruction.h"
#include "../src/scenery_edits.h"
#include "../src/jolt_world.h"
#include "../src/game_internal.h"
#include "../src/terrain.h"
#include "../src/regions.h"
#include "../src/savegame.h"
#include "../src/wildlife.h"
#include "../src/birds.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
using namespace game;
void toggle(){bool next=!builder::active();assert(builder::requestToggle());assert(finishBuilderTransition(true)&&builder::active()==next);assert(jolt_world::lastSceneryRecovery().unresolved==0);}
void ticks(int count){for(int n=0;n<count;++n)jolt_world::step(1.0f/60);}
void clean(){builder::reset();buildings.clear();trees.clear();peds.clear();vehicles.clear();props.clear();wildlife::animals.clear();birds::flock.clear();
    player=previousPlayer={140,60};playerY=0;occupied=enteringVehicle=-1;leftMouse=rightMouse=false;jolt_world::reset();}
void pit(){for(int x=2;x<=4;++x)for(int z=2;z<=4;++z)for(int y=-2;y<0;++y)assert(mineTerrainVolume({x,y,z}));}
void corpse(bool pinned=false){Ped ped{};ped.id="builder-recovery-corpse";ped.p=ped.target={140,140};ped.style=1;ped.cash=73;ped.looted=true;ped.respawn=45;peds.push_back(ped);
    jolt_world::addPed();jolt_world::teleportPed(0,ped.p,-80);peds[0].alive=false;peds[0].health=0;peds[0].corpseVisualDelay=6;peds[0].pinned=pinned;
    peds[0].pinAnchor=ped.p;jolt_world::spawnRagdoll(peds[0],{},pinned?&peds[0].pinAnchor:nullptr);ticks(1);}
void rigid(const std::vector<RagdollPart>& before,const std::vector<RagdollPart>& after){assert(before.size()==6&&after.size()==6);auto delta=after[0].p-before[0].p;
    for(std::size_t n=0;n<before.size();++n){assert(len(after[n].p-before[n].p-delta)<.01f);assert(len(after[n].rest-before[n].rest-delta)<.01f);
        assert(len(after[n].origin-before[n].origin-delta)<.01f);assert(after[n].qx==before[n].qx&&after[n].qy==before[n].qy&&after[n].qz==before[n].qz&&after[n].qw==before[n].qw);
        assert(after[n].style==before[n].style&&after[n].part==before[n].part);}}
}
void builderRecoveryScenarios(){
    using namespace game;reset();assert(builder::loadCatalog());clean();
    // Props genuinely fall through excavated terrain, then recover without a
    // simulation tick or a velocity reset on unrelated props.
    props.push_back({{140,140},{},0});props.push_back({{260,80},{},0});props.push_back({{4000,80},{},0});jolt_world::reset();toggle();pit();ticks(120);
    assert(std::abs(props[0].y+80)<1);jolt_world::impulse(1,{7,0,0});ticks(1);auto untouched=props[1],distant=props[2];int healthBefore=props[0].health;
    toggle();assert(jolt_world::lastSceneryRecovery().props==1&&props[0].y>=0&&props[0].health==healthBefore);
    assert(len(props[1].p-untouched.p)<.001f&&len(props[1].v-untouched.v)<.001f&&props[1].vy==untouched.vy);
    assert(len(props[2].p-distant.p)<.001f&&props[2].y==distant.y);ticks(120);assert(std::abs(props[0].y)<1);
    toggle();ticks(120);assert(std::abs(props[0].y+80)<1);assert(builder::active());
    std::puts("recovery: prop pit fall, immediate F5 resurface, preserved unrelated momentum and re-entry fall passed");
    // Water has a lower Jolt bed than the nominal terrain height. F5 must not
    // mistake an unchanged submerged prop for an actor buried by a world edit.
    clean();Vec2 sea{850,SHORE+200};assert(regions::waterAt(sea));player=previousPlayer={850,SHORE-100};
    props.push_back({sea,{},-15});jolt_world::reset();ticks(120);assert(std::abs(props[0].y+15)<1);auto submerged=props[0];
    toggle();assert(jolt_world::lastSceneryRecovery().props==0&&len(props[0].p-submerged.p)<.001f&&props[0].y==submerged.y);
    toggle();assert(jolt_world::lastSceneryRecovery().props==0&&props[0].y==submerged.y);
    std::puts("recovery: unchanged submerged prop retains the existing water-bed physics passed");
    // A live, pinned ragdoll is translated as a whole, preserving its pose and
    // internal joints. Its obsolete world pin must not pull it back underground.
    clean();toggle();pit();corpse(true);ticks(30);auto before=ragdollParts;float respawn=peds[0].respawn,delay=peds[0].corpseVisualDelay;
    toggle();assert(jolt_world::lastSceneryRecovery().ragdolls==1&&jolt_world::lastSceneryRecovery().releasedPins==1);rigid(before,ragdollParts);
    assert(!peds[0].pinned&&!peds[0].alive&&peds[0].looted&&peds[0].cash==73&&peds[0].respawn==respawn&&peds[0].corpseVisualDelay==delay);
    for(auto part:ragdollParts)assert(part.p.y>0&&part.p.y<100);ticks(120);assert(ragdollParts.size()==6);
    for(auto part:ragdollParts)assert(part.p.y>0&&part.p.y<60&&len(part.p-ragdollParts[0].p)<60);
    std::puts("recovery: live ragdoll rigid pose/joint recovery and obsolete pin release passed");
    // A settled snapshot has no active rigid bodies. It still needs to be
    // lifted out of restored ground and lowered onto re-entered pit support.
    clean();toggle();pit();corpse();ticks(420);assert(corpseSnapshots.size()==1&&ragdollParts.size()==6);before=ragdollParts;
    for(auto part:before)assert(part.p.y<-50);respawn=peds[0].respawn;delay=peds[0].corpseVisualDelay;
    toggle();assert(jolt_world::lastSceneryRecovery().corpses==1);rigid(before,ragdollParts);
    for(auto part:ragdollParts)assert(part.p.y>0&&part.p.y<40);assert(peds[0].respawn==respawn&&peds[0].corpseVisualDelay==delay);
    before=ragdollParts;toggle();assert(jolt_world::lastSceneryRecovery().corpses==1);rigid(before,ragdollParts);
    for(auto part:ragdollParts)assert(part.p.y<-50&&part.p.y>-80);assert(corpseSnapshots.size()==1);
    for(int n=0;n<4;++n){toggle();for(auto part:ragdollParts)assert(builder::active()?part.p.y<-50:part.p.y>0);assert(corpseSnapshots.size()==1);}
    assert(excavation::cells().size()==144&&peds[0].cash==73&&peds[0].looted);
    std::puts("recovery: settled corpse snapshot support casts, rigid poses and repeated F5 isolation passed");
    // Preserve support from shared dynamic scenery too. A captured pose placed
    // on a normal crate must not be lowered through that unchanged crate.
    clean();Ped supported{};supported.id="builder-recovery-supported-corpse";supported.p=supported.target={140,140};supported.respawn=45;peds.push_back(supported);jolt_world::reset();
    peds[0].alive=false;peds[0].health=0;peds[0].corpseVisualDelay=6;jolt_world::spawnRagdoll(peds[0],{});ticks(420);assert(corpseSnapshots.size()==1);
    auto pose=corpseSnapshots[0];for(auto& part:pose.parts){part.p.y+=22.2f;part.rest.y+=22.2f;part.origin.y+=22.2f;}
    props.push_back({{140,140},{},0});jolt_world::reset();corpseSnapshots.push_back(pose);before.assign(pose.parts.begin(),pose.parts.end());
    toggle();rigid(before,ragdollParts);assert(ragdollParts[0].p.y>20&&before[0].p.y-ragdollParts[0].p.y<5&&props[0].y==0);
    before=ragdollParts;toggle();rigid(before,ragdollParts);assert(ragdollParts[0].p.y>20);
    std::puts("recovery: settled corpse retains support from an unchanged dynamic crate passed");
    // Saved placed collision can appear around a normal-mode prop or an old
    // corpse animation; both must be outside it before input resumes.
    clean();toggle();assert(placeEditVolume({3,0,3},builder::itemIndex("granite"),false));toggle();
    props.push_back({{140,140},{},0});Ped old{};old.id="builder-recovery-old-corpse";old.p=old.target={140,140};old.alive=false;old.health=0;old.respawn=45;peds.push_back(old);jolt_world::reset();
    toggle();assert(jolt_world::lastSceneryRecovery().props==1&&jolt_world::lastSceneryRecovery().corpses==1);
    assert(props[0].p.x<108||props[0].p.x>172||props[0].p.z<108||props[0].p.z>172||props[0].y>=40);
    assert(peds[0].p.x<108||peds[0].p.x>172||peds[0].p.z<108||peds[0].p.z>172);ticks(120);assert(props[0].alive&&props[0].health==80);
    std::puts("recovery: reappearing saved block clears loose props and legacy corpse animation origins passed");
    // Restored tree collision is not an axis-aligned building or placed cube.
    // The actual incoming Jolt trunk shape must also clear a loose prop.
    clean();dx11::loadMeshes(L"assets/models/baked");Tree tree{};tree.id="builder-recovery-tree";tree.p={4500,4500};trees.push_back(tree);
    player=previousPlayer={4420,4500};playerY=terrain::baseHeight(player);jolt_world::reset();toggle();
    auto root=builder::cellAt({4500,1,4500});for(int x=-1;x<=0;++x)for(int z=-1;z<=0;++z)for(int y=0;y<2;++y){builder::Cell c{root.x+x,y,root.z+z};if(scenery_edits::validCut(scenery_edits::treeId(0),c))assert(scenery_edits::cut(scenery_edits::treeId(0),c));}
    props.push_back({{4500,4500},{},0});jolt_world::reset();ticks(60);assert(len(props[0].p-tree.p)<1);
    toggle();assert(jolt_world::lastSceneryRecovery().props==1&&len(props[0].p-tree.p)>jolt_world::treeRadius(tree)+11);
    assert(scenery_edits::records().size()==1);ticks(60);assert(props[0].alive&&props[0].health==80);
    std::puts("recovery: restored Jolt tree trunk collision clears a prop from its mined root passed");
    // A wide restored building exhausts the short local escape search. A prop
    // can be lifted onto its roof; a legacy corpse must find a ground origin,
    // because its old animation cannot store a roof altitude.
    clean();buildings.push_back({80,80,640,640,80,rgb(90,90,90),"builder-recovery-wide"});jolt_world::reset();toggle();
    assert(destruction::cut(0,{{80,0,80},{720,80,720}}));props.push_back({{400,400},{},0});
    old.id="builder-recovery-wide-corpse";old.p=old.target={400,400};peds.push_back(old);jolt_world::reset();toggle();
    assert(jolt_world::lastSceneryRecovery().props==1&&jolt_world::lastSceneryRecovery().corpses==1&&props[0].y>=79.9f);
    assert(peds[0].p.x<62||peds[0].p.x>738||peds[0].p.z<62||peds[0].p.z>738);ticks(120);assert(std::abs(props[0].y-80)<1);
    toggle();ticks(120);assert(std::abs(props[0].y)<1&&props[0].health==80);
    std::puts("recovery: wide building roof fallback, ground-only legacy corpse escape and removed-roof fall passed");
    // Keep a valid wall pin in an unchanged layer, then remove only its support
    // in the saved builder layer. Re-entry releases it without changing the pose.
    clean();buildings.push_back({240,120,40,40,80,rgb(90,90,90),"builder-recovery-wall"});
    Ped wall{};wall.id="builder-recovery-wall-corpse";wall.p=wall.target={230,140};wall.respawn=45;peds.push_back(wall);jolt_world::reset();
    peds[0].alive=false;peds[0].health=0;peds[0].pinned=true;peds[0].pinAnchor={241,140};peds[0].corpseVisualDelay=15;
    jolt_world::spawnRagdoll(peds[0],{},&peds[0].pinAnchor);ticks(1);before=ragdollParts;toggle();
    assert(peds[0].pinned&&jolt_world::lastSceneryRecovery().releasedPins==0&&jolt_world::lastSceneryRecovery().ragdolls==0);rigid(before,ragdollParts);
    assert(destruction::cut(0,{{240,0,120},{280,80,160}}));toggle();before=ragdollParts;toggle();
    assert(!peds[0].pinned&&jolt_world::lastSceneryRecovery().releasedPins==1);rigid(before,ragdollParts);ticks(120);
    assert(ragdollParts.size()==6);for(auto part:ragdollParts)assert(part.p.y>0&&part.p.y<50&&len(part.p-ragdollParts[0].p)<60);
    std::puts("recovery: valid wall pin retained, removed saved wall releases only its external constraint passed");
    reset();assert(savegame::save());jolt_world::shutdown();
}
