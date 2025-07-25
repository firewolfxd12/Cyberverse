#pragma once

#include <cstdint>

enum EMessageTypeServerbound: uint16_t {
    EINIT_AUTH = 0,
    EPLAYER_JOIN_WORLD = 1,
    ePlayerActionTracked = 2,
    ePlayerPositionUpdate = 3,
    ePlayerSpawnCar = 4,
    ePlayerMountCar = 5, // TODO: Implement
    ePlayerUnmountCar = 6,
    ePlayerEquipItem = 7,
    ePlayerShoot = 8,
    ePlayerAppearance = 9,
    ePlayerLocomotion = 10
};