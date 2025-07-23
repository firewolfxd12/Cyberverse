#pragma once
#include "../MessageFrame.h"
#include "EMessageTypeClientbound.h"
#include "../NetworkEnums.h"
#include <array>

struct ApplyAppearance {
    uint64_t networkedEntityId;
    uint32_t dataLength;
    std::array<uint8_t, 512> data;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = eApplyAppearance;
        frame.channel_id = 0;
    }
};

struct UpdateLocomotion {
    uint64_t networkedEntityId;
    LocomotionState state;

    inline static void FillMessageFrame(MessageFrame& frame)
    {
        frame.message_type = eUpdateLocomotion;
        frame.channel_id = 1;
    }
};
