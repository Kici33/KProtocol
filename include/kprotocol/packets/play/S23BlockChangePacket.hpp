#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/types.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <string>

namespace kprotocol {

class S23BlockChangePacket {
public:
    Position location{};
    BlockState block{};

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_block_change;
#else
        const std::string_view key = "play.clientbound.block_change";
#endif
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"location", location},
                {"type", block.id},
            }
        };
    }

    static S23BlockChangePacket from_packet(const Packet& packet) {
        return S23BlockChangePacket{
            .location = require_field<Position>(packet.fields, "location"),
            .block = BlockState{require_field<std::int32_t>(packet.fields, "type")},
        };
    }
};

} // namespace kprotocol
