#pragma once

#include "kprotocol/packet.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

namespace kprotocol {

class S10ClearTitlesPacket {
public:
    bool reset_times{};

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_clear_titles;
#else
        const std::string_view key = "play.clientbound.clear_titles";
#endif
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {{"reset", reset_times}},
        };
    }

    static S10ClearTitlesPacket from_packet(const Packet& packet) {
        return S10ClearTitlesPacket{
            .reset_times = require_field<bool>(packet.fields, "reset"),
        };
    }
};

} // namespace kprotocol
