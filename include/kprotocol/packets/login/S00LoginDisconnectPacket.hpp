#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <string>

namespace kprotocol {

class S00LoginDisconnectPacket {
public:
    std::string reason_json;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_disconnect,
            .state = PacketState::login,
            .direction = PacketDirection::clientbound,
            .fields = {{"reason_json", reason_json}}
        };
    }

    static S00LoginDisconnectPacket from_packet(const Packet& packet) {
        return S00LoginDisconnectPacket{
            .reason_json = require_field<std::string>(packet.fields, "reason_json")
        };
    }
};

} // namespace kprotocol
