#pragma once

#include "kprotocol/codec.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/text_component.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace kprotocol {

enum class ScoreboardObjectiveAction : std::int8_t {
    create = 0,
    remove = 1,
    update = 2,
};

class S3BScoreboardObjectivePacket {
public:
    std::string name;
    ScoreboardObjectiveAction action{ScoreboardObjectiveAction::create};
    std::string display_text;
    std::int32_t render_type{};
    std::vector<std::uint8_t> tail;

    [[nodiscard]] static S3BScoreboardObjectivePacket make_create(
        std::string objective_name,
        const std::string& display_name) {

        S3BScoreboardObjectivePacket packet{
            .name = std::move(objective_name),
            .action = ScoreboardObjectiveAction::create,
            .display_text = display_name,
            .render_type = 0,
        };
        packet.tail = encode_create_tail(display_name);
        return packet;
    }

    [[nodiscard]] static S3BScoreboardObjectivePacket make_update(
        std::string objective_name,
        const std::string& display_name) {

        S3BScoreboardObjectivePacket packet{
            .name = std::move(objective_name),
            .action = ScoreboardObjectiveAction::update,
            .display_text = display_name,
            .render_type = 0,
        };
        std::vector<std::uint8_t> out;
        codec::write_string(out, json_text(display_name));
        packet.tail = std::move(out);
        return packet;
    }

    [[nodiscard]] static S3BScoreboardObjectivePacket make_remove(std::string objective_name) {
        return S3BScoreboardObjectivePacket{
            .name = std::move(objective_name),
            .action = ScoreboardObjectiveAction::remove,
            .tail = {},
        };
    }

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_scoreboard_objective;
#else
        const std::string_view key = "play.clientbound.scoreboard_objective";
#endif
        PacketFields fields{
            {"name", name},
            {"action", static_cast<std::int8_t>(action)},
            {"tail", tail},
        };
        if (action == ScoreboardObjectiveAction::create ||
            action == ScoreboardObjectiveAction::update) {
            fields["displayText"] = display_text;
            fields["type"] = render_type;
        }
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = std::move(fields),
        };
    }

    static S3BScoreboardObjectivePacket from_packet(const Packet& packet) {
        return S3BScoreboardObjectivePacket{
            .name = require_field<std::string>(packet.fields, "name"),
            .action = static_cast<ScoreboardObjectiveAction>(
                require_field<std::int8_t>(packet.fields, "action")),
            .display_text = field_or<std::string>(packet.fields, "displayText", {}),
            .render_type = field_or<std::int32_t>(packet.fields, "type", 0),
            .tail = field_or<std::vector<std::uint8_t>>(packet.fields, "tail", {}),
        };
    }

private:
    [[nodiscard]] static std::vector<std::uint8_t> encode_create_tail(const std::string& display_name) {
        std::vector<std::uint8_t> out;
        codec::write_string(out, json_text(display_name));
        codec::write_string(out, "integer");
        return out;
    }
};

} // namespace kprotocol
