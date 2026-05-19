#pragma once

#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"

#include <cstdint>
#include <string>

namespace kprotocol {

namespace packet_keys {
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
} // namespace packet_keys

void register_baseline_packets(PacketRegistry& registry, PacketTranslator& translator);

class C00HandshakePacket {
public:
    std::int32_t protocol_version{};
    std::string server_address;
    std::uint16_t server_port{};
    std::int32_t next_state{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::handshake,
            .state = PacketState::handshaking,
            .direction = PacketDirection::serverbound,
            .fields = {
                {"protocol_version", protocol_version},
                {"server_address", server_address},
                {"server_port", server_port},
                {"next_state", next_state}
            }
        };
    }

    static C00HandshakePacket from_packet(const Packet& packet) {
        return C00HandshakePacket{
            .protocol_version = require_field<std::int32_t>(packet.fields, "protocol_version"),
            .server_address = require_field<std::string>(packet.fields, "server_address"),
            .server_port = require_field<std::uint16_t>(packet.fields, "server_port"),
            .next_state = require_field<std::int32_t>(packet.fields, "next_state")
        };
    }
};

class C00StatusRequestPacket {
public:
    Packet to_packet() const {
        return Packet{
            .key = packet_keys::status_request,
            .state = PacketState::status,
            .direction = PacketDirection::serverbound,
            .fields = {}
        };
    }

    static C00StatusRequestPacket from_packet(const Packet&) {
        return {};
    }
};

class S00StatusResponsePacket {
public:
    std::string json_response;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::status_response,
            .state = PacketState::status,
            .direction = PacketDirection::clientbound,
            .fields = {{"json_response", json_response}}
        };
    }

    static S00StatusResponsePacket from_packet(const Packet& packet) {
        return S00StatusResponsePacket{
            .json_response = require_field<std::string>(packet.fields, "json_response")
        };
    }
};

class C01PingRequestPacket {
public:
    std::int64_t payload{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::ping_request,
            .state = PacketState::status,
            .direction = PacketDirection::serverbound,
            .fields = {{"payload", payload}}
        };
    }

    static C01PingRequestPacket from_packet(const Packet& packet) {
        return C01PingRequestPacket{
            .payload = require_field<std::int64_t>(packet.fields, "payload")
        };
    }
};

class S01PongResponsePacket {
public:
    std::int64_t payload{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::pong_response,
            .state = PacketState::status,
            .direction = PacketDirection::clientbound,
            .fields = {{"payload", payload}}
        };
    }

    static S01PongResponsePacket from_packet(const Packet& packet) {
        return S01PongResponsePacket{
            .payload = require_field<std::int64_t>(packet.fields, "payload")
        };
    }
};

class C00LoginStartPacket {
public:
    std::string username;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_start,
            .state = PacketState::login,
            .direction = PacketDirection::serverbound,
            .fields = {{"username", username}}
        };
    }

    static C00LoginStartPacket from_packet(const Packet& packet) {
        return C00LoginStartPacket{
            .username = require_field<std::string>(packet.fields, "username")
        };
    }
};

class S02LoginSuccessPacket {
public:
    std::string uuid;
    std::string username;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_success,
            .state = PacketState::login,
            .direction = PacketDirection::clientbound,
            .fields = {
                {"uuid", uuid},
                {"username", username}
            }
        };
    }

    static S02LoginSuccessPacket from_packet(const Packet& packet) {
        return S02LoginSuccessPacket{
            .uuid = require_field<std::string>(packet.fields, "uuid"),
            .username = require_field<std::string>(packet.fields, "username")
        };
    }
};

class S00LoginDisconnectPacket {
public:
    std::string reason_json;

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::login_disconnect,
            .state = PacketState::login,
            .direction = PacketDirection::clientbound,
            .fields = {{"reason_json", reason_json}}
        };
    }

    static S00LoginDisconnectPacket from_packet(const Packet& packet) {
        return S00LoginDisconnectPacket{
            .reason_json = require_field<std::string>(packet.fields, "reason_json")
        };
    }
};

class C15KeepAlivePacket {
public:
    std::int64_t id{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::keep_alive_serverbound,
            .state = PacketState::play,
            .direction = PacketDirection::serverbound,
            .fields = {{"id", id}}
        };
    }

    static C15KeepAlivePacket from_packet(const Packet& packet) {
        return C15KeepAlivePacket{
            .id = require_field<std::int64_t>(packet.fields, "id")
        };
    }
};

class S24KeepAlivePacket {
public:
    std::int64_t id{};

    Packet to_packet() const {
        return Packet{
            .key = packet_keys::keep_alive_clientbound,
            .state = PacketState::play,
            .direction = PacketDirection::clientbound,
            .fields = {{"id", id}}
        };
    }

    static S24KeepAlivePacket from_packet(const Packet& packet) {
        return S24KeepAlivePacket{
            .id = require_field<std::int64_t>(packet.fields, "id")
        };
    }
};

} // namespace kprotocol
