#pragma once

#include <string_view>

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

namespace kprotocol::packet_keys {

// Canonical dot-notation keys when the generated catalog is linked.
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
inline constexpr std::string_view handshake = generated::packet_keys::handshaking_serverbound_set_protocol;
inline constexpr std::string_view status_request = generated::packet_keys::status_serverbound_ping_start;
inline constexpr std::string_view status_response = generated::packet_keys::status_clientbound_server_info;
inline constexpr std::string_view ping_request = generated::packet_keys::status_serverbound_ping;
inline constexpr std::string_view pong_response = generated::packet_keys::status_clientbound_ping;
inline constexpr std::string_view login_start = generated::packet_keys::login_serverbound_login_start;
inline constexpr std::string_view login_success = generated::packet_keys::login_clientbound_success;
inline constexpr std::string_view login_disconnect = generated::packet_keys::login_clientbound_disconnect;
inline constexpr std::string_view keep_alive_serverbound = generated::packet_keys::play_serverbound_keep_alive;
inline constexpr std::string_view keep_alive_clientbound = generated::packet_keys::play_clientbound_keep_alive;
#else
inline constexpr char handshake[] = "handshaking.serverbound.set_protocol";
inline constexpr char status_request[] = "status.serverbound.ping_start";
inline constexpr char status_response[] = "status.clientbound.status_response";
inline constexpr char ping_request[] = "status.serverbound.ping";
inline constexpr char pong_response[] = "status.clientbound.ping";
inline constexpr char login_start[] = "login.serverbound.login_start";
inline constexpr char login_success[] = "login.clientbound.success";
inline constexpr char login_disconnect[] = "login.clientbound.disconnect";
inline constexpr char keep_alive_serverbound[] = "play.serverbound.keep_alive";
inline constexpr char keep_alive_clientbound[] = "play.clientbound.keep_alive";
#endif

} // namespace kprotocol::packet_keys
