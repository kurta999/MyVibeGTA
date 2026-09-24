#pragma once

namespace weapons {
struct Stats {
    const char* name;
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
};
bool load(const char* path=nullptr);
const Stats& stats(int index);
}
