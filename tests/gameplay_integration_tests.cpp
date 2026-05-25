// Integration tests: block change, title, scoreboard encode/decode across wire 47 and 767.

#include "kprotocol/codec.hpp"
#include "kprotocol/entity_metadata.hpp"
#include "kprotocol/initialize.hpp"
#include "kprotocol/packets/play/S23BlockChangePacket.hpp"
#include "kprotocol/packets/play/S45TitlePacket.hpp"
#include "kprotocol/packets/play/S60SetTitleTextPacket.hpp"
#include "kprotocol/packets/play/S55ActionBarPacket.hpp"
#include "kprotocol/packets/play/S3BScoreboardObjectivePacket.hpp"
#include "kprotocol/packets/play/S3CScoreboardScorePacket.hpp"
#include "kprotocol/packets/play/S3DScoreboardDisplayPacket.hpp"
#include "kprotocol/text_component.hpp"
#include "kprotocol/translation_registry.hpp"

#include "kprotocol/generated/packet_keys.hpp"

#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <vector>

#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            throw std::runtime_error(msg);                                     \
        }                                                                      \
    } while (false)

namespace {

void roundtrip_packet(
    kprotocol::PacketRegistry& registry,
    kprotocol::Packet packet,
    kprotocol::ProtocolVersion version) {

    const auto encoded = registry.encode_packet(packet, version);
    kprotocol::codec::EncodedFrame frame;
    std::size_t consumed = 0;
    KPC_CHECK(kprotocol::codec::try_decode_frame(encoded, consumed, frame), "frame decode");
    const auto decoded = registry.decode_packet(
        frame, version, packet.state, packet.direction);
    KPC_CHECK(decoded.key == packet.key, "key");
}

void test_block_change_roundtrip_1_8_and_1_21() {
    std::cout << "  block_change round-trip wire 47 and 767... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::S23BlockChangePacket pkt;
    pkt.location = kprotocol::Position{.x = 1, .y = 64, .z = -3};
    pkt.block = kprotocol::BlockState{.id = 1};

    roundtrip_packet(registry, pkt.to_packet(), kprotocol::ProtocolVersion::v1_8);
    roundtrip_packet(registry, pkt.to_packet(), kprotocol::ProtocolVersion::v1_21_1);
    std::cout << "ok\n";
}

void test_block_id_translation_1_8_to_1_21() {
    std::cout << "  block id translation 1.8 -> 1.21.1... ";
    kprotocol::PacketTranslator translator;
    kprotocol::TranslationRegistry::initialize_all(translator);

    const auto mapped = kprotocol::TranslationRegistry::map_block_id(
        kprotocol::ProtocolVersion::v1_8,
        kprotocol::ProtocolVersion::v1_21_1,
        1);
    KPC_CHECK(mapped == 0, "stone default state remapped for 1.21.1");
    std::cout << "ok (id=" << mapped << ")\n";
}

void test_title_roundtrip_1_8() {
    std::cout << "  legacy title round-trip wire 47... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::S45TitlePacket title;
    title.action = 0;
    title.tail = [] {
        std::vector<std::uint8_t> out;
        kprotocol::codec::write_string(out, R"({"text":"Hello"})");
        return out;
    }();

    roundtrip_packet(registry, title.to_packet(), kprotocol::ProtocolVersion::v1_8);
    std::cout << "ok\n";
}

void test_scoreboard_display_roundtrip() {
    std::cout << "  scoreboard_display round-trip wire 47 and 767... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::S3DScoreboardDisplayPacket packet{
        .position = 1,
        .name = "sidebar",
    };

    roundtrip_packet(
        registry,
        packet.to_packet(kprotocol::ProtocolVersion::v1_8),
        kprotocol::ProtocolVersion::v1_8);
    roundtrip_packet(
        registry,
        packet.to_packet(kprotocol::ProtocolVersion::v1_21_1),
        kprotocol::ProtocolVersion::v1_21_1);
    std::cout << "ok\n";
}

