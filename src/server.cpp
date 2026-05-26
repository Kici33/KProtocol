#include "kprotocol/server.hpp"

#include "kprotocol/codec.hpp"
#include "kprotocol/connection.hpp"
#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif
#include "kprotocol/packets/packet_keys.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"
#include "kprotocol/version.hpp"

#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/write.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace kprotocol {

struct ClientSession::Shared {
    std::shared_ptr<asio::ip::tcp::socket> socket;
    std::atomic<bool> open{true};
    std::atomic<std::int32_t> client_wire{protocol_number(ProtocolVersion::v1_21_1)};
    std::atomic<PacketState> state{PacketState::handshaking};
    ProtocolVersion internal_version{ProtocolVersion::v1_21_1};
    FrameDecoder inbound_decoder;
    std::int32_t outbound_compression_threshold{-1};
    PacketRegistry* registry{nullptr};
    PacketTranslator* translator{nullptr};
    std::string remote;
    std::mutex send_mutex;
    std::function<void(const ClientSession&, const Packet&)> sent_event;
};

ClientSession::ClientSession(std::shared_ptr<Shared> shared)
    : shared_(std::move(shared)) {}

bool ClientSession::send_packet(const Packet& packet) const {
    if (!valid()) {
        return false;
    }

    Packet translated = packet;
    translated.direction = PacketDirection::clientbound;
    translated.state = shared_->state.load();
    translated = shared_->translator->translate(
        translated, shared_->internal_version, client_protocol_version());

    return send_packet_direct(translated, client_protocol_version());
}

bool ClientSession::send_packet_direct(const Packet& packet, const ProtocolVersion encode_version) const {
    if (!valid()) {
        return false;
    }

    const auto encoded = shared_->registry->encode_packet(
        packet, encode_version, shared_->outbound_compression_threshold);

    std::lock_guard lock(shared_->send_mutex);
    asio::error_code ec;
    asio::write(*shared_->socket, asio::buffer(encoded), ec);
    if (ec) {
        return false;
    }
    if (shared_->sent_event) {
        shared_->sent_event(ClientSession(shared_), packet);
    }
    return true;
}

ProtocolVersion ClientSession::protocol_version() const {
    return client_protocol_version();
}

WireProtocol ClientSession::client_wire() const {
    return shared_ ? WireProtocol{shared_->client_wire.load()} : WireProtocol{};
}

ProtocolVersion ClientSession::client_protocol_version() const {
    if (!shared_) {
        return ProtocolVersion::v1_21_1;
    }
    const WireProtocol wire{shared_->client_wire.load()};
    return catalog_anchor_for(wire);
}

PacketState ClientSession::state() const {
    return shared_ ? shared_->state.load() : PacketState::handshaking;
}

void ClientSession::set_state(const PacketState state) const {
    if (shared_) {
        shared_->state.store(state);
    }
}

void ClientSession::enable_compression(const std::int32_t threshold) const {
    if (!shared_) {
        return;
    }
    shared_->inbound_decoder.set_threshold(threshold);
    shared_->outbound_compression_threshold = threshold;
}

std::string ClientSession::remote_address() const {
    return shared_ ? shared_->remote : "unknown";
}

bool ClientSession::valid() const noexcept {
    return static_cast<bool>(shared_) && shared_->open.load() && shared_->socket &&
           shared_->socket->is_open();
}

struct MinecraftServer::Impl {
    PacketRegistry& registry;
    PacketTranslator& translator;
    asio::io_context io;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_guard;
    asio::ip::tcp::acceptor acceptor{io};
    std::thread io_thread;
    std::atomic<bool> running{false};
    std::uint16_t listen_port_{0};
    std::int32_t default_compression_threshold_{-1};
    ProtocolVersion internal_version{ProtocolVersion::v1_21_1};
    PacketHandler packet_handler;
    ErrorHandler error_handler;
    std::mutex listeners_mutex;
    std::vector<std::shared_ptr<ProtocolListener>> listeners;
    std::mutex clients_mutex;
    std::vector<std::shared_ptr<ClientSession::Shared>> clients;

    Impl(PacketRegistry& reg, PacketTranslator& trans)
        : registry(reg), translator(trans) {}

    ~Impl() {
        stop();
    }

    void emit_error(const std::string& message) {
        if (error_handler) {
            error_handler(message);
        }
        std::lock_guard lock(listeners_mutex);
        for (const auto& listener : listeners) {
            listener->onError(message);
        }
    }

    void emit_received(const ClientSession& session, const Packet& packet) {
        std::lock_guard lock(listeners_mutex);
        for (const auto& listener : listeners) {
            listener->onPacketReceived(session, packet);
        }
    }

