#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/codec.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace kprotocol {

// Legacy combined title packet (1.8–1.16.5): action + tail rest_buffer.
class S45TitlePacket {
public:
    std::int32_t action{};
    std::vector<std::uint8_t> tail;
    std::string text;
    std::int32_t fade_in{};
    std::int32_t stay{};
    std::int32_t fade_out{};

    [[nodiscard]] Packet to_packet() const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_title;
#else
        const std::string_view key = "play.clientbound.title";
#endif
        PacketFields fields{
            {"action", action},
            {"tail", tail},
        };
        const bool has_text = action == 0 || action == 1 || (action == 2 && tail.size() != 12U);
        const bool has_times = (action == 2 && tail.size() == 12U) || action == 3;
        if (has_text) {
            if (!text.empty()) {
                fields["text"] = text;
            } else if (!tail.empty()) {
                std::size_t offset = 0;
                fields["text"] = codec::read_string(tail, offset);
            }
        }
        if (has_times) {
            if (fade_in != 0 || stay != 0 || fade_out != 0) {
                fields["fadeIn"] = fade_in;
                fields["stay"] = stay;
                fields["fadeOut"] = fade_out;
            } else if (tail.size() == 12U) {
                std::size_t offset = 0;
                fields["fadeIn"] = codec::read_int(tail, offset);
                fields["stay"] = codec::read_int(tail, offset);
                fields["fadeOut"] = codec::read_int(tail, offset);
            }
        }

        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = std::move(fields)
        };
    }

    static S45TitlePacket from_packet(const Packet& packet) {
        return S45TitlePacket{
            .action = require_field<std::int32_t>(packet.fields, "action"),
            .tail = field_or<std::vector<std::uint8_t>>(packet.fields, "tail", {}),
            .text = field_or<std::string>(packet.fields, "text", {}),
            .fade_in = field_or<std::int32_t>(packet.fields, "fadeIn", 0),
            .stay = field_or<std::int32_t>(packet.fields, "stay", 0),
            .fade_out = field_or<std::int32_t>(packet.fields, "fadeOut", 0),
        };
    }
};

} // namespace kprotocol
