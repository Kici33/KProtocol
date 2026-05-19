#include "kprotocol/server.hpp"

#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/codec.hpp"

#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_type = SOCKET;
constexpr socket_type invalid_socket = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_type = int;
constexpr socket_type invalid_socket = -1;
#endif

namespace kprotocol {

namespace {

void close_socket(const socket_type sock) {
    if (sock == invalid_socket) {
        return;
    }
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

std::string socket_to_string(const sockaddr_in& address) {
    std::array<char, INET_ADDRSTRLEN> buffer{};
    if (inet_ntop(AF_INET, &address.sin_addr, buffer.data(), static_cast<socklen_t>(buffer.size())) == nullptr) {
        return "unknown";
    }
    return std::string(buffer.data()) + ":" + std::to_string(ntohs(address.sin_port));
}

} // namespace

struct ClientSession::Shared {
    socket_type socket{invalid_socket};
    std::atomic<ProtocolVersion> client_version{ProtocolVersion::v1_21_1};
    std::atomic<PacketState> state{PacketState::handshaking};
    ProtocolVersion internal_version{ProtocolVersion::v1_21_1};
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
    translated = shared_->translator->translate(translated, shared_->internal_version, shared_->client_version.load());

    const auto encoded = shared_->registry->encode_packet(translated, shared_->client_version.load());
    const auto* data = reinterpret_cast<const char*>(encoded.data());
    auto remaining = static_cast<int>(encoded.size());
    std::lock_guard lock(shared_->send_mutex);
    while (remaining > 0) {
#ifdef _WIN32
        const auto sent = send(shared_->socket, data, remaining, 0);
#else
        const auto sent = static_cast<int>(send(shared_->socket, data, static_cast<std::size_t>(remaining), 0));
#endif
        if (sent <= 0) {
            return false;
        }
        remaining -= sent;
        data += sent;
    }
    if (shared_->sent_event) {
        shared_->sent_event(ClientSession(shared_), translated);
    }
    return true;
}

ProtocolVersion ClientSession::protocol_version() const {
    return shared_ ? shared_->client_version.load() : ProtocolVersion::v1_21_1;
}

PacketState ClientSession::state() const {
    return shared_ ? shared_->state.load() : PacketState::handshaking;
}

void ClientSession::set_state(const PacketState state) const {
    if (shared_) {
        shared_->state.store(state);
    }
}

std::string ClientSession::remote_address() const {
    return shared_ ? shared_->remote : "unknown";
}

bool ClientSession::valid() const noexcept {
    return static_cast<bool>(shared_) && shared_->socket != invalid_socket;
}

struct MinecraftServer::Impl {
    PacketRegistry& registry;
    PacketTranslator& translator;
    std::atomic<bool> running{false};
    socket_type listener{invalid_socket};
    std::thread accept_thread;
    std::mutex clients_mutex;
    std::vector<std::shared_ptr<ClientSession::Shared>> clients;
    PacketHandler packet_handler;
    ErrorHandler error_handler;
    std::mutex listeners_mutex;
    std::vector<std::shared_ptr<ProtocolListener>> listeners;
    ProtocolVersion internal_version{ProtocolVersion::v1_21_1};

#ifdef _WIN32
    bool winsock_ready{false};
#endif

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

