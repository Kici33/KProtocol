#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <cstdint>

namespace kprotocol {

class C01PingRequestPacket {
public:
    std::int64_t payload{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::ping_request,
            .state = PacketState::status,
            .direction = PacketDirection::serverbound,
            .fields = {{"payload", payload}}
        };
    }

    static C01PingRequestPacket from_packet(const Packet& packet) {
        return C01PingRequestPacket{
            .payload = require_field<std::int64_t>(packet.fields, "payload")
        };
    }
};

} // namespace kprotocol