    void emit_sent(const ClientSession& session, const Packet& packet) {
        std::lock_guard lock(listeners_mutex);
        for (const auto& listener : listeners) {
            listener->onPacketSent(session, packet);
        }
    }

    void update_handshake_state(const Packet& packet, const std::shared_ptr<ClientSession::Shared>& shared) {
        if (!packet.key_matches(packet_keys::handshake)) {
            return;
        }

        if (const auto it = packet.fields.find("protocol_version"); it != packet.fields.end()) {
            if (const auto* version_number = std::get_if<std::int32_t>(&it->second); version_number != nullptr) {
                const WireProtocol wire{*version_number};
                if (!is_known_protocol(wire)) {
                    emit_error("Client " + shared->remote + " announced unknown protocol "
                               + name_of(wire));
                }
                shared->client_wire.store(wire.value);
            }
        }

        if (const auto it = packet.fields.find("next_state"); it != packet.fields.end()) {
            if (const auto* state_id = std::get_if<std::int32_t>(&it->second); state_id != nullptr) {
                if (*state_id == 1) {
                    shared->state = PacketState::status;
                } else if (*state_id == 2) {
                    shared->state = PacketState::login;
                } else if (*state_id == 3) {
                    shared->state = PacketState::configuration;
                }
            }
        }
    }

    void apply_set_compression(const Packet& packet, const std::shared_ptr<ClientSession::Shared>& shared) {
#ifdef KPROTOCOL_HAS_GENERATED_CATALOG
        if (!packet.key_matches(generated::packet_keys::login_clientbound_compress)) {
            return;
        }
#else
        (void)packet;
        return;
#endif
        if (const auto it = packet.fields.find("threshold"); it != packet.fields.end()) {
            if (const auto* threshold = std::get_if<std::int32_t>(&it->second); threshold != nullptr) {
                shared->inbound_decoder.set_threshold(*threshold);
                shared->outbound_compression_threshold = *threshold;
            }
        }
    }

    void update_session_state(const Packet& packet, const std::shared_ptr<ClientSession::Shared>& shared) {
        update_handshake_state(packet, shared);

#ifdef KPROTOCOL_HAS_GENERATED_CATALOG
        if (packet.key_matches(generated::packet_keys::login_serverbound_login_acknowledged)) {
            shared->state = PacketState::configuration;
            return;
        }
        if (packet.key_matches(generated::packet_keys::configuration_serverbound_finish_configuration)) {
            shared->state = PacketState::play;
        }
#endif
    }

    void process_frame(const codec::EncodedFrame& frame, const std::shared_ptr<ClientSession::Shared>& shared) {
        try {
            const auto client_ver = catalog_anchor_for(WireProtocol{shared->client_wire.load()});
            Packet packet = registry.decode_packet(
                frame, client_ver, shared->state.load(), PacketDirection::serverbound);
            packet = translator.translate(packet, client_ver, internal_version);
            apply_set_compression(packet, shared);
            update_session_state(packet, shared);
            const ClientSession session(shared);
            emit_received(session, packet);
            if (packet_handler) {
                packet_handler(session, packet);
            }
        } catch (const std::exception& ex) {
            emit_error(std::string("Packet processing error from ") + shared->remote + ": " + ex.what());
        }
    }

    asio::awaitable<void> client_session(std::shared_ptr<ClientSession::Shared> shared) {
        std::vector<std::uint8_t> inbound;
        inbound.reserve(8192);
        std::array<std::uint8_t, 4096> recv_buffer{};

        while (running.load() && shared->open.load()) {
            std::size_t bytes_read = 0;
            try {
                bytes_read = co_await shared->socket->async_read_some(
                    asio::buffer(recv_buffer), asio::use_awaitable);
            } catch (const std::exception& ex) {
                if (running.load()) {
                    emit_error(std::string("Read error from ") + shared->remote + ": " + ex.what());
                }
                break;
            }

            if (bytes_read == 0) {
                break;
            }
            inbound.insert(inbound.end(), recv_buffer.begin(), recv_buffer.begin() + bytes_read);

            while (true) {
                codec::EncodedFrame frame;
                std::size_t consumed = 0;
                if (!shared->inbound_decoder.try_decode(inbound, consumed, frame)) {
                    break;
                }
                inbound.erase(inbound.begin(), inbound.begin() + static_cast<std::ptrdiff_t>(consumed));
                process_frame(frame, shared);
            }
        }

        shared->open = false;
        if (shared->socket && shared->socket->is_open()) {
            asio::error_code ec;
            shared->socket->close(ec);
        }
    }

