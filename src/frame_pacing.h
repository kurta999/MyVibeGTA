#pragma once
#include <algorithm>
#include <cmath>

// Fixed-step simulation pacing shared by the game loop and its tests.
namespace frame_pacing {
struct StepPlan {
    int steps = 0;        // fixed updates to run this frame
    bool dropped = false; // true when the simulation could not keep up
};

// Consumes whole fixed steps from the accumulator, up to maxSteps. When the
// cap is hit with more than one step still owed, the backlog is discarded but
// the fractional remainder is kept so render interpolation stays continuous.
inline StepPlan consume(double& accumulator, double fixedStep, int maxSteps) {
    StepPlan plan;
    if (fixedStep <= 0 || maxSteps <= 0) {
        accumulator = 0;
        return plan;
    }
    accumulator = std::max(0.0, accumulator);
    while (accumulator >= fixedStep && plan.steps < maxSteps) {
        accumulator -= fixedStep;
        ++plan.steps;
    }
    if (accumulator >= fixedStep) {
        accumulator = std::fmod(accumulator, fixedStep);
        plan.dropped = true;
    }
    return plan;
}

// Render interpolation factor in [0, 1].
inline float alpha(double accumulator, double fixedStep) {
    if (fixedStep <= 0)
        return 1.0f;
    return float(std::clamp(accumulator / fixedStep, 0.0, 1.0));
}
} // namespace frame_pacing
