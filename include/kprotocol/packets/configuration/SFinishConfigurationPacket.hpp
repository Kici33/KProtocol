#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

namespace kprotocol {

class SFinishConfigurationPacket {
public:
    Packet to_packet() const {
        return Packet{
            .key = packet_keys::configuration_finish_clientbound,
            .state = PacketState::configuration,
            .direction = PacketDirection::clientbound,
            .fields = {}
        };
    }

    static SFinishConfigurationPacket from_packet(const Packet&) {
        return {};
    }
};

} // namespace kprotocol
