#pragma once

#include "kprotocol/packet.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace kprotocol {

class S3BScoreboardObjectivePacket {
public:
    std::string name;
    std::int8_t action{};
    std::vector<std::uint8_t> tail;

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_scoreboard_objective;
#else
        const std::string_view key = "play.clientbound.scoreboard_objective";
#endif
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"name", name},
                {"action", action},
                {"tail", tail},
            }
        };
    }

    static S3BScoreboardObjectivePacket from_packet(const Packet& packet) {
        return S3BScoreboardObjectivePacket{
            .name = require_field<std::string>(packet.fields, "name"),
            .action = require_field<std::int8_t>(packet.fields, "action"),
            .tail = field_or<std::vector<std::uint8_t>>(packet.fields, "tail", {}),
        };
    }
};

} // namespace kprotocol
