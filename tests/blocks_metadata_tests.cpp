#include "kprotocol/block_registry.hpp"
#include "kprotocol/blocks.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/entity_metadata.hpp"
#include "kprotocol/initialize.hpp"
#include "kprotocol/metadata_registry.hpp"
#include "kprotocol/packets/play/S22MultiBlockChangePacket.hpp"
#include "kprotocol/packets/play/S24BlockActionPacket.hpp"
#include "kprotocol/packets/play/STileEntityDataPacket.hpp"
#include "kprotocol/packets/play/S23BlockChangePacket.hpp"

#include <cstdio>
#include <iostream>
#include <stdexcept>

#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            throw std::runtime_error(msg);                                     \
        }                                                                      \
    } while (false)

namespace {

void roundtrip(kprotocol::PacketRegistry& registry, kprotocol::Packet packet, kprotocol::ProtocolVersion version) {
    const auto encoded = registry.encode_packet(packet, version);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame");
    const auto decoded = registry.decode_packet(
        frame, version, packet.state, packet.direction);
    KPC_CHECK(decoded.key == packet.key, "key match");
}

void test_block_registry() {
    std::cout << "  block registry stone lookup... ";
    const auto id = kprotocol::BlockRegistry::default_state_id(kprotocol::ProtocolVersion::v1_8, "stone");
    KPC_CHECK(id.has_value(), "stone id");
    KPC_CHECK(*id == 1, "stone default on 1.8");
    std::cout << "ok\n";
}

void test_metadata_registry() {
    std::cout << "  metadata registry zombie silent index... ";
    const auto index = kprotocol::MetadataRegistry::index_for("zombie", "silent");
    KPC_CHECK(index.has_value(), "silent index");
    std::cout << "ok (index=" << static_cast<int>(*index) << ")\n";
}

void test_multi_block_legacy_roundtrip() {
    std::cout << "  multi_block_change legacy round-trip... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::S22MultiBlockChangePacket pkt;
    pkt.wire_version = kprotocol::ProtocolVersion::v1_8;
    pkt.chunk_x = 0;
    pkt.chunk_z = 0;
    pkt.legacy_records = {{5, 64, kprotocol::BlockState{2}}};
    roundtrip(registry, pkt.to_packet(), kprotocol::ProtocolVersion::v1_8);

    const auto decoded = kprotocol::S22MultiBlockChangePacket::from_packet(
        pkt.to_packet(), kprotocol::ProtocolVersion::v1_8);
    KPC_CHECK(decoded.legacy_records.size() == 1, "one record");
    KPC_CHECK(decoded.legacy_records[0].block.id == 2, "block id");
    std::cout << "ok\n";
}

void test_multi_block_modern_roundtrip() {
    std::cout << "  multi_block_change modern round-trip... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::S22MultiBlockChangePacket pkt;
    pkt.wire_version = kprotocol::ProtocolVersion::v1_21_1;
    pkt.chunk_coordinates = kprotocol::pack_chunk_coordinates(0, 64, 0);
    pkt.var_int_records = {42};
    roundtrip(registry, pkt.to_packet(), kprotocol::ProtocolVersion::v1_21_1);
    std::cout << "ok\n";
}

void test_tile_entity_roundtrip() {
    std::cout << "  tile_entity_data round-trip 1.8 and 1.21.1... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::STileEntityDataPacket legacy;
    legacy.wire_version = kprotocol::ProtocolVersion::v1_8;
    legacy.location = kprotocol::Position{1, 64, 2};
    legacy.action = 2;
    roundtrip(registry, legacy.to_packet(), kprotocol::ProtocolVersion::v1_8);

    kprotocol::STileEntityDataPacket modern;
    modern.wire_version = kprotocol::ProtocolVersion::v1_21_1;
    modern.location = kprotocol::Position{1, 64, 2};
    modern.action = 2;
    roundtrip(registry, modern.to_packet(), kprotocol::ProtocolVersion::v1_21_1);
    std::cout << "ok\n";
}

void test_block_action_roundtrip() {
    std::cout << "  block_action round-trip... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::S24BlockActionPacket pkt;
    pkt.location = kprotocol::Position{0, 64, 0};
    pkt.byte1 = 1;
    pkt.byte2 = 0;
    pkt.block = kprotocol::BlockState{1};
    roundtrip(registry, pkt.to_packet(), kprotocol::ProtocolVersion::v1_8);
    std::cout << "ok\n";
}

void test_entity_metadata_typed_decode() {
    std::cout << "  entity metadata typed decode... ";
    std::vector<kprotocol::MetadataEntry> entries;
    kprotocol::MetadataEntry silent;
    silent.index = 4;
    silent.type = kprotocol::MetadataType::boolean;
    silent.raw_value = {0x01};
    entries.push_back(silent);

    const auto blob = kprotocol::encode_entity_metadata(entries);
    const auto decoded = kprotocol::decode_entity_metadata(blob);
    KPC_CHECK(decoded.size() == 1, "one entry");
    KPC_CHECK(std::holds_alternative<bool>(decoded[0].value), "boolean value");
    KPC_CHECK(std::get<bool>(decoded[0].value), "true");
    std::cout << "ok\n";
}

void test_block_state_translation() {
    std::cout << "  block state translation 1.8 -> 1.21.1... ";
    const auto translated = kprotocol::BlockRegistry::translate(
        kprotocol::BlockState{1},
        kprotocol::ProtocolVersion::v1_8,
        kprotocol::ProtocolVersion::v1_21_1);
    KPC_CHECK(translated.id == 0, "stone remapped");
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "blocks_metadata_tests:\n";
    try {
        test_block_registry();
        test_metadata_registry();
        test_multi_block_legacy_roundtrip();
        test_multi_block_modern_roundtrip();
        test_tile_entity_roundtrip();
        test_block_action_roundtrip();
        test_entity_metadata_typed_decode();
        test_block_state_translation();
    } catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "All blocks/metadata tests passed.\n";
    return 0;
}
