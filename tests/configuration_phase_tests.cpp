#include "kprotocol/configuration.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/initialize.hpp"
#include "kprotocol/packets/configuration/CFinishConfigurationPacket.hpp"
#include "kprotocol/packets/configuration/SFeatureFlagsPacket.hpp"
#include "kprotocol/packets/configuration/SFinishConfigurationPacket.hpp"
#include "kprotocol/packets/configuration/SRegistryDataPacket.hpp"
#include "kprotocol/packets/login/S02LoginSuccessPacket.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

int main() {
    std::cout << "configuration_phase_tests:\n";
    const char* step = "start";
    try {

    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    {
        step = "login_success";
        const auto packet = kprotocol::S02LoginSuccessPacket{
            .uuid = "00000000-0000-0000-0000-000000000001",
            .username = "ConfigPlayer",
        }.to_packet();
        const auto encoded = registry.encode_packet(packet, kprotocol::ProtocolVersion::v1_21_1);
        std::size_t consumed = 0;
        kprotocol::codec::EncodedFrame frame;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::login,
            kprotocol::PacketDirection::clientbound);
        const auto typed = kprotocol::S02LoginSuccessPacket::from_packet(decoded);
        assert(typed.uuid == "00000000-0000-0000-0000-000000000001");
        assert(typed.username == "ConfigPlayer");
    }

    {
        step = "feature_flags";
        const auto packet = kprotocol::SFeatureFlagsPacket{}.to_packet();
        const auto encoded = registry.encode_packet(packet, kprotocol::ProtocolVersion::v1_21_1);
        std::size_t consumed = 0;
        kprotocol::codec::EncodedFrame frame;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::configuration,
            kprotocol::PacketDirection::clientbound);
        const auto typed = kprotocol::SFeatureFlagsPacket::from_packet(decoded);
        assert(typed.features.size() == 1U);
        assert(typed.features[0] == "minecraft:vanilla");
    }

    {
        step = "clientbound_finish";
        const auto packet = kprotocol::SFinishConfigurationPacket{}.to_packet();
        const auto encoded = registry.encode_packet(packet, kprotocol::ProtocolVersion::v1_21_1);
        std::size_t consumed = 0;
        kprotocol::codec::EncodedFrame frame;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::configuration,
            kprotocol::PacketDirection::clientbound);
        assert(decoded.key == packet.key);
    }

    {
        step = "serverbound_finish";
        const auto packet = kprotocol::CFinishConfigurationPacket{}.to_packet();
        const auto encoded = registry.encode_packet(packet, kprotocol::ProtocolVersion::v1_21_1);
        std::size_t consumed = 0;
        kprotocol::codec::EncodedFrame frame;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        const auto decoded = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::configuration,
            kprotocol::PacketDirection::serverbound);
        assert(decoded.key == packet.key);
    }

    {
        step = "registry_data";
        const auto packet = kprotocol::SRegistryDataPacket{
            .id = "minecraft:dimension_type",
            .entries = {},
        }.to_packet();
        const auto encoded = registry.encode_packet(packet, kprotocol::ProtocolVersion::v1_21_1);
        std::size_t consumed = 0;
        kprotocol::codec::EncodedFrame frame;
        assert(kprotocol::codec::try_decode_frame(encoded, consumed, frame));
        assert(frame.packet_id == 7);
    }

    std::cout << "All configuration phase tests passed.\n";
    return 0;
    } catch (const std::exception& ex) {
        std::cerr << "configuration_phase_tests failed at " << step << ": " << ex.what() << '\n';
        return 1;
    }
}
