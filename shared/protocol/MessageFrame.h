#pragma once

#include <cstdint>

// Updated for Cyberpunk 2.3 compatibility
constexpr uint32_t PROTOCOL_VERSION_CURRENT = 0x3;

struct MessageFrame {
    uint8_t channel_id;
    uint16_t message_type;
    uint8_t reserved;
};

// TODO: refactor into a different file
struct Vector3
{
    float x;
    float y;
    float z;
};