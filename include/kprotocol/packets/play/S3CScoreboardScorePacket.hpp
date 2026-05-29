#pragma once

#include "kprotocol/codec.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/version.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace kprotocol {

class S3CScoreboardScorePacket {
public:
    std::string item_name;
    std::string objective_name;
    std::int32_t value{};
    // Legacy (<1.20.4): 0 create/update, 1 remove.
    std::int32_t legacy_action{};

    [[nodiscard]] static S3CScoreboardScorePacket make_set(
        std::string item_name,
        std::string objective_name,
        const std::int32_t value) {

        return S3CScoreboardScorePacket{
            .item_name = std::move(item_name),
            .objective_name = std::move(objective_name),
            .value = value,
            .legacy_action = 0,
        };
    }

    [[nodiscard]] static S3CScoreboardScorePacket make_remove(
        std::string item_name,
        std::string objective_name) {

        return S3CScoreboardScorePacket{
            .item_name = std::move(item_name),
            .objective_name = std::move(objective_name),
            .value = 0,
            .legacy_action = 1,
        };
    }

    [[nodiscard]] Packet to_packet(const ProtocolVersion wire_version) const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_scoreboard_score;
#else
        const std::string_view key = "play.clientbound.scoreboard_score";
#endif
        PacketFields fields;
        fields.emplace("itemName", item_name);
        fields.emplace("scoreName", objective_name);

        if (protocol_number(wire_version) >= protocol_number(ProtocolVersion::v1_20_4)) {
            fields.emplace("value", value);
            fields.emplace("display_name_present", false);
            fields.emplace("number_format_present", false);
            fields.emplace("tail", std::vector<std::uint8_t>{});
        } else {
            fields.emplace("action", legacy_action);
            if (legacy_action == 0) {
                std::vector<std::uint8_t> tail;
                codec::write_var_int(tail, value);
                fields.emplace("tail", std::move(tail));
            } else {
                fields.emplace("tail", std::vector<std::uint8_t>{});
            }
        }

        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = std::move(fields),
        };
    }

    static S3CScoreboardScorePacket from_packet(const Packet& packet) {
        S3CScoreboardScorePacket out{
            .item_name = require_field<std::string>(packet.fields, "itemName"),
            .objective_name = require_field<std::string>(packet.fields, "scoreName"),
        };
        if (const auto it = packet.fields.find("value"); it != packet.fields.end()) {
            out.value = std::get<std::int32_t>(it->second);
        }
        if (const auto it = packet.fields.find("action"); it != packet.fields.end()) {
            out.legacy_action = std::get<std::int32_t>(it->second);
        }
        return out;
    }
};

} // namespace kprotocol
