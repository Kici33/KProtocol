#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <string>

namespace kprotocol {

class C00LoginStartPacket {
public:
    std::string username;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_start,
            .state = PacketState::login,
            .direction = PacketDirection::serverbound,
            .fields = {{"username", username}}
        };
    }

    static C00LoginStartPacket from_packet(const Packet& packet) {
        return C00LoginStartPacket{
            .username = require_field<std::string>(packet.fields, "username")
        };
    }
};

} // namespace kprotocol
