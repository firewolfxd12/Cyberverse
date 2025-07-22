#pragma once

#include "../MessageFrame.h"
#include "EMessageTypeServerbound.h"

struct PlayerJoinWorld {
    Vector3 position;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = EPLAYER_JOIN_WORLD;
        // Join packets are small and should be reliable
        frame.channel_id = 0;
    }
};

enum PlayerAction: uint8_t
{
    eACTION_JUMP,
    eACTION_RANGED_ATTACK,
};

struct PlayerActionTracked
{
    // True when the button causing the action was pressed, false on release
    bool buttonState;
    // Relative tick count since connection establishment
    uint64_t networkTick;
    PlayerAction action;
    Vector3 worldTransform;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = ePlayerActionTracked;
        // Input actions are sent unreliably
        frame.channel_id = 1;
    }
};

struct PlayerPositionUpdate
{
    // Relative tick count since connection establishment
    uint64_t networkTick;
    Vector3 worldTransform;
    float yaw;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = ePlayerPositionUpdate;
        // Position updates are sent on the unreliable channel
        frame.channel_id = 1;
    }
};

// TODO: Hook "call player car", then it's just a matter of tracking NPC cars.
struct PlayerSpawnCar {
    Vector3 worldTransform;
    float yaw;
    uint64_t recordId;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = ePlayerSpawnCar;
        frame.channel_id = 0;
    }
};

struct PlayerUnmountCar {
    // TODO: NetworkedEntityId

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = ePlayerUnmountCar;
        frame.channel_id = 0;
    }
};

struct PlayerEquipItem {
    uint64_t slot;
    uint64_t itemId;
    bool isWeapon;
    bool isUnequipping;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = ePlayerEquipItem;
        frame.channel_id = 0;
    }
};

struct PlayerShoot {
    // TODO: This should be the weaponId perspectively, not the TweakDBID
    uint64_t itemIdWeapon;
    Vector3 startPoint;
    float charge;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = ePlayerShoot;
        frame.channel_id = 0;
    }
};