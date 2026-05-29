#pragma once

#include "kprotocol/codec/limits.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

namespace kprotocol {

class ClientSession;

struct ServerRuntimeOptions {
    // Maximum simultaneously open sessions. Extra accepted sockets are closed.
    std::size_t max_connections{1024};
    // Maximum bytes retained while waiting for a full frame. Prevents peers
    // from forcing unbounded growth with malformed or never-completed frames.
    std::size_t max_inbound_buffer{
        static_cast<std::size_t>(codec::limits::max_packet_length) + codec::limits::max_var_int_bytes
    };
    // Close the peer after a packet decode/handler/translation error.
    bool disconnect_on_packet_error{true};
    // Reject login_start usernames that do not match the Java Edition limits
    // used by offline and online-mode login names.
    bool validate_login_usernames{true};
    // Enable PacketTranslator strict mode while this server is running.
    bool require_explicit_translations{false};
};

class ProtocolListener {
public:
    virtual ~ProtocolListener() = default;
    virtual void onPacketReceive(const ClientSession&, const Packet&) {}
    virtual void onPacketReceived(const ClientSession& client, const Packet& packet) {
        onPacketReceive(client, packet);
    }
    virtual void onPacketSent(const ClientSession&, const Packet&) {}
    virtual void onDisconnect(const ClientSession&, const std::string&) {}
    virtual void onError(const std::string&) {}
};

class ClientSession {
public:
    ClientSession() = default;

    bool send_packet(const Packet& packet) const;
    // Encode and send without internal→client translation (packet fields must
    // match encode_version schema).
    bool send_packet_direct(const Packet& packet, ProtocolVersion encode_version) const;
    // Raw handshake protocol_version (may be unknown / future wire).
    WireProtocol client_wire() const;
    // Catalog anchor used for registry encode/decode (largest known wire <= client_wire).
    ProtocolVersion protocol_version() const;
    [[nodiscard]] KnownVersion catalog_known_version() const;

private:
    ProtocolVersion client_protocol_version() const;

public:
    PacketState state() const;
    void set_state(PacketState state) const;
    // Call after sending login.clientbound.compress (or equivalent) so inbound
    // frames use the compressed decoder matching outbound encoding.
    void enable_compression(std::int32_t threshold) const;
    void close(const std::string& reason = "closed") const;
    std::string remote_address() const;
    bool valid() const noexcept;

private:
    struct Shared;
    explicit ClientSession(std::shared_ptr<Shared> shared);

    std::shared_ptr<Shared> shared_;
    friend class MinecraftServer;
};

class MinecraftServer {
public:
    using PacketHandler = std::function<void(const ClientSession&, const Packet&)>;
    using ErrorHandler = std::function<void(const std::string&)>;

    MinecraftServer(PacketRegistry& registry, PacketTranslator& translator);
    ~MinecraftServer();

    MinecraftServer(const MinecraftServer&) = delete;
    MinecraftServer& operator=(const MinecraftServer&) = delete;
    MinecraftServer(MinecraftServer&&) noexcept;
    MinecraftServer& operator=(MinecraftServer&&) noexcept;

    // compression_threshold: Minecraft "Set Compression" threshold in bytes.
    // Pass -1 (default) to disable compression on the wire.
    bool start(std::uint16_t port,
               ProtocolVersion internal_version = ProtocolVersion::v1_21_1,
               std::int32_t compression_threshold = -1);
    void stop();

    bool running() const noexcept;
    // Bound port after start(); 0 if not started.
    std::uint16_t listen_port() const noexcept;
    [[nodiscard]] std::size_t active_connections() const;
    // Runtime options must be set before start(); returns false while running.
    bool set_runtime_options(ServerRuntimeOptions options);
    [[nodiscard]] ServerRuntimeOptions runtime_options() const;

    void on_packet(PacketHandler handler);
    void on_error(ErrorHandler handler);
    void add_listener(std::shared_ptr<ProtocolListener> listener);
    void set_listener(std::shared_ptr<ProtocolListener> listener);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace kprotocol
