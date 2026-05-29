// Wave 2: round-trip every new FieldType through PacketRegistry::encode_packet
// and decode_packet. Each test registers a one-off PacketSchema that exercises
// one field type, builds a Packet whose `fields` map contains the test value,
// encodes -> decodes, and asserts the recovered value is bit-identical.

#include "kprotocol/codec.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/types.hpp"
#include "kprotocol/version.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// We run every assertion in this file via this macro so the tests stay
// effective in Release builds (where assert() is a no-op).
#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            std::abort();                                                      \
        }                                                                      \
    } while (false)

const auto kVersion = kprotocol::ProtocolVersion::v1_20_4;
const auto kState   = kprotocol::PacketState::play;
const auto kDir     = kprotocol::PacketDirection::clientbound;

kprotocol::PacketSchema make_schema(const std::string& key,
                                    std::vector<kprotocol::FieldSpec> fields,
                                    std::int32_t id) {
    kprotocol::PacketSchema schema;
    schema.key = key;
    schema.state = kState;
    schema.direction = kDir;
    schema.ids[kVersion] = id;
    schema.field_sets[kVersion] = std::move(fields);
    return schema;
}

template <typename T>
void assert_roundtrip(const char* tag,
                      kprotocol::FieldType field_type,
                      const T& value,
                      std::int32_t packet_id) {
    std::cout << "  " << tag << "... " << std::flush;
    kprotocol::PacketRegistry registry;
    registry.register_schema(make_schema(std::string("test.") + tag,
                                         {{"v", field_type}},
                                         packet_id));

    kprotocol::Packet original;
    original.key = std::string("test.") + tag;
    original.state = kState;
    original.direction = kDir;
    original.fields["v"] = value;

    const auto encoded = registry.encode_packet(original, kVersion);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
    KPC_CHECK(consumed == encoded.size(), "frame consumed all bytes");

    const auto decoded = registry.decode_packet(frame, kVersion, kState, kDir);
    KPC_CHECK(std::get<T>(decoded.fields.at("v")) == value, "value matches");
    std::cout << "ok\n";
}

void test_legacy_definition_still_works() {
    std::cout << "  PacketDefinition shim still encodes/decodes... " << std::flush;
    kprotocol::PacketRegistry registry;
    kprotocol::PacketDefinition def;
    def.key = "test.shim";
    def.state = kState;
    def.direction = kDir;
    def.fields = {{"x", kprotocol::FieldType::var_int}};
    def.ids[kVersion] = 0x70;
    registry.register_definition(def);

    kprotocol::Packet p;
    p.key = "test.shim";
    p.state = kState;
    p.direction = kDir;
    p.fields["x"] = std::int32_t{12345};

    const auto encoded = registry.encode_packet(p, kVersion);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
    const auto decoded = registry.decode_packet(frame, kVersion, kState, kDir);
    KPC_CHECK(std::get<std::int32_t>(decoded.fields.at("x")) == 12345, "x matches");
    std::cout << "ok\n";
}

void test_position_packing_known_vector() {
    std::cout << "  Position bit packing matches spec for (-5, 64, 33554431)... " << std::flush;
    kprotocol::PacketRegistry registry;
    registry.register_schema(make_schema("test.position_known",
                                         {{"p", kprotocol::FieldType::position}}, 0x71));
    kprotocol::Packet p;
    p.key = "test.position_known";
    p.state = kState;
    p.direction = kDir;
    p.fields["p"] = kprotocol::Position{-5, 64, 33554431};

    const auto encoded = registry.encode_packet(p, kVersion);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
    const auto decoded = registry.decode_packet(frame, kVersion, kState, kDir);
    const auto out = std::get<kprotocol::Position>(decoded.fields.at("p"));
    KPC_CHECK(out.x == -5,        "x matches");
    KPC_CHECK(out.y == 64,        "y matches");
    KPC_CHECK(out.z == 33554431,  "z matches");
    std::cout << "ok\n";
}

