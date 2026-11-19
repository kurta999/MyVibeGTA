#pragma once
#include "game.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

// Uniform grid over pedestrian positions for neighbour queries. Avoidance
// used to scan every pedestrian for every candidate velocity of every
// pedestrian (O(17 n^2) per tick). The grid is rebuilt lazily whenever the
// simulation clock or the pedestrian list changes, so callers never see
// positions older than the current tick (plus a small movement margin).
namespace ped_grid {
constexpr float CELL = 64;
constexpr float MARGIN = 12; // peds move a few units within one tick
struct Grid {
    std::vector<std::pair<std::int64_t, int>> entries; // sorted (cell key, ped index)
    float builtTime = -1;
    std::size_t builtCount = 0;
    const game::Ped* builtData = nullptr;
};
inline Grid& grid() {
    static Grid instance;
    return instance;
}
inline std::int64_t key(int x, int z) {
    return (std::int64_t(x) << 32) ^ std::uint32_t(z);
}
inline int cell(float value) {
    return int(std::floor(value / CELL));
}
inline void rebuild() {
    auto& g = grid();
    g.entries.clear();
    g.entries.reserve(game::peds.size());
    for (int i = 0; i < int(game::peds.size()); ++i) {
        const auto& ped = game::peds[i];
        g.entries.push_back({key(cell(ped.p.x), cell(ped.p.z)), i});
    }
    std::sort(g.entries.begin(), g.entries.end());
    g.builtTime = game::worldTime;
    g.builtCount = game::peds.size();
    g.builtData = game::peds.data();
}
inline void ensureCurrent() {
    const auto& g = grid();
    if (g.builtTime != game::worldTime || g.builtCount != game::peds.size() ||
        g.builtData != game::peds.data())
        rebuild();
}
// Calls visit(other) for pedestrians whose cell overlaps the radius around
// p (a superset; callers keep their exact distance tests). Stops early when
// visit returns true.
template <class Visit> void forEachNear(game::Vec2 p, float radius, Visit&& visit) {
    ensureCurrent();
    const auto& g = grid();
    radius += MARGIN;
    const int x0 = cell(p.x - radius), x1 = cell(p.x + radius), z0 = cell(p.z - radius),
              z1 = cell(p.z + radius);
    for (int x = x0; x <= x1; ++x)
        for (int z = z0; z <= z1; ++z) {
            const std::int64_t k = key(x, z);
            auto it = std::lower_bound(g.entries.begin(), g.entries.end(), std::make_pair(k, -1));
            for (; it != g.entries.end() && it->first == k; ++it) {
                if (it->second >= int(game::peds.size()))
                    continue;
                if (visit(game::peds[it->second]))
                    return;
            }
        }
}
} // namespace ped_grid
