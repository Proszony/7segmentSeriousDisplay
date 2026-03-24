#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>

#define FRAME_START 0x2137
#define FRAME_END   0x69

struct SegState {
    bool on[7];
};

struct Frame {
    uint16_t start;
    uint8_t max_X;
    uint8_t max_Y;
    std::vector<uint8_t> cells;
    uint8_t end;
};

Frame create_frame(const std::vector<SegState>& segStates, uint8_t max_X, uint8_t max_Y);

void writeUint16LE(std::vector<uint8_t>& out, uint16_t v);

std::vector<uint8_t> serialize(const Frame& f);

Frame deserialize(const std::vector<uint8_t>& data);