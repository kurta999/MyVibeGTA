#pragma once
#include "game.h"
#include <string>

namespace physics {
// Shared by the DX11 scene, Jolt chassis, camera and gameplay hit volumes.
float vehicleScale(game::Kind kind);
float vehicleRestHeight(game::Kind kind);
float vehicleRadius(game::Kind kind);
game::Vec3 chassisHalf(game::Kind kind);
bool vehicleContains(const game::Vehicle& vehicle,game::Vec3 point);
bool vehicleSegmentHit(const game::Vehicle& vehicle,game::Vec3 start,game::Vec3 end,float& entry);
struct VehicleTuning {
    float acceleration,maxSpeed,reverseSpeed,grip,drag,turnRate,turnResponse,brake;
    float mass,engineTorque,suspensionFrequency,suspensionDamping;
    float wheelRadius,steerAngle,brakeTorque,handbrakeTorque;
    float maxHealth,smokeThreshold,collisionDamageScale,bulletDamageScale;
    int repairCost;
    float driftMinSpeed,driftMinSlip,driftMaxSlip,driftMinDuration,driftMinDistance,driftCooldown;
    int driftReward;
};
bool load(const char* path=nullptr);
const std::string& lastError();
VehicleTuning tuning(game::Kind kind);
float tractionAt(game::Vec2 point);
game::Vec2 stepVehicle(game::Vehicle& vehicle,float throttle,float steering,float dt);
int updateDrift(game::Vehicle& vehicle,float steering,bool handbrake,float dt);
void stepCharacterVertical(float dt,bool jumpPressed);
}
