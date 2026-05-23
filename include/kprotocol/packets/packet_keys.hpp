#pragma once

namespace kprotocol::packet_keys {

inline constexpr char handshake[] = "handshake";
inline constexpr char status_request[] = "status_request";
inline constexpr char status_response[] = "status_response";
inline constexpr char ping_request[] = "ping_request";
inline constexpr char pong_response[] = "pong_response";
inline constexpr char login_start[] = "login_start";
inline constexpr char login_success[] = "login_success";
inline constexpr char login_disconnect[] = "login_disconnect";
inline constexpr char keep_alive_serverbound[] = "keep_alive_serverbound";
inline constexpr char keep_alive_clientbound[] = "keep_alive_clientbound";

} // namespace kprotocol::packet_keys
