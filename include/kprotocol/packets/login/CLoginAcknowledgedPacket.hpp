#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

namespace kprotocol {

class CLoginAcknowledgedPacket {
public:
    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_acknowledged,
            .state = PacketState::login,
            .direction = PacketDirection::serverbound,
            .fields = {}
        };
    }

    static CLoginAcknowledgedPacket from_packet(const Packet&) {
        return {};
    }
};

} // namespace kprotocol
