#pragma once
#include <string>

namespace weapons {
struct Stats {
    std::string name;
    int magazine;
    float reloadSeconds;
    float secondsBetweenShots;
    int damage;
    float projectileSpeed;
    float spread;
    int pellets;
    float gravity;
    float range;
    float falloff;
    float recoilKick;
    int reservePickup;
    std::string id;
    bool driveByAllowed=false;
    bool rocket=false;
    float explosionRadius=0;
    int explosionDamage=0;
    int streamType=0; // 0 ballistic, 1 flame, 2 extinguisher, 3 water cannon
    bool silenced=false;
    bool melee=false;
    bool arrow=false;
    bool dualWieldAllowed=false;
};
bool load(const char* path=nullptr);
const Stats& stats(int index);
int count();
int indexOf(const std::string& id);
const std::string& lastError();
}
