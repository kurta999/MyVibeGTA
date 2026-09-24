#pragma once
#include "game.h"

namespace physics {
struct VehicleTuning {
    float acceleration,maxSpeed,reverseSpeed,grip,drag,turnRate,turnResponse,brake;
};
VehicleTuning tuning(game::Kind kind);
game::Vec2 stepVehicle(game::Vehicle& vehicle,float throttle,float steering,float dt);
void stepCharacterVertical(float dt,bool jumpPressed);
}
