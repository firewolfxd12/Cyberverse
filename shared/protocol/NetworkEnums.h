#pragma once
#include <cstdint>

enum class EStance : uint16_t {
    Stand = 0,
    Crouch = 1,
};

enum class ELocomotion : uint16_t {
    Idle = 0,
    Walk = 1,
    Run = 2,
    Sprint = 3,
    Swim = 4,
};

struct LocomotionState {
    uint16_t stanceId;
    uint16_t locomotionId;
    uint16_t overlayId;
    uint16_t phase;
};
