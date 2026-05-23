#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

namespace kprotocol {

class C00StatusRequestPacket {
public:
    Packet to_packet() const {
        return Packet{
            .key = packet_keys::status_request,
            .state = PacketState::status,
            .direction = PacketDirection::serverbound,
            .fields = {}
        };
    }

    static C00StatusRequestPacket from_packet(const Packet&) {
        return {};
    }
};

} // namespace kprotocol
