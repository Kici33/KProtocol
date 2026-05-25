#pragma once

#include "kprotocol/server.hpp"
#include "kprotocol/types.hpp"

#include <cstdint>
#include <string>

namespace kprotocol {

struct TitleOptions {
    std::string title;
    std::string subtitle;
    std::int32_t fade_in = 10;
    std::int32_t stay = 70;
    std::int32_t fade_out = 20;
};

// Send title/subtitle/timings using the wire shape appropriate for the client's
// protocol version (legacy title packet or 1.20.4+ split packets).
bool send_title(const ClientSession& client, const TitleOptions& options);

// Send action bar text (legacy chat position or modern action_bar / system_chat).
bool send_action_bar(const ClientSession& client, const std::string& text);

// Show a scoreboard objective in a display slot (sidebar, belowName, etc.).
bool send_scoreboard_display(
    const ClientSession& client,
    const std::string& objective_name,
    std::int8_t position);

// Single block update at client version (block id translated from internal_version).
bool send_block_change(
    const ClientSession& client,
    ProtocolVersion internal_version,
    const Position& location,
    BlockState block);

} // namespace kprotocol
