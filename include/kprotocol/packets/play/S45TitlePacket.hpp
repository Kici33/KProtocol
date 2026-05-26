#pragma once

#include "kprotocol/packet.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace kprotocol {

// Legacy combined title packet (1.8–1.16.5): action + tail rest_buffer.
class S45TitlePacket {
public:
    std::int32_t action{};
    std::vector<std::uint8_t> tail;

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_title;
#else
        const std::string_view key = "play.clientbound.title";
#endif
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"action", action},
                {"tail", tail},
            }
        };
    }

    static S45TitlePacket from_packet(const Packet& packet) {
        return S45TitlePacket{
            .action = require_field<std::int32_t>(packet.fields, "action"),
            .tail = field_or<std::vector<std::uint8_t>>(packet.fields, "tail", {}),
        };
    }
};

} // namespace kprotocol
