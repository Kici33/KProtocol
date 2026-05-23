#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <string>

namespace kprotocol {

class S00StatusResponsePacket {
public:
    std::string json_response;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::status_response,
            .state = PacketState::status,
            .direction = PacketDirection::clientbound,
            .fields = {{"json_response", json_response}}
        };
    }

    static S00StatusResponsePacket from_packet(const Packet& packet) {
        return S00StatusResponsePacket{
            .json_response = require_field<std::string>(packet.fields, "json_response")
        };
    }
};

} // namespace kprotocol
