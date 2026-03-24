#include "framing.h"

Frame create_frame(const std::vector<SegState>& segStates, uint8_t max_X, uint8_t max_Y) {
    Frame f;
    f.start = FRAME_START;
    f.max_X = max_X;
    f.max_Y = max_Y;
    f.cells.reserve(static_cast<size_t>(max_X) * max_Y);
    f.end = FRAME_END;

    for (const auto& seg : segStates) {
        uint8_t b = 0;
        for (int i = 0; i < 7; i++) {
            if (seg.on[i]) b |= (1u << i);
        }
        f.cells.push_back(b);
    }

    return f;
}

void writeUint16LE(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}

std::vector<uint8_t> serialize(const Frame& f) {
    const size_t expected_cells = static_cast<size_t>(f.max_X) * f.max_Y;
    if (f.cells.size() != expected_cells) {
        throw std::runtime_error("Invalid frame cell size");
    }

    std::vector<uint8_t> data;
    data.reserve(2 + 1 + 1 + f.cells.size() + 1);

    writeUint16LE(data, f.start);
    data.push_back(f.max_X);
    data.push_back(f.max_Y);
    data.insert(data.end(), f.cells.begin(), f.cells.end());
    data.push_back(f.end);

    return data;
}

Frame deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < 5) {
        throw std::runtime_error("Frame too short");
    }

    uint16_t start = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
    if (start != FRAME_START) {
        throw std::runtime_error("Invalid frame start marker");
    }

    uint8_t max_X = data[2];
    uint8_t max_Y = data[3];
    size_t expected = static_cast<size_t>(max_X) * max_Y;

    if (data.size() != 5 + expected) {
        throw std::runtime_error("Invalid frame size");
    }

    if (data[data.size() - 1] != FRAME_END) {
        throw std::runtime_error("Invalid frame end marker");
    }

    Frame f;
    f.start = start;
    f.max_X = max_X;
    f.max_Y = max_Y;
    f.cells.assign(data.begin() + 4, data.end() - 1);
    f.end = FRAME_END;

    return f;
}