void test_scoreboard_objective_and_score_roundtrip() {
    std::cout << "  scoreboard objective/score round-trip wire 47 and 770... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    const auto objective = kprotocol::S3BScoreboardObjectivePacket::make_create("obj", "Demo");
    roundtrip_packet(registry, objective.to_packet(), kprotocol::ProtocolVersion::v1_8);
    roundtrip_packet(registry, objective.to_packet(), kprotocol::ProtocolVersion::v1_21_5);

    const auto score = kprotocol::S3CScoreboardScorePacket::make_set("Player", "obj", 42);
    roundtrip_packet(
        registry,
        score.to_packet(kprotocol::ProtocolVersion::v1_8),
        kprotocol::ProtocolVersion::v1_8);
    roundtrip_packet(
        registry,
        score.to_packet(kprotocol::ProtocolVersion::v1_21_5),
        kprotocol::ProtocolVersion::v1_21_5);
    std::cout << "ok\n";
}

void test_action_bar_roundtrip_1_17_and_1_21() {
    std::cout << "  action_bar round-trip wire 755 and 767... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::S55ActionBarPacket packet{.text = "Hello"};
    roundtrip_packet(
        registry,
        packet.to_packet(kprotocol::ProtocolVersion::v1_17),
        kprotocol::ProtocolVersion::v1_17);
    roundtrip_packet(
        registry,
        packet.to_packet(kprotocol::ProtocolVersion::v1_21_1),
        kprotocol::ProtocolVersion::v1_21_1);
    std::cout << "ok\n";
}

void test_split_title_roundtrip_1_17() {
    std::cout << "  split title round-trip wire 755... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::S60SetTitleTextPacket title{.text = "Hello"};
    roundtrip_packet(
        registry,
        title.to_packet(kprotocol::ProtocolVersion::v1_17),
        kprotocol::ProtocolVersion::v1_17);
    std::cout << "ok\n";
}

void test_entity_metadata_roundtrip() {
    std::cout << "  entity_metadata round-trip wire 47 and 767... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    const auto metadata_blob = kprotocol::encode_entity_metadata({});

    kprotocol::Packet packet;
    packet.key = std::string(kprotocol::generated::packet_keys::play_clientbound_entity_metadata);
    packet.state = kprotocol::PacketState::play;
    packet.direction = kprotocol::PacketDirection::clientbound;
    packet.fields["entityId"] = static_cast<std::int32_t>(42);
    packet.fields["metadata"] = metadata_blob;

    roundtrip_packet(registry, packet, kprotocol::ProtocolVersion::v1_8);
    roundtrip_packet(registry, packet, kprotocol::ProtocolVersion::v1_21_1);
    std::cout << "ok\n";
}

void test_initialize_wires_translations() {
    std::cout << "  initialize() registers block_change translations... ";
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::Packet packet;
    packet.key = std::string(kprotocol::generated::packet_keys::play_clientbound_block_change);
    packet.state = kprotocol::PacketState::play;
    packet.direction = kprotocol::PacketDirection::clientbound;
    packet.fields["location"] = kprotocol::Position{};
    packet.fields["type"] = static_cast<std::int32_t>(1);

    const auto translated = translator.translate(
        packet,
        kprotocol::ProtocolVersion::v1_8,
        kprotocol::ProtocolVersion::v1_21_1);
    const auto& type = std::get<std::int32_t>(translated.fields.at("type"));
    KPC_CHECK(type == 0, "block id should translate to stone default on 1.21.1");
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "gameplay_integration_tests:\n";
    try {
        test_block_change_roundtrip_1_8_and_1_21();
        test_block_id_translation_1_8_to_1_21();
        test_title_roundtrip_1_8();
        test_split_title_roundtrip_1_17();
        test_action_bar_roundtrip_1_17_and_1_21();
        test_scoreboard_display_roundtrip();
        test_scoreboard_objective_and_score_roundtrip();
        test_entity_metadata_roundtrip();
        test_initialize_wires_translations();
    } catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "All gameplay integration tests passed.\n";
    return 0;
}
