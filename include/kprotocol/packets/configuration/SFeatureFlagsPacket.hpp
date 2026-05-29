#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <string>
#include <vector>

namespace kprotocol {

class SFeatureFlagsPacket {
public:
    std::vector<std::string> features{"minecraft:vanilla"};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::configuration_feature_flags,
            .state = PacketState::configuration,
            .direction = PacketDirection::clientbound,
            .fields = {{"features", features}}
        };
    }

    static SFeatureFlagsPacket from_packet(const Packet& packet) {
        return SFeatureFlagsPacket{
            .features = require_field<std::vector<std::string>>(packet.fields, "features")
        };
    }
};

} // namespace kprotocol
