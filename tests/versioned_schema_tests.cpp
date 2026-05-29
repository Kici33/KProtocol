// Wave 2: PacketSchema and version-aware lookup tests.
//
// The schema describes a packet whose wire layout differs by protocol
// version. We register one PacketSchema covering multiple versions and
// confirm that:
//   - encode_packet/decode_packet pick the right field set per version
//   - packet_id_for returns the correct id per version
//   - fields_for / schema_for return non-null views
//   - merging via repeated register_schema() calls works as documented

#include "kprotocol/codec.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/version.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

// We deliberately do NOT include <cassert>: this file uses `assert(x)` as a
// hard-fail check that must remain effective in Release builds (NDEBUG).
#ifdef assert
#undef assert
#endif
#define assert(cond)                                                          \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d)\n",                         \
                         #cond, __FILE__, __LINE__);                           \
            std::abort();                                                      \
        }                                                                      \
    } while (false)

namespace {

void test_per_version_field_layout_selected() {
    std::cout << "  field set selection picks highest <= requested version... ";
    kprotocol::PacketRegistry registry;

    // Pretend a packet has 1 string field at v1.8, and 2 string fields at
    // v1.20.4. Any version between 1.8 and (1.20.4 - 1) inherits the 1.8
    // layout; v1.20.4 and onwards uses the 2-field layout.
    kprotocol::PacketSchema schema;
    schema.key = "play.test.versioned";
    schema.state = kprotocol::PacketState::play;
    schema.direction = kprotocol::PacketDirection::clientbound;
    schema.ids[kprotocol::KnownVersion::v1_8]    = 0x20;
    schema.ids[kprotocol::KnownVersion::v1_20_4] = 0x21;
    schema.field_sets[kprotocol::KnownVersion::v1_8] = {
        {"a", kprotocol::FieldType::string},
    };
    schema.field_sets[kprotocol::KnownVersion::v1_20_4] = {
        {"a", kprotocol::FieldType::string},
        {"b", kprotocol::FieldType::string},
    };
    registry.register_schema(schema);

    // v1.8 encode/decode: 1 field.
    {
        kprotocol::Packet p;
        p.key = "play.test.versioned";
        p.state = kprotocol::PacketState::play;
        p.direction = kprotocol::PacketDirection::clientbound;
        p.fields["a"] = std::string("hello");
        const auto encoded = registry.encode_packet(p, kprotocol::KnownVersion::v1_8);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        assert(frame.packet_id == 0x20);
        const auto decoded = registry.decode_packet(
            frame, kprotocol::KnownVersion::v1_8,
            kprotocol::PacketState::play, kprotocol::PacketDirection::clientbound);
        assert(decoded.fields.size() == 1);
        assert(std::get<std::string>(decoded.fields.at("a")) == "hello");
    }

    // v1.16.5 inherits the v1.8 layout (highest key <= 754 is 47).
    {
        const auto* fields = registry.fields_for("play.test.versioned",
                                                 kprotocol::KnownVersion::v1_16_5);
        assert(fields != nullptr);
        assert(fields->size() == 1);
        assert((*fields)[0].name == "a");
    }

    // v1.20.4 uses the 2-field layout.
    {
        kprotocol::Packet p;
        p.key = "play.test.versioned";
        p.state = kprotocol::PacketState::play;
        p.direction = kprotocol::PacketDirection::clientbound;
        p.fields["a"] = std::string("foo");
        p.fields["b"] = std::string("bar");
        const auto encoded = registry.encode_packet(p, kprotocol::KnownVersion::v1_20_4);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        assert(frame.packet_id == 0x21);
        const auto decoded = registry.decode_packet(
            frame, kprotocol::KnownVersion::v1_20_4,
            kprotocol::PacketState::play, kprotocol::PacketDirection::clientbound);
        assert(decoded.fields.size() == 2);
        assert(std::get<std::string>(decoded.fields.at("a")) == "foo");
        assert(std::get<std::string>(decoded.fields.at("b")) == "bar");
    }
    std::cout << "ok\n";
}

void test_below_minimum_declared_version_is_unsupported() {
    std::cout << "  encode/decode for version below all declared keys is rejected... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketSchema schema;
    schema.key = "play.test.lowfloor";
    schema.state = kprotocol::PacketState::play;
    schema.direction = kprotocol::PacketDirection::clientbound;
    schema.ids[kprotocol::KnownVersion::v1_20_4] = 0x25;
    schema.field_sets[kprotocol::KnownVersion::v1_20_4] = {
        {"v", kprotocol::FieldType::var_int},
    };
    registry.register_schema(std::move(schema));

    // packet_id_for at v1_8: no id declared for that version.
    const auto id = registry.packet_id_for("play.test.lowfloor",
                                           kprotocol::KnownVersion::v1_8);
    assert(!id.has_value());

    // encode_packet at v1_8 must throw.
    kprotocol::Packet p;
    p.key = "play.test.lowfloor";
    p.state = kprotocol::PacketState::play;
    p.direction = kprotocol::PacketDirection::clientbound;
    p.fields["v"] = std::int32_t{1};
    bool threw = false;
    try {
        (void)registry.encode_packet(p, kprotocol::KnownVersion::v1_8);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok\n";
}

void test_repeated_register_merges() {
    std::cout << "  repeated register_schema on the same key merges field_sets + ids... ";
    kprotocol::PacketRegistry registry;

    {
        kprotocol::PacketSchema schema;
        schema.key = "play.test.merged";
        schema.state = kprotocol::PacketState::play;
        schema.direction = kprotocol::PacketDirection::clientbound;
        schema.ids[kprotocol::KnownVersion::v1_8] = 0x30;
        schema.field_sets[kprotocol::KnownVersion::v1_8] = {
            {"x", kprotocol::FieldType::var_int},
        };
        registry.register_schema(std::move(schema));
    }
    {
        kprotocol::PacketSchema schema;
        schema.key = "play.test.merged";
        schema.state = kprotocol::PacketState::play;
        schema.direction = kprotocol::PacketDirection::clientbound;
        schema.ids[kprotocol::KnownVersion::v1_20_4] = 0x31;
        schema.field_sets[kprotocol::KnownVersion::v1_20_4] = {
            {"x", kprotocol::FieldType::var_int},
            {"y", kprotocol::FieldType::var_int},
        };
        registry.register_schema(std::move(schema));
    }

    const auto* schema = registry.schema_for("play.test.merged");
    assert(schema != nullptr);
    assert(schema->ids.size() == 2);
    assert(schema->field_sets.size() == 2);
    assert(schema->field_sets.at(kprotocol::KnownVersion::v1_8).size() == 1);
    assert(schema->field_sets.at(kprotocol::KnownVersion::v1_20_4).size() == 2);
    std::cout << "ok\n";
}

void test_condition_field_controls_switch_payload() {
    std::cout << "  condition fields encode/decode switch-shaped payloads... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketSchema schema;
    schema.key = "play.test.conditional";
    schema.state = kprotocol::PacketState::play;
    schema.direction = kprotocol::PacketDirection::clientbound;
    schema.ids[kprotocol::KnownVersion::v1_20_4] = 0x32;
    schema.field_sets[kprotocol::KnownVersion::v1_20_4] = {
        {"action", kprotocol::FieldType::var_int},
        {"title", kprotocol::FieldType::string, "", "action", {0, 3}},
        {"health", kprotocol::FieldType::f32_be, "", "action", {0, 2}},
    };
    registry.register_schema(std::move(schema));

    {
        kprotocol::Packet p;
        p.key = "play.test.conditional";
        p.state = kprotocol::PacketState::play;
        p.direction = kprotocol::PacketDirection::clientbound;
        p.fields["action"] = std::int32_t{2};
        p.fields["health"] = 0.75F;

        const auto encoded = registry.encode_packet(p, kprotocol::KnownVersion::v1_20_4);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded = registry.decode_packet(
            frame, kprotocol::KnownVersion::v1_20_4,
            kprotocol::PacketState::play, kprotocol::PacketDirection::clientbound);
        assert(decoded.fields.size() == 2);
        assert(std::get<std::int32_t>(decoded.fields.at("action")) == 2);
        assert(std::get<float>(decoded.fields.at("health")) == 0.75F);
        assert(decoded.fields.find("title") == decoded.fields.end());
    }

    {
        kprotocol::Packet p;
        p.key = "play.test.conditional";
        p.state = kprotocol::PacketState::play;
        p.direction = kprotocol::PacketDirection::clientbound;
        p.fields["action"] = std::int32_t{1};

        const auto encoded = registry.encode_packet(p, kprotocol::KnownVersion::v1_20_4);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded = registry.decode_packet(
            frame, kprotocol::KnownVersion::v1_20_4,
            kprotocol::PacketState::play, kprotocol::PacketDirection::clientbound);
        assert(decoded.fields.size() == 1);
        assert(std::get<std::int32_t>(decoded.fields.at("action")) == 1);
    }
    std::cout << "ok\n";
}

void test_conflicting_state_rejected() {
    std::cout << "  re-registering with conflicting state throws... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketSchema schema;
    schema.key = "play.test.conflict";
    schema.state = kprotocol::PacketState::play;
    schema.direction = kprotocol::PacketDirection::clientbound;
    schema.ids[kprotocol::KnownVersion::v1_20_4] = 0x40;
    schema.field_sets[kprotocol::KnownVersion::v1_20_4] = {};
    registry.register_schema(schema);

    kprotocol::PacketSchema bad = schema;
    bad.state = kprotocol::PacketState::login;
    bool threw = false;
    try {
        registry.register_schema(std::move(bad));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok\n";
}

void test_id_collision_across_keys_rejected() {
    std::cout << "  two keys claiming the same (version, state, direction, id) is rejected... ";
    kprotocol::PacketRegistry registry;
    {
        kprotocol::PacketSchema schema;
        schema.key = "play.test.first";
        schema.state = kprotocol::PacketState::play;
        schema.direction = kprotocol::PacketDirection::clientbound;
        schema.ids[kprotocol::KnownVersion::v1_20_4] = 0x50;
        schema.field_sets[kprotocol::KnownVersion::v1_20_4] = {};
        registry.register_schema(std::move(schema));
    }
    kprotocol::PacketSchema collision;
    collision.key = "play.test.second";
    collision.state = kprotocol::PacketState::play;
    collision.direction = kprotocol::PacketDirection::clientbound;
    collision.ids[kprotocol::KnownVersion::v1_20_4] = 0x50;
    collision.field_sets[kprotocol::KnownVersion::v1_20_4] = {};

    bool threw = false;
    try {
        registry.register_schema(std::move(collision));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "versioned_schema_tests:\n";
    test_per_version_field_layout_selected();
    test_below_minimum_declared_version_is_unsupported();
    test_repeated_register_merges();
    test_condition_field_controls_switch_payload();
    test_conflicting_state_rejected();
    test_id_collision_across_keys_rejected();
    std::cout << "All versioned schema tests passed.\n";
    return 0;
}
