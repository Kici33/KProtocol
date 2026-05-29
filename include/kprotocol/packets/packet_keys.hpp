#pragma once

#include <string>
#include <string_view>

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

namespace kprotocol::packet_keys {

// Canonical dot-notation keys when the generated catalog is linked.
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
inline const std::string handshake{generated::packet_keys::handshaking_serverbound_set_protocol};
inline const std::string status_request{generated::packet_keys::status_serverbound_ping_start};
inline const std::string status_response{generated::packet_keys::status_clientbound_server_info};
inline const std::string ping_request{generated::packet_keys::status_serverbound_ping};
inline const std::string pong_response{generated::packet_keys::status_clientbound_ping};
inline const std::string login_start{generated::packet_keys::login_serverbound_login_start};
inline const std::string login_success{generated::packet_keys::login_clientbound_success};
inline const std::string login_disconnect{generated::packet_keys::login_clientbound_disconnect};
inline const std::string login_encryption_request{generated::packet_keys::login_clientbound_encryption_begin};
inline const std::string login_encryption_response{generated::packet_keys::login_serverbound_encryption_begin};
inline const std::string login_set_compression{generated::packet_keys::login_clientbound_compress};
inline const std::string login_acknowledged{generated::packet_keys::login_serverbound_login_acknowledged};
inline const std::string configuration_finish_clientbound{generated::packet_keys::configuration_clientbound_finish_configuration};
inline const std::string configuration_finish_serverbound{generated::packet_keys::configuration_serverbound_finish_configuration};
inline const std::string configuration_feature_flags{generated::packet_keys::configuration_clientbound_feature_flags};
inline const std::string configuration_registry_data{generated::packet_keys::configuration_clientbound_registry_data};
inline const std::string keep_alive_serverbound{generated::packet_keys::play_serverbound_keep_alive};
inline const std::string keep_alive_clientbound{generated::packet_keys::play_clientbound_keep_alive};
#else
inline constexpr char handshake[] = "handshaking.serverbound.set_protocol";
inline constexpr char status_request[] = "status.serverbound.ping_start";
inline constexpr char status_response[] = "status.clientbound.status_response";
inline constexpr char ping_request[] = "status.serverbound.ping";
inline constexpr char pong_response[] = "status.clientbound.ping";
inline constexpr char login_start[] = "login.serverbound.login_start";
inline constexpr char login_success[] = "login.clientbound.success";
inline constexpr char login_disconnect[] = "login.clientbound.disconnect";
inline constexpr char login_encryption_request[] = "login.clientbound.encryption_begin";
inline constexpr char login_encryption_response[] = "login.serverbound.encryption_begin";
inline constexpr char login_set_compression[] = "login.clientbound.compress";
inline constexpr char login_acknowledged[] = "login.serverbound.login_acknowledged";
inline constexpr char configuration_finish_clientbound[] = "configuration.clientbound.finish_configuration";
inline constexpr char configuration_finish_serverbound[] = "configuration.serverbound.finish_configuration";
inline constexpr char configuration_feature_flags[] = "configuration.clientbound.feature_flags";
inline constexpr char configuration_registry_data[] = "configuration.clientbound.registry_data";
inline constexpr char keep_alive_serverbound[] = "play.serverbound.keep_alive";
inline constexpr char keep_alive_clientbound[] = "play.clientbound.keep_alive";
#endif

} // namespace kprotocol::packet_keys
