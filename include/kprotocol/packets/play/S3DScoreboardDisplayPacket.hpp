#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/version.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>
#include <string>

namespace kprotocol {

class S3DScoreboardDisplayPacket {
public:
    std::int8_t position{};
    std::string name;

    [[nodiscard]] Packet to_packet(const ProtocolVersion wire_version) const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_scoreboard_display_objective;
#else
        const std::string_view key = "play.clientbound.scoreboard_display_objective";
#endif
        PacketFields fields;
        fields.emplace("name", name);
        if (protocol_number(wire_version) >= protocol_number(ProtocolVersion::v1_20_2)) {
            fields.emplace("position", static_cast<std::int32_t>(position));
        } else {
            fields.emplace("position", position);
        }
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = std::move(fields),
        };
    }

    static S3DScoreboardDisplayPacket from_packet(const Packet& packet) {
        S3DScoreboardDisplayPacket out{
            .name = require_field<std::string>(packet.fields, "name"),
        };
        const auto it = packet.fields.find("position");
        if (it != packet.fields.end()) {
            if (const auto* i8 = std::get_if<std::int8_t>(&it->second); i8 != nullptr) {
                out.position = *i8;
            } else if (const auto* i32 = std::get_if<std::int32_t>(&it->second); i32 != nullptr) {
                out.position = static_cast<std::int8_t>(*i32);
            }
        }
        return out;
    }
};

} // namespace kprotocol
