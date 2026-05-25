#pragma once

#include "kprotocol/server.hpp"
#include "kprotocol/version.hpp"

#include <string>

namespace kprotocol {

// Offline-mode login success. For protocols before 1.20.2 also enters play state.
bool send_offline_login_success(
    const ClientSession& client,
    const std::string& username,
    const std::string& uuid = "00000000-0000-0000-0000-000000000003");

// Reference play-state showcase: title, action bar, scoreboard, block change, entity metadata.
// Block ids are translated from internal_version to the client's protocol version.
bool send_play_demo(const ClientSession& client, ProtocolVersion internal_version);

} // namespace kprotocol
