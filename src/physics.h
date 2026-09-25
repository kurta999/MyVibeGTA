#pragma once
#include "game.h"
#include <string>

namespace physics {
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
