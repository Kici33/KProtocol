#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <cstdint>

namespace kprotocol {

class S01PongResponsePacket {
public:
    std::int64_t payload{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::pong_response,
            .state = PacketState::status,
            .direction = PacketDirection::clientbound,
            .fields = {{"payload", payload}}
        };
    }

    static S01PongResponsePacket from_packet(const Packet& packet) {
        return S01PongResponsePacket{
            .payload = require_field<std::int64_t>(packet.fields, "payload")
        };
    }
};

} // namespace kprotocol
