#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/types.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <string>

namespace kprotocol {

class S24BlockActionPacket {
public:
    Position location{};
    std::uint8_t byte1{};
    std::uint8_t byte2{};
    BlockState block{};

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_block_action;
#else
        const std::string_view key = "play.clientbound.block_action";
#endif
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"location", location},
                {"byte1", byte1},
                {"byte2", byte2},
                {"blockId", block.id},
            }
        };
    }

    static S24BlockActionPacket from_packet(const Packet& packet) {
        return S24BlockActionPacket{
            .location = require_field<Position>(packet.fields, "location"),
            .byte1 = static_cast<std::uint8_t>(require_field<std::uint8_t>(packet.fields, "byte1")),
            .byte2 = static_cast<std::uint8_t>(require_field<std::uint8_t>(packet.fields, "byte2")),
            .block = BlockState{require_field<std::int32_t>(packet.fields, "blockId")},
        };
    }
};

} // namespace kprotocol