void test_rest_buffer_consumes_trailing_bytes() {
    std::cout << "  rest_buffer consumes everything after preceding fields... " << std::flush;
    kprotocol::PacketRegistry registry;
    kprotocol::PacketSchema schema;
    schema.key = "test.rest_buffer_after_prefix";
    schema.state = kState;
    schema.direction = kDir;
    schema.ids[kVersion] = 0x72;
    schema.field_sets[kVersion] = {
        {"version", kprotocol::FieldType::var_int},
        {"payload", kprotocol::FieldType::rest_buffer},
    };
    registry.register_schema(std::move(schema));

    kprotocol::Packet p;
    p.key = "test.rest_buffer_after_prefix";
    p.state = kState;
    p.direction = kDir;
    p.fields["version"] = std::int32_t{42};
    p.fields["payload"] = std::vector<std::uint8_t>{0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

    const auto encoded = registry.encode_packet(p, kVersion);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
    const auto decoded = registry.decode_packet(frame, kVersion, kState, kDir);
    KPC_CHECK(std::get<std::int32_t>(decoded.fields.at("version")) == 42, "version");
    const auto& tail = std::get<std::vector<std::uint8_t>>(decoded.fields.at("payload"));
    KPC_CHECK((tail == std::vector<std::uint8_t>{0xAA, 0xBB, 0xCC, 0xDD, 0xEE}), "tail bytes");
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "field_types_tests:\n";

    assert_roundtrip<std::int8_t>("i8",         kprotocol::FieldType::i8,     std::int8_t{-42},   0x10);
    assert_roundtrip<std::uint8_t>("u8",        kprotocol::FieldType::u8,     std::uint8_t{200},  0x11);
    assert_roundtrip<std::int16_t>("i16_be",    kprotocol::FieldType::i16_be, std::int16_t{-30000}, 0x12);
    assert_roundtrip<std::uint16_t>("u16_be",   kprotocol::FieldType::u16_be, std::uint16_t{60000}, 0x13);
    assert_roundtrip<std::int32_t>("i32_be_min", kprotocol::FieldType::i32_be,
                                   std::numeric_limits<std::int32_t>::min(), 0x14);
    assert_roundtrip<std::uint32_t>("u32_be",   kprotocol::FieldType::u32_be, std::uint32_t{0xCAFEF00DU}, 0x15);
    assert_roundtrip<std::int64_t>("i64_be_max", kprotocol::FieldType::i64_be,
                                   std::numeric_limits<std::int64_t>::max(), 0x16);
    assert_roundtrip<std::uint64_t>("u64_be",   kprotocol::FieldType::u64_be, std::uint64_t{0xDEADBEEFFEEDFACEULL}, 0x17);
    assert_roundtrip<float>("f32_be",           kprotocol::FieldType::f32_be, 3.14159265f, 0x18);
    assert_roundtrip<double>("f64_be",          kprotocol::FieldType::f64_be, -2.718281828459045, 0x19);
    assert_roundtrip<kprotocol::UUID>("uuid", kprotocol::FieldType::uuid,
        kprotocol::UUID{{{0x12,0x34,0x56,0x78,0x9A,0xBC,0xDE,0xF0,
                          0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10}}}, 0x1A);
    assert_roundtrip<kprotocol::Position>("position", kprotocol::FieldType::position,
        kprotocol::Position{12345, -678, -3456789}, 0x1B);

    test_legacy_definition_still_works();
    test_position_packing_known_vector();
    test_rest_buffer_consumes_trailing_bytes();

    std::cout << "  var_int_array round-trip... " << std::flush;
    {
        kprotocol::PacketRegistry registry;
        registry.register_schema(make_schema("test.var_int_array",
            {{"values", kprotocol::FieldType::var_int_array}}, 0x73));
        kprotocol::Packet p;
        p.key = "test.var_int_array";
        p.state = kState;
        p.direction = kDir;
        p.fields["values"] = std::vector<std::int32_t>{1, 2, 300, -4};
        const auto encoded = registry.encode_packet(p, kVersion);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
        const auto decoded = registry.decode_packet(frame, kVersion, kState, kDir);
        KPC_CHECK((std::get<std::vector<std::int32_t>>(decoded.fields.at("values"))
                   == std::vector<std::int32_t>{1, 2, 300, -4}), "var_int_array");
    }
    std::cout << "ok\n";

    assert_roundtrip<std::vector<std::int64_t>>("i64_array", kprotocol::FieldType::i64_array,
        std::vector<std::int64_t>{1, -2, 0x0102030405060708LL}, 0x75);
    assert_roundtrip<std::vector<std::string>>("string_array", kprotocol::FieldType::string_array,
        std::vector<std::string>{"minecraft:overworld", "minecraft:the_nether"}, 0x76);
    assert_roundtrip<std::vector<kprotocol::UUID>>("uuid_array", kprotocol::FieldType::uuid_array,
        std::vector<kprotocol::UUID>{
            kprotocol::UUID{{{0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
                              0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F}}},
            kprotocol::UUID{{{0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,
                              0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F}}},
        }, 0x77);

    std::cout << "  slot_array round-trip... " << std::flush;
    {
        kprotocol::PacketRegistry registry;
        registry.register_schema(make_schema("test.slot_array",
            {{"stacks", kprotocol::FieldType::slot_array}}, 0x78));
        kprotocol::types::Slot a;
        a.present = true;
        a.item_id = 42;
        a.count = 3;
        kprotocol::types::Slot b;
        b.present = true;
        b.item_id = 7;
        b.count = 64;
        kprotocol::Packet p;
        p.key = "test.slot_array";
        p.state = kState;
        p.direction = kDir;
        p.fields["stacks"] = std::vector<kprotocol::types::Slot>{a, b};
        const auto encoded = registry.encode_packet(p, kVersion);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
        const auto decoded = registry.decode_packet(frame, kVersion, kState, kDir);
        const auto out = std::get<std::vector<kprotocol::types::Slot>>(decoded.fields.at("stacks"));
        KPC_CHECK(out.size() == 2, "slot_array size");
        KPC_CHECK(out[0].present && out[0].item_id == 42 && out[0].count == 3, "slot_array first");
        KPC_CHECK(out[1].present && out[1].item_id == 7 && out[1].count == 64, "slot_array second");
    }
    std::cout << "ok\n";

    std::cout << "  slot round-trip... " << std::flush;
    {
        kprotocol::PacketRegistry registry;
        registry.register_schema(make_schema("test.slot",
            {{"stack", kprotocol::FieldType::slot}}, 0x74));
        kprotocol::types::Slot slot;
        slot.present = true;
        slot.item_id = 42;
        slot.count = 3;
        kprotocol::Packet p;
        p.key = "test.slot";
        p.state = kState;
        p.direction = kDir;
        p.fields["stack"] = slot;
        const auto encoded = registry.encode_packet(p, kVersion);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
        const auto decoded = registry.decode_packet(frame, kVersion, kState, kDir);
        const auto out = std::get<kprotocol::types::Slot>(decoded.fields.at("stack"));
        KPC_CHECK(out.present && out.item_id == 42 && out.count == 3, "slot fields");
    }
    std::cout << "ok\n";

    std::cout << "All field-type round-trip tests passed.\n";
    return 0;
}
