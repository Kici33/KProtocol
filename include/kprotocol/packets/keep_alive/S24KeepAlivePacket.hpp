#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/packets/packet_keys.hpp"

#include <cstdint>

namespace kprotocol {

class S24KeepAlivePacket {
public:
    std::int64_t id{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::keep_alive_clientbound,
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {{"id", id}}
        };
    }

    static S24KeepAlivePacket from_packet(const Packet& packet) {
        return S24KeepAlivePacket{
            .id = require_field<std::int64_t>(packet.fields, "id")
        };
    }
};

} // namespace kprotocol
