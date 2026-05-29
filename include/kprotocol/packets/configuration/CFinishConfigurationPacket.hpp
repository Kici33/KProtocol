#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

namespace kprotocol {

class CFinishConfigurationPacket {
public:
    Packet to_packet() const {
        return Packet{
            .key = packet_keys::configuration_finish_serverbound,
            .state = PacketState::configuration,
            .direction = PacketDirection::serverbound,
            .fields = {}
        };
    }

    static CFinishConfigurationPacket from_packet(const Packet&) {
        return {};
    }
};

} // namespace kprotocol
