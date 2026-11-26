#include "framing.h"
#include "test_util.h"

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <vector>

namespace {

SegState make_seg(std::initializer_list<int> bits)
{
    SegState s{};
    for (int bit : bits) {
        if (bit >= 0 && bit < 7)
            s.on[bit] = true;
    }
    return s;
}

void test_uint16_le()
{
    TEST_SECTION("writeUint16LE writes little-endian bytes");

    std::vector<uint8_t> out;
    writeUint16LE(out, 0x2137);
    CHECK_EQ(out.size(), size_t(2));
    CHECK_EQ(int(out[0]), 0x37);
    CHECK_EQ(int(out[1]), 0x21);
}

void test_serialize_layout()
{
    TEST_SECTION("serialize produces the documented layout");

    // Two segments on: bit 0 and bit 6 -> 0b0100_0001 = 0x41.
    std::vector<SegState> states{make_seg({0, 6})};
    Frame f = create_frame(states, 1, 1);

    CHECK_EQ(int(f.start), 0x2137);
    CHECK_EQ(int(f.max_X), 1);
    CHECK_EQ(int(f.max_Y), 1);
    CHECK_EQ(f.cells.size(), size_t(1));
    CHECK_EQ(int(f.cells[0]), 0x41);
    CHECK_EQ(int(f.end), 0x69);

    const auto data = serialize(f);
    CHECK_EQ(data.size(), size_t(6));
    CHECK_EQ(int(data[0]), 0x37);
    CHECK_EQ(int(data[1]), 0x21);
    CHECK_EQ(int(data[2]), 0x01);
    CHECK_EQ(int(data[3]), 0x01);
    CHECK_EQ(int(data[4]), 0x41);
    CHECK_EQ(int(data[5]), 0x69);
}

void test_roundtrip()
{
    TEST_SECTION("serialize/deserialize roundtrip");

    std::vector<SegState> states;
    for (int i = 0; i < 64; ++i)
        states.push_back(make_seg({i % 7}));

    Frame f = create_frame(states, 8, 8);
    const auto data = serialize(f);
    Frame g = deserialize(data);

    CHECK_EQ(int(g.start), int(f.start));
    CHECK_EQ(int(g.max_X), 8);
    CHECK_EQ(int(g.max_Y), 8);
    CHECK_EQ(int(g.end), 0x69);
    CHECK(g.cells == f.cells);
}

void test_serialize_rejects_bad_cell_count()
{
    TEST_SECTION("serialize rejects a mismatched cell count");

    Frame f;
    f.start = FRAME_START;
    f.max_X = 2;
    f.max_Y = 2;
    f.cells = {0x01}; // expects 4 cells
    f.end = FRAME_END;

    CHECK_THROWS(serialize(f), std::runtime_error);

    // create_frame does not trim extra cells either.
    std::vector<SegState> states{make_seg({0}), make_seg({1}), make_seg({2})};
    Frame extra = create_frame(states, 1, 1);
    CHECK_THROWS(serialize(extra), std::runtime_error);
}

void test_deserialize_rejects_malformed()
{
    TEST_SECTION("deserialize rejects malformed frames");

    // Too short to even contain the header.
    CHECK_THROWS(deserialize(std::vector<uint8_t>{0x37, 0x21, 0x01}),
                 std::runtime_error);

    // Wrong start marker.
    CHECK_THROWS(deserialize(std::vector<uint8_t>{0x00, 0x21, 0x01, 0x01, 0x00, 0x69}),
                 std::runtime_error);

    // Declared 1x1 cell but the payload length does not match.
    CHECK_THROWS(deserialize(std::vector<uint8_t>{0x37, 0x21, 0x01, 0x01, 0x00, 0x00, 0x69}),
                 std::runtime_error);

    // Correct length but wrong end marker.
    CHECK_THROWS(deserialize(std::vector<uint8_t>{0x37, 0x21, 0x01, 0x01, 0x00, 0x00}),
                 std::runtime_error);
}

void test_empty_frame()
{
    TEST_SECTION("empty frame is valid");

    std::vector<SegState> states;
    Frame f = create_frame(states, 0, 0);
    const auto data = serialize(f);

    CHECK_EQ(data.size(), size_t(5));
    CHECK_EQ(int(data[0]), 0x37);
    CHECK_EQ(int(data[1]), 0x21);
    CHECK_EQ(int(data[2]), 0x00);
    CHECK_EQ(int(data[3]), 0x00);
    CHECK_EQ(int(data[4]), 0x69);

    Frame g = deserialize(data);
    CHECK(g.cells.empty());
}

}

int main()
{
    test_uint16_le();
    test_serialize_layout();
    test_roundtrip();
    test_serialize_rejects_bad_cell_count();
    test_deserialize_rejects_malformed();
    test_empty_frame();
    return test::summary();
}