    bool setup_socket(std::uint16_t port) {
#ifdef _WIN32
        WSADATA wsa_data{};
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            emit_error("WSAStartup failed");
            return false;
        }
        winsock_ready = true;
#endif

        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == invalid_socket) {
            emit_error("Failed to create socket");
            return false;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port);
        server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        int enable = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&enable), sizeof(enable));

        if (bind(listener, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) != 0) {
            emit_error("Failed to bind socket");
            close_socket(listener);
            listener = invalid_socket;
            return false;
        }

        if (listen(listener, SOMAXCONN) != 0) {
            emit_error("Failed to listen");
            close_socket(listener);
            listener = invalid_socket;
            return false;
        }
        return true;
    }

    void stop() {
        if (!running.exchange(false)) {
            return;
        }

        close_socket(listener);
        listener = invalid_socket;

        if (accept_thread.joinable()) {
            accept_thread.join();
        }

        std::lock_guard lock(clients_mutex);
        for (const auto& client : clients) {
            close_socket(client->socket);
            client->socket = invalid_socket;
        }
        clients.clear();

#ifdef _WIN32
        if (winsock_ready) {
            WSACleanup();
            winsock_ready = false;
        }
#endif
    }

    void run_accept_loop() {
        while (running.load()) {
            sockaddr_in client_addr{};
#ifdef _WIN32
            int addr_size = sizeof(client_addr);
            const auto client_socket = accept(listener, reinterpret_cast<sockaddr*>(&client_addr), &addr_size);
#else
            socklen_t addr_size = sizeof(client_addr);
            const auto client_socket = accept(listener, reinterpret_cast<sockaddr*>(&client_addr), &addr_size);
#endif
            if (client_socket == invalid_socket) {
                if (running.load()) {
                    emit_error("Accept failed");
                }
                break;
            }

            auto shared = std::make_shared<ClientSession::Shared>();
            shared->socket = client_socket;
            shared->internal_version = internal_version;
            shared->registry = &registry;
            shared->translator = &translator;
            shared->remote = socket_to_string(client_addr);
            shared->sent_event = [this](const ClientSession& session, const Packet& packet) {
                emit_sent(session, packet);
            };

            {
                std::lock_guard lock(clients_mutex);
                clients.push_back(shared);
            }

            std::thread([this, shared] { handle_client(shared); }).detach();
        }
    }

    void handle_client(const std::shared_ptr<ClientSession::Shared>& shared) {
        std::vector<std::uint8_t> inbound;
        inbound.reserve(8192);
        std::array<std::uint8_t, 4096> recv_buffer{};

        while (running.load()) {
#ifdef _WIN32
            const auto read = recv(shared->socket, reinterpret_cast<char*>(recv_buffer.data()), static_cast<int>(recv_buffer.size()), 0);
#else
            const auto read = recv(shared->socket, recv_buffer.data(), recv_buffer.size(), 0);
#endif
            if (read <= 0) {
                break;
            }
            inbound.insert(inbound.end(), recv_buffer.begin(), recv_buffer.begin() + read);

            while (true) {
                codec::EncodedFrame frame;
                std::size_t consumed = 0;
                if (!codec::try_decode_frame(inbound, consumed, frame)) {
                    break;
                }

                inbound.erase(inbound.begin(), inbound.begin() + static_cast<std::ptrdiff_t>(consumed));

                try {
                    Packet packet = registry.decode_packet(frame, shared->client_version.load(), shared->state.load(), PacketDirection::serverbound);
                    packet = translator.translate(packet, shared->client_version.load(), internal_version);
                    update_handshake_state(packet, shared);
                    const ClientSession session(shared);
                    emit_received(session, packet);
                    if (packet_handler) {
                        packet_handler(session, packet);
                    }
                } catch (const std::exception& ex) {
                    emit_error(std::string("Packet processing error from ") + shared->remote + ": " + ex.what());
                }
            }
        }

        close_socket(shared->socket);
        shared->socket = invalid_socket;
    }

    void update_handshake_state(const Packet& packet, const std::shared_ptr<ClientSession::Shared>& shared) const {
        if (packet.key != packet_keys::handshake) {
            return;
        }

        if (const auto it = packet.fields.find("protocol_version"); it != packet.fields.end()) {
            if (const auto* version_number = std::get_if<std::int32_t>(&it->second); version_number != nullptr) {
                shared->client_version = static_cast<ProtocolVersion>(*version_number);
            }
        }

        if (const auto it = packet.fields.find("next_state"); it != packet.fields.end()) {
            if (const auto* state_id = std::get_if<std::int32_t>(&it->second); state_id != nullptr) {
                if (*state_id == 1) {
                    shared->state = PacketState::status;
                } else if (*state_id == 2) {
                    shared->state = PacketState::login;
                }
            }
        }
    }
};

MinecraftServer::MinecraftServer(PacketRegistry& registry, PacketTranslator& translator)
    : impl_(std::make_unique<Impl>(registry, translator)) {}

MinecraftServer::~MinecraftServer() = default;

MinecraftServer::MinecraftServer(MinecraftServer&&) noexcept = default;
MinecraftServer& MinecraftServer::operator=(MinecraftServer&&) noexcept = default;

bool MinecraftServer::start(const std::uint16_t port, const ProtocolVersion internal_version) {
    if (impl_->running.load()) {
        return false;
    }
    impl_->internal_version = internal_version;
    if (!impl_->setup_socket(port)) {
        return false;
    }
    impl_->running = true;
    impl_->accept_thread = std::thread([impl = impl_.get()] { impl->run_accept_loop(); });
    return true;
}

void MinecraftServer::stop() {
    impl_->stop();
}

bool MinecraftServer::running() const noexcept {
    return impl_->running.load();
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
