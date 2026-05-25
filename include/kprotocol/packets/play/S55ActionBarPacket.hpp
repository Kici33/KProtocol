#pragma once

#include "kprotocol/packet.hpp"
#include "kprotocol/text_component.hpp"
#include "kprotocol/version.hpp"

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

#include <cstdint>
#include <string>

namespace kprotocol {

class S55ActionBarPacket {
public:
    std::string text;

    [[nodiscard]] Packet to_packet(const ProtocolVersion wire_version) const {
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
        const std::string_view key = generated::packet_keys::play_clientbound_action_bar;
#else
        const std::string_view key = "play.clientbound.action_bar";
#endif
        PacketFields fields;
        if (uses_nbt_text(wire_version)) {
            fields.emplace("text", text_component_nbt(text));
        } else {
            fields.emplace("text", json_text(text));
        }
        return Packet{
            .key = std::string(key),
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = std::move(fields),
        };
    }

    static S55ActionBarPacket from_packet(const Packet& packet) {
        S55ActionBarPacket out;
        const auto it = packet.fields.find("text");
        if (it == packet.fields.end()) {
            return out;
        }
        if (const auto* s = std::get_if<std::string>(&it->second); s != nullptr) {
            out.text = *s;
        }
        return out;
    }
};

} // namespace kprotocol
