#pragma once

#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace kprotocol {

class ClientSession;

class ProtocolListener {
public:
    virtual ~ProtocolListener() = default;
    virtual void onPacketReceive(const ClientSession&, const Packet&) {}
    virtual void onPacketReceived(const ClientSession& client, const Packet& packet) {
        onPacketReceive(client, packet);
    }
    virtual void onPacketSent(const ClientSession&, const Packet&) {}
    virtual void onError(const std::string&) {}
};

class ClientSession {
public:
    ClientSession() = default;

    bool send_packet(const Packet& packet) const;
    ProtocolVersion protocol_version() const;
    PacketState state() const;
    void set_state(PacketState state) const;
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

    bool start(std::uint16_t port, ProtocolVersion internal_version = ProtocolVersion::v1_21_1);
    void stop();

    bool running() const noexcept;

    void on_packet(PacketHandler handler);
    void on_error(ErrorHandler handler);
    void add_listener(std::shared_ptr<ProtocolListener> listener);
    void set_listener(std::shared_ptr<ProtocolListener> listener);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace kprotocol
