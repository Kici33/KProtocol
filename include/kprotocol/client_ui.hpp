#pragma once

#include "kprotocol/packets/play/S3BScoreboardObjectivePacket.hpp"
#include "kprotocol/server.hpp"
#include "kprotocol/types.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace kprotocol {

struct TitleOptions {
    std::string title;
    std::string subtitle;
    std::int32_t fade_in = 10;
    std::int32_t stay = 70;
    std::int32_t fade_out = 20;
};

struct ScoreboardLine {
    std::string entry;
    std::int32_t value{};
};

// Send title/subtitle/timings using the wire shape appropriate for the client's
// protocol version (legacy title packet or 1.17+ split packets).
bool send_title(const ClientSession& client, const TitleOptions& options);

// Clear title/subtitle (and optionally reset timings on 1.17+).
bool clear_title(const ClientSession& client, bool reset_times = false);

// Send action bar text (legacy chat position 2 or 1.17+ action_bar packet).
bool send_action_bar(const ClientSession& client, const std::string& text);

// Create, update, or remove a scoreboard objective.
bool send_scoreboard_objective(
    const ClientSession& client,
    const std::string& objective_name,
    const std::string& display_name,
    ScoreboardObjectiveAction action = ScoreboardObjectiveAction::create);

// Set or remove a score line on an objective.
bool send_scoreboard_score(
    const ClientSession& client,
    const std::string& entry_name,
    const std::string& objective_name,
    std::int32_t value);

bool send_scoreboard_score_remove(
    const ClientSession& client,
    const std::string& entry_name,
    const std::string& objective_name);

// Show a scoreboard objective in a display slot (sidebar, belowName, etc.).
bool send_scoreboard_display(
    const ClientSession& client,
    const std::string& objective_name,
    std::int8_t position);

// Convenience: create objective, set lines, then show in a display slot.
bool send_scoreboard_sidebar(
    const ClientSession& client,
    const std::string& objective_name,
    const std::string& display_name,
    std::span<const ScoreboardLine> lines,
    std::int8_t position = 1);

// Single block update at client version (block id translated from internal_version).
bool send_block_change(
    const ClientSession& client,
    ProtocolVersion internal_version,
    const Position& location,
    BlockState block);

// Same as above, resolving block_name via BlockRegistry at internal_version.
bool send_block_change(
    const ClientSession& client,
    ProtocolVersion internal_version,
    const Position& location,
    const std::string& block_name);

} // namespace kprotocol
