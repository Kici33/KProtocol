#pragma once

#include "kprotocol/codec.hpp"
#include "kprotocol/packet.hpp"
#include "kprotocol/text_component.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace kprotocol {

enum class ScoreboardObjectiveAction : std::int8_t {
    create = 0,
    update = 1,
    remove = 2,
};

class S3BScoreboardObjectivePacket {
public:
    std::string name;
    ScoreboardObjectiveAction action{ScoreboardObjectiveAction::create};
    std::vector<std::uint8_t> tail;

    [[nodiscard]] static S3BScoreboardObjectivePacket make_create(
        std::string objective_name,
        const std::string& display_name) {

        S3BScoreboardObjectivePacket packet{
            .name = std::move(objective_name),
            .action = ScoreboardObjectiveAction::create,
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
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"name", name},
                {"action", static_cast<std::int8_t>(action)},
                {"tail", tail},
            },
        };
    }

    static S3BScoreboardObjectivePacket from_packet(const Packet& packet) {
        return S3BScoreboardObjectivePacket{
            .name = require_field<std::string>(packet.fields, "name"),
            .action = static_cast<ScoreboardObjectiveAction>(
                require_field<std::int8_t>(packet.fields, "action")),
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