    asio::awaitable<void> accept_loop() {
        while (running.load()) {
            asio::ip::tcp::socket socket(io);
            try {
                co_await acceptor.async_accept(socket, asio::use_awaitable);
            } catch (const std::exception& ex) {
                if (running.load()) {
                    emit_error(std::string("Accept failed: ") + ex.what());
                }
                break;
            }

            auto shared = std::make_shared<ClientSession::Shared>();
            shared->socket = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
            shared->internal_version = internal_version;
            shared->inbound_decoder = FrameDecoder{-1};
            shared->outbound_compression_threshold = default_compression_threshold_;
            shared->registry = &registry;
            shared->translator = &translator;
            try {
                const auto endpoint = shared->socket->remote_endpoint();
                shared->remote = endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
            } catch (const std::exception&) {
                shared->remote = "unknown";
            }
            shared->sent_event = [this](const ClientSession& session, const Packet& packet) {
                emit_sent(session, packet);
            };

            {
                std::lock_guard lock(clients_mutex);
                clients.push_back(shared);
            }

            co_spawn(io, client_session(std::move(shared)), asio::detached);
        }
    }

    bool bind_and_listen(std::uint16_t port) {
        try {
            asio::error_code ec;
            const asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
            acceptor.open(endpoint.protocol(), ec);
            if (ec) {
                emit_error("Failed to open acceptor: " + ec.message());
                return false;
            }
            acceptor.set_option(asio::socket_base::reuse_address(true), ec);
            acceptor.bind(endpoint, ec);
            if (ec) {
                emit_error("Failed to bind: " + ec.message());
                return false;
            }
            acceptor.listen(asio::socket_base::max_listen_connections, ec);
            if (ec) {
                emit_error("Failed to listen: " + ec.message());
                return false;
            }
            listen_port_ = acceptor.local_endpoint(ec).port();
            return true;
        } catch (const std::exception& ex) {
            emit_error(ex.what());
            return false;
        }
    }

    void stop() {
        if (!running.exchange(false)) {
            return;
        }

        asio::error_code ec;
        acceptor.close(ec);

        {
            std::lock_guard lock(clients_mutex);
            for (const auto& client : clients) {
                client->open = false;
                if (client->socket && client->socket->is_open()) {
                    client->socket->close(ec);
                }
            }
            clients.clear();
        }

        work_guard.reset();
        io.stop();
        if (io_thread.joinable()) {
            io_thread.join();
        }
        io.restart();
    }
};

MinecraftServer::MinecraftServer(PacketRegistry& registry, PacketTranslator& translator)
    : impl_(std::make_unique<Impl>(registry, translator)) {}

MinecraftServer::~MinecraftServer() = default;

MinecraftServer::MinecraftServer(MinecraftServer&&) noexcept = default;
MinecraftServer& MinecraftServer::operator=(MinecraftServer&&) noexcept = default;

bool MinecraftServer::start(
    const std::uint16_t port,
    const ProtocolVersion internal_version,
    const std::int32_t compression_threshold) {
    if (impl_->running.load()) {
        return false;
    }
    impl_->internal_version = internal_version;
    impl_->default_compression_threshold_ = compression_threshold;
    if (!impl_->bind_and_listen(port)) {
        return false;
    }

    impl_->work_guard.emplace(asio::make_work_guard(impl_->io));
    impl_->running = true;
    co_spawn(impl_->io, impl_->accept_loop(), asio::detached);
    impl_->io_thread = std::thread([impl = impl_.get()] { impl->io.run(); });
    return true;
}

void MinecraftServer::stop() {
    impl_->stop();
}

bool MinecraftServer::running() const noexcept {
    return impl_->running.load();
}

std::uint16_t MinecraftServer::listen_port() const noexcept {
    return impl_->listen_port_;
}

void MinecraftServer::on_packet(PacketHandler handler) {
    impl_->packet_handler = std::move(handler);
}

void MinecraftServer::on_error(ErrorHandler handler) {
    impl_->error_handler = std::move(handler);
}

void MinecraftServer::add_listener(std::shared_ptr<ProtocolListener> listener) {
    if (!listener) {
        return;
    }
    std::lock_guard lock(impl_->listeners_mutex);
    impl_->listeners.push_back(std::move(listener));
}

void MinecraftServer::set_listener(std::shared_ptr<ProtocolListener> listener) {
    std::lock_guard lock(impl_->listeners_mutex);
    impl_->listeners.clear();
    if (listener) {
        impl_->listeners.push_back(std::move(listener));
    }
}

} // namespace kprotocol
