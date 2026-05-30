#include "kprotocol/server.hpp"

#include "kprotocol/codec.hpp"
#include "kprotocol/connection.hpp"
#include "kprotocol/gameplay_session.hpp"
#include "kprotocol/login_security.hpp"
#include "kprotocol/packets/packet_keys.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"
#include "kprotocol/version.hpp"

#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/redirect_error.hpp>
#include <asio/steady_timer.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/write.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace kprotocol {

namespace {

std::uint64_t monotonic_ms() noexcept {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

} // namespace

struct ClientSession::Shared {
    std::shared_ptr<asio::ip::tcp::socket> socket;
    std::atomic<bool> open{true};
    std::atomic<std::int32_t> client_wire{protocol_number(ProtocolVersion::v1_21_1)};
    std::atomic<PacketState> state{PacketState::handshaking};
    ProtocolVersion internal_version{ProtocolVersion::v1_21_1};
    FrameDecoder inbound_decoder;
    std::int32_t outbound_compression_threshold{-1};
    bool require_explicit_translations{false};
    PacketRegistry* registry{nullptr};
    PacketTranslator* translator{nullptr};
    std::string remote;
    std::atomic<std::uint64_t> connected_ms{monotonic_ms()};
    std::atomic<std::uint64_t> last_activity_ms{monotonic_ms()};
    std::atomic<std::uint64_t> bytes_received{0};
    std::atomic<std::uint64_t> bytes_sent{0};
    std::atomic<std::uint64_t> packets_received{0};
    std::atomic<std::uint64_t> packets_sent{0};
    std::uint64_t rate_window_ms{monotonic_ms()};
    std::uint32_t packets_in_window{0};
    std::uint32_t bytes_in_window{0};
    mutable std::mutex gameplay_mutex;
    GameplaySession gameplay;
    std::mutex send_mutex;
    std::function<void(const ClientSession&, const Packet&)> sent_event;
    std::function<void(const ClientSession&, const std::string&)> disconnect_event;
};

ClientSession::ClientSession(std::shared_ptr<Shared> shared)
    : shared_(std::move(shared)) {}

bool ClientSession::send_packet(const Packet& packet) const {
    if (!valid()) {
        return false;
    }

    try {
        Packet translated = packet;
        translated.direction = PacketDirection::clientbound;
        translated.state = shared_->state.load();
        translated = shared_->translator->translate(
            translated,
            shared_->internal_version,
            client_protocol_version(),
            shared_->require_explicit_translations);

        return send_packet_direct(translated, client_protocol_version());
    } catch (const std::exception&) {
        close("send translation failed");
        return false;
    }
}

bool ClientSession::send_packet_direct(const Packet& packet, const ProtocolVersion encode_version) const {
    if (!valid()) {
        return false;
    }

    std::vector<std::uint8_t> encoded;
    try {
        encoded = shared_->registry->encode_packet(
            packet, encode_version, shared_->outbound_compression_threshold);
    } catch (const std::exception&) {
        close("packet encode failed");
        return false;
    }

    std::lock_guard lock(shared_->send_mutex);
    asio::error_code ec;
    asio::write(*shared_->socket, asio::buffer(encoded), ec);
    if (ec) {
        close("socket write failed: " + ec.message());
        return false;
    }
    if (shared_->sent_event) {
        shared_->sent_event(ClientSession(shared_), packet);
    }
    shared_->bytes_sent.fetch_add(static_cast<std::uint64_t>(encoded.size()), std::memory_order_relaxed);
    shared_->packets_sent.fetch_add(1, std::memory_order_relaxed);
    return true;
}

ProtocolVersion ClientSession::protocol_version() const {
    return client_protocol_version();
}

KnownVersion ClientSession::catalog_known_version() const {
    return catalog_anchor_known_for(client_wire());
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

void ClientSession::close(const std::string& reason) const {
    if (!shared_) {
        return;
    }
    const bool was_open = shared_->open.exchange(false);
    asio::error_code ec;
    if (shared_->socket && shared_->socket->is_open()) {
        shared_->socket->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        shared_->socket->close(ec);
    }
    if (was_open && shared_->disconnect_event) {
        shared_->disconnect_event(ClientSession(shared_), reason);
    }
}

std::string ClientSession::remote_address() const {
    return shared_ ? shared_->remote : "unknown";
}

GameplaySession ClientSession::gameplay_session() const {
    if (!shared_) {
        return {};
    }
    std::lock_guard lock(shared_->gameplay_mutex);
    return shared_->gameplay;
}

void ClientSession::set_player_profile(PlayerProfile profile) const {
    if (!shared_) {
        return;
    }
    std::lock_guard lock(shared_->gameplay_mutex);
    shared_->gameplay.profile = std::move(profile);
    shared_->gameplay.login_started = !shared_->gameplay.profile.username.empty();
}

void ClientSession::set_entity_id(const std::int32_t entity_id) const {
    if (!shared_) {
        return;
    }
    std::lock_guard lock(shared_->gameplay_mutex);
    shared_->gameplay.entity_id = entity_id;
}

void ClientSession::set_game_mode(const GameMode mode) const {
    if (!shared_) {
        return;
    }
    std::lock_guard lock(shared_->gameplay_mutex);
    shared_->gameplay.game_mode = mode;
}

void ClientSession::set_dimension(std::string dimension) const {
    if (!shared_) {
        return;
    }
    std::lock_guard lock(shared_->gameplay_mutex);
    shared_->gameplay.dimension = std::move(dimension);
}

void ClientSession::set_location(const PlayerLocation location) const {
    if (!shared_) {
        return;
    }
    std::lock_guard lock(shared_->gameplay_mutex);
    shared_->gameplay.location = location;
}

void ClientSession::mark_login_complete() const {
    if (!shared_) {
        return;
    }
    std::lock_guard lock(shared_->gameplay_mutex);
    shared_->gameplay.login_complete = true;
}

void ClientSession::mark_configuration_complete() const {
    if (!shared_) {
        return;
    }
    std::lock_guard lock(shared_->gameplay_mutex);
    shared_->gameplay.configuration_complete = true;
}

void ClientSession::mark_joined_game() const {
    if (!shared_) {
        return;
    }
    std::lock_guard lock(shared_->gameplay_mutex);
    shared_->gameplay.joined_game = true;
    shared_->gameplay.login_complete = true;
}

std::uint64_t ClientSession::bytes_received() const noexcept {
    return shared_ ? shared_->bytes_received.load(std::memory_order_relaxed) : 0;
}

std::uint64_t ClientSession::bytes_sent() const noexcept {
    return shared_ ? shared_->bytes_sent.load(std::memory_order_relaxed) : 0;
}

std::uint64_t ClientSession::packets_received() const noexcept {
    return shared_ ? shared_->packets_received.load(std::memory_order_relaxed) : 0;
}

std::uint64_t ClientSession::packets_sent() const noexcept {
    return shared_ ? shared_->packets_sent.load(std::memory_order_relaxed) : 0;
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
    ServerRuntimeOptions options;
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

    void emit_disconnect(const ClientSession& session, const std::string& reason) {
        std::lock_guard lock(listeners_mutex);
        for (const auto& listener : listeners) {
            listener->onDisconnect(session, reason);
        }
    }

    void prune_clients() {
        std::lock_guard lock(clients_mutex);
        clients.erase(
            std::remove_if(clients.begin(), clients.end(), [](const auto& client) {
                return !client || !client->open.load();
            }),
            clients.end());
    }

    std::size_t active_connections() {
        prune_clients();
        std::lock_guard lock(clients_mutex);
        return clients.size();
    }

    void update_handshake_state(const Packet& packet, const std::shared_ptr<ClientSession::Shared>& shared) {
        if (!packet.key_matches(packet_keys::handshake)) {
            return;
        }

        auto it = packet.fields.find("protocol_version");
        if (it == packet.fields.end()) {
            it = packet.fields.find("protocolVersion");
        }
        if (it != packet.fields.end()) {
            if (const auto* version_number = std::get_if<std::int32_t>(&it->second); version_number != nullptr) {
                const WireProtocol wire{*version_number};
                if (!is_known_protocol(wire)) {
                    emit_error("Client " + shared->remote + " announced unknown protocol "
                               + name_of(wire));
                }
                shared->client_wire.store(wire.value);
            }
        }

        it = packet.fields.find("next_state");
        if (it == packet.fields.end()) {
            it = packet.fields.find("nextState");
        }
        if (it != packet.fields.end()) {
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
        if (!packet.key_matches(packet_keys::login_set_compression)) {
            return;
        }
        if (const auto it = packet.fields.find("threshold"); it != packet.fields.end()) {
            if (const auto* threshold = std::get_if<std::int32_t>(&it->second); threshold != nullptr) {
                shared->inbound_decoder.set_threshold(*threshold);
                shared->outbound_compression_threshold = *threshold;
            }
        }
    }

    void update_session_state(const Packet& packet, const std::shared_ptr<ClientSession::Shared>& shared) {
        update_handshake_state(packet, shared);

        if (packet.key_matches(packet_keys::login_acknowledged)) {
            shared->state = PacketState::configuration;
            ClientSession(shared).mark_login_complete();
            return;
        }
        if (packet.key_matches(packet_keys::configuration_finish_serverbound)) {
            shared->state = PacketState::play;
            ClientSession(shared).mark_configuration_complete();
        }
    }

    void update_gameplay_session(const Packet& packet, const std::shared_ptr<ClientSession::Shared>& shared) {
        const ClientSession session(shared);
        if (packet.key_matches(packet_keys::login_start)) {
            const auto it = packet.fields.find("username");
            if (it != packet.fields.end()) {
                if (const auto* username = std::get_if<std::string>(&it->second); username != nullptr) {
                    auto gameplay = session.gameplay_session();
                    gameplay.profile.username = *username;
                    gameplay.login_started = true;
                    session.set_player_profile(std::move(gameplay.profile));
                }
            }
        }

        bool has_location_update = false;
        auto location = session.gameplay_session().location;
        if (const auto it = packet.fields.find("x"); it != packet.fields.end()) {
            if (const auto* value = std::get_if<double>(&it->second); value != nullptr) {
                location.x = *value;
                has_location_update = true;
            }
        }
        if (const auto it = packet.fields.find("y"); it != packet.fields.end()) {
            if (const auto* value = std::get_if<double>(&it->second); value != nullptr) {
                location.y = *value;
                has_location_update = true;
            }
        }
        if (const auto it = packet.fields.find("z"); it != packet.fields.end()) {
            if (const auto* value = std::get_if<double>(&it->second); value != nullptr) {
                location.z = *value;
                has_location_update = true;
            }
        }
        if (const auto it = packet.fields.find("yaw"); it != packet.fields.end()) {
            if (const auto* value = std::get_if<float>(&it->second); value != nullptr) {
                location.yaw = *value;
                has_location_update = true;
            }
        }
        if (const auto it = packet.fields.find("pitch"); it != packet.fields.end()) {
            if (const auto* value = std::get_if<float>(&it->second); value != nullptr) {
                location.pitch = *value;
                has_location_update = true;
            }
        }
        auto ground_it = packet.fields.find("onGround");
        if (ground_it == packet.fields.end()) {
            ground_it = packet.fields.find("on_ground");
        }
        if (ground_it != packet.fields.end()) {
            if (const auto* value = std::get_if<bool>(&ground_it->second); value != nullptr) {
                location.on_ground = *value;
                has_location_update = true;
            }
        }
        if (has_location_update) {
            location.known = true;
            session.set_location(location);
        }
    }

    bool validate_login_start(const Packet& packet, const std::shared_ptr<ClientSession::Shared>& shared) {
        if (!options.validate_login_usernames || !packet.key_matches(packet_keys::login_start)) {
            return true;
        }

        const auto it = packet.fields.find("username");
        if (it == packet.fields.end()) {
            emit_error("Closing " + shared->remote + ": login_start missing username");
            ClientSession(shared).close("invalid login username");
            return false;
        }
        const auto* username = std::get_if<std::string>(&it->second);
        if (username == nullptr || !is_valid_login_username(*username)) {
            emit_error("Closing " + shared->remote + ": invalid login username");
            ClientSession(shared).close("invalid login username");
            return false;
        }
        return true;
    }

    bool enforce_rate_limits(
        const std::shared_ptr<ClientSession::Shared>& shared,
        const std::uint32_t packets,
        const std::uint32_t bytes) {

        if (options.max_packets_per_second == 0 && options.max_bytes_per_second == 0) {
            return true;
        }

        const auto now = monotonic_ms();
        if (now - shared->rate_window_ms >= 1000U) {
            shared->rate_window_ms = now;
            shared->packets_in_window = 0;
            shared->bytes_in_window = 0;
        }
        shared->packets_in_window += packets;
        shared->bytes_in_window += bytes;

        if (options.max_packets_per_second != 0 &&
            shared->packets_in_window > options.max_packets_per_second) {
            emit_error("Closing " + shared->remote + ": inbound packet rate exceeded");
            ClientSession(shared).close("inbound packet rate exceeded");
            return false;
        }
        if (options.max_bytes_per_second != 0 &&
            shared->bytes_in_window > options.max_bytes_per_second) {
            emit_error("Closing " + shared->remote + ": inbound byte rate exceeded");
            ClientSession(shared).close("inbound byte rate exceeded");
            return false;
        }
        return true;
    }

    bool process_frame(const codec::EncodedFrame& frame, const std::shared_ptr<ClientSession::Shared>& shared) {
        try {
            shared->last_activity_ms.store(monotonic_ms(), std::memory_order_relaxed);
            shared->packets_received.fetch_add(1, std::memory_order_relaxed);
            if (!enforce_rate_limits(shared, 1, 0)) {
                return false;
            }
            const auto client_ver = catalog_anchor_for(WireProtocol{shared->client_wire.load()});
            Packet packet = registry.decode_packet(
                frame, client_ver, shared->state.load(), PacketDirection::serverbound);
            packet = translator.translate(
                packet,
                client_ver,
                internal_version,
                shared->require_explicit_translations);
            apply_set_compression(packet, shared);
            update_session_state(packet, shared);
            update_gameplay_session(packet, shared);
            if (!validate_login_start(packet, shared)) {
                return false;
            }
            const ClientSession session(shared);
            emit_received(session, packet);
            if (packet_handler) {
                packet_handler(session, packet);
            }
            return true;
        } catch (const std::exception& ex) {
            emit_error(std::string("Packet processing error from ") + shared->remote + ": " + ex.what());
            if (options.disconnect_on_packet_error) {
                ClientSession(shared).close("packet processing error");
                return false;
            }
            return true;
        }
    }

    asio::awaitable<void> session_watchdog(std::shared_ptr<ClientSession::Shared> shared) {
        asio::steady_timer timer(io);
        while (running.load() && shared->open.load()) {
            timer.expires_after(std::chrono::milliseconds(250));
            asio::error_code ec;
            co_await timer.async_wait(asio::redirect_error(asio::use_awaitable, ec));
            if (ec || !running.load() || !shared->open.load()) {
                break;
            }

            const auto now = monotonic_ms();
            if (options.handshake_timeout_ms != 0 &&
                shared->state.load() == PacketState::handshaking &&
                now - shared->connected_ms.load(std::memory_order_relaxed) > options.handshake_timeout_ms) {
                emit_error("Closing " + shared->remote + ": handshake timeout");
                ClientSession(shared).close("handshake timeout");
                break;
            }
            if (options.idle_timeout_ms != 0 &&
                now - shared->last_activity_ms.load(std::memory_order_relaxed) > options.idle_timeout_ms) {
                emit_error("Closing " + shared->remote + ": idle timeout");
                ClientSession(shared).close("idle timeout");
                break;
            }
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
            shared->last_activity_ms.store(monotonic_ms(), std::memory_order_relaxed);
            shared->bytes_received.fetch_add(static_cast<std::uint64_t>(bytes_read), std::memory_order_relaxed);
            if (!enforce_rate_limits(shared, 0, static_cast<std::uint32_t>(bytes_read))) {
                break;
            }
            inbound.insert(inbound.end(), recv_buffer.begin(), recv_buffer.begin() + bytes_read);
            if (inbound.size() > options.max_inbound_buffer) {
                emit_error("Closing " + shared->remote + ": inbound frame buffer exceeded limit");
                ClientSession(shared).close("inbound frame buffer exceeded limit");
                break;
            }

            while (true) {
                codec::EncodedFrame frame;
                std::size_t consumed = 0;
                if (!shared->inbound_decoder.try_decode(inbound, consumed, frame)) {
                    break;
                }
                inbound.erase(inbound.begin(), inbound.begin() + static_cast<std::ptrdiff_t>(consumed));
                if (!process_frame(frame, shared)) {
                    break;
                }
            }
        }

        ClientSession(shared).close("connection closed");
        prune_clients();
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

            prune_clients();
            bool reject_connection = false;
            {
                std::lock_guard lock(clients_mutex);
                if (clients.size() >= options.max_connections) {
                    reject_connection = true;
                }
            }
            if (reject_connection) {
                asio::error_code close_ec;
                socket.shutdown(asio::ip::tcp::socket::shutdown_both, close_ec);
                socket.close(close_ec);
                emit_error("Rejected connection: max_connections reached");
                continue;
            }

            auto shared = std::make_shared<ClientSession::Shared>();
            shared->socket = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
            shared->internal_version = internal_version;
            shared->inbound_decoder = FrameDecoder{-1};
            shared->outbound_compression_threshold = default_compression_threshold_;
            shared->require_explicit_translations = options.require_explicit_translations;
            const auto now = monotonic_ms();
            shared->connected_ms.store(now, std::memory_order_relaxed);
            shared->last_activity_ms.store(now, std::memory_order_relaxed);
            shared->rate_window_ms = now;
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
            shared->disconnect_event = [this](const ClientSession& session, const std::string& reason) {
                emit_disconnect(session, reason);
            };

            {
                std::lock_guard lock(clients_mutex);
                clients.push_back(shared);
            }

            co_spawn(io, session_watchdog(shared), asio::detached);
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

        std::vector<std::shared_ptr<ClientSession::Shared>> closing;
        {
            std::lock_guard lock(clients_mutex);
            closing = std::move(clients);
            clients.clear();
        }
        for (const auto& client : closing) {
            ClientSession(client).close("server stopped");
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

std::size_t MinecraftServer::active_connections() const {
    return impl_->active_connections();
}

bool MinecraftServer::set_runtime_options(ServerRuntimeOptions options) {
    if (impl_->running.load()) {
        return false;
    }
    if (options.max_connections == 0) {
        options.max_connections = 1;
    }
    const auto min_buffer = static_cast<std::size_t>(codec::limits::max_var_int_bytes);
    if (options.max_inbound_buffer < min_buffer) {
        options.max_inbound_buffer = min_buffer;
    }
    impl_->options = options;
    return true;
}

ServerRuntimeOptions MinecraftServer::runtime_options() const {
    return impl_->options;
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
