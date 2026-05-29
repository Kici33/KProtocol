#include "kprotocol/codec.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/text_component.hpp"
#include "kprotocol/types.hpp"
#include "kprotocol/version.hpp"

#include <cstdint>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            return 1;                                                          \
        }                                                                      \
    } while (false)

namespace {

kprotocol::NBTValue nbt_string(std::string value) {
    kprotocol::NBTValue out;
    out.type = kprotocol::NBTTagType::string;
    out.string_value = std::move(value);
    return out;
}

kprotocol::NBTValue nbt_int(std::int32_t value) {
    kprotocol::NBTValue out;
    out.type = kprotocol::NBTTagType::int_;
    out.int_value = value;
    return out;
}

kprotocol::NBTValue nbt_long_list(std::vector<std::int64_t> values) {
    kprotocol::NBTValue out;
    out.type = kprotocol::NBTTagType::list;
    out.list_element_type = kprotocol::NBTTagType::long_;
    for (const auto value : values) {
        kprotocol::NBTValue item;
        item.type = kprotocol::NBTTagType::long_;
        item.long_value = value;
        out.list_values.push_back(item);
    }
    return out;
}

kprotocol::NBTValue nbt_compound() {
    kprotocol::NBTValue root;
    root.type = kprotocol::NBTTagType::compound;
    root.compound_names = {"name", "health", "ticks"};
    root.compound_values = {
        nbt_string("KProtocol"),
        nbt_int(20),
        nbt_long_list({1, 2, 3}),
    };
    return root;
}

kprotocol::NBTBlob anonymous_text_component(std::string text) {
    return kprotocol::text_component_nbt(text);
}

} // namespace

int main() {
    std::cout << "nbt_tests:\n";

    {
        std::cout << "  named NBT tree encodes and decodes... " << std::flush;
        kprotocol::NBTNamedTag root;
        root.name = "root";
        root.value = nbt_compound();

        const auto blob = kprotocol::types::encode_named_nbt(root);
        const auto decoded = kprotocol::types::decode_named_nbt(blob);
        KPC_CHECK(decoded == root, "named NBT round-trip");
        std::cout << "ok\n";
    }

    {
        std::cout << "  anonymous packet NBT validates with u16 strings... " << std::flush;
        const auto blob = anonymous_text_component("Hello");
        std::vector<std::uint8_t> out;
        kprotocol::types::write_nbt(out, blob);
        KPC_CHECK(out == blob.data, "write_nbt preserves anonymous payload");

        std::size_t offset = 0;
        const auto decoded = kprotocol::types::read_nbt(out, offset);
        KPC_CHECK(offset == out.size(), "anonymous NBT consumed");
        KPC_CHECK(decoded.data == blob.data, "anonymous NBT bytes round-trip");
        std::cout << "ok\n";
    }

    {
        std::cout << "  optional_nbt field round-trips through PacketRegistry... " << std::flush;
        kprotocol::PacketRegistry registry;
        kprotocol::PacketSchema schema;
        schema.key = "test.optional_nbt";
        schema.state = kprotocol::PacketState::play;
        schema.direction = kprotocol::PacketDirection::clientbound;
        schema.ids[kprotocol::ProtocolVersion::v1_20_4] = 0x55;
        schema.field_sets[kprotocol::ProtocolVersion::v1_20_4] = {
            {"nbt", kprotocol::FieldType::optional_nbt},
        };
        registry.register_schema(std::move(schema));

        kprotocol::Packet packet;
        packet.key = "test.optional_nbt";
        packet.state = kprotocol::PacketState::play;
        packet.direction = kprotocol::PacketDirection::clientbound;
        packet.fields["nbt"] = anonymous_text_component("Registry");

        const auto encoded = registry.encode_packet(packet, kprotocol::ProtocolVersion::v1_20_4);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
        const auto decoded = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::PacketState::play,
            kprotocol::PacketDirection::clientbound);
        KPC_CHECK(
            std::get<kprotocol::NBTBlob>(decoded.fields.at("nbt")).data ==
                std::get<kprotocol::NBTBlob>(packet.fields.at("nbt")).data,
            "optional_nbt value");
        std::cout << "ok\n";
    }

    {
        std::cout << "  optional_nbt_array field round-trips through PacketRegistry... " << std::flush;
        kprotocol::PacketRegistry registry;
        kprotocol::PacketSchema schema;
        schema.key = "test.optional_nbt_array";
        schema.state = kprotocol::PacketState::play;
        schema.direction = kprotocol::PacketDirection::clientbound;
        schema.ids[kprotocol::ProtocolVersion::v1_20_4] = 0x56;
        schema.field_sets[kprotocol::ProtocolVersion::v1_20_4] = {
            {"nbt_values", kprotocol::FieldType::optional_nbt_array},
        };
        registry.register_schema(std::move(schema));

        const std::vector<kprotocol::NBTBlob> values = {
            anonymous_text_component("A"),
            {},
            anonymous_text_component("B"),
        };
        kprotocol::Packet packet;
        packet.key = "test.optional_nbt_array";
        packet.state = kprotocol::PacketState::play;
        packet.direction = kprotocol::PacketDirection::clientbound;
        packet.fields["nbt_values"] = values;

        const auto encoded = registry.encode_packet(packet, kprotocol::ProtocolVersion::v1_20_4);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
        const auto decoded = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_20_4,
            kprotocol::PacketState::play,
            kprotocol::PacketDirection::clientbound);
        KPC_CHECK(
            std::get<std::vector<kprotocol::NBTBlob>>(decoded.fields.at("nbt_values")) == values,
            "optional_nbt_array value");
        std::cout << "ok\n";
    }

    {
        std::cout << "  slot NBT round-trips with compound payload... " << std::flush;
        kprotocol::types::Slot slot;
        slot.present = true;
        slot.item_id = 1;
        slot.count = 64;
        slot.nbt = anonymous_text_component("Slot");
        const auto bytes = kprotocol::types::slot_to_bytes(slot);
        const auto decoded = kprotocol::types::slot_from_bytes(bytes);
        KPC_CHECK(decoded.nbt.data == slot.nbt.data, "slot NBT");
        std::cout << "ok\n";
    }

    {
        std::cout << "  malformed NBT is rejected... " << std::flush;
        const kprotocol::NBTBlob bad{{0x0A, 0x08, 0x00, 0x04, 't', 'e'}};
        bool threw = false;
        try {
            std::vector<std::uint8_t> out;
            kprotocol::types::write_nbt(out, bad);
        } catch (const std::exception&) {
            threw = true;
        }
        KPC_CHECK(threw, "truncated NBT rejected");
        std::cout << "ok\n";
    }

    std::cout << "All NBT tests passed.\n";
    return 0;
}
