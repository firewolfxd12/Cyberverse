#pragma once
#include "../MessageFrame.h"
#include "EMessageTypeServerbound.h"
#include "../NetworkEnums.h"
#include <array>

struct PlayerAppearance {
    uint32_t dataLength;
    std::array<uint8_t, 512> data;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = ePlayerAppearance;
        frame.channel_id = 0;
    }
};

struct PlayerLocomotion {
    LocomotionState state;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = ePlayerLocomotion;
        frame.channel_id = 1;
    }
};
