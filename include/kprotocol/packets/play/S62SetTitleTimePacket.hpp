#pragma once

#include "kprotocol/packet.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>

namespace kprotocol {

class S62SetTitleTimePacket {
public:
    std::int32_t fade_in{};
    std::int32_t stay{};
    std::int32_t fade_out{};

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_set_title_time;
#else
        const std::string_view key = "play.clientbound.set_title_time";
#endif
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"fadeIn", fade_in},
                {"stay", stay},
                {"fadeOut", fade_out},
            },
        };
    }

    static S62SetTitleTimePacket from_packet(const Packet& packet) {
        return S62SetTitleTimePacket{
            .fade_in = require_field<std::int32_t>(packet.fields, "fadeIn"),
            .stay = require_field<std::int32_t>(packet.fields, "stay"),
            .fade_out = require_field<std::int32_t>(packet.fields, "fadeOut"),
        };
    }
};

} // namespace kprotocol
