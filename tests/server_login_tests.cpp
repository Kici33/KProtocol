// Integration test: login -> configuration state machine (1.20.2+ flow).

#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/packets/login/CLoginAcknowledgedPacket.hpp"
#include "kprotocol/packets/packet_keys.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/server.hpp"
#include "kprotocol/translation.hpp"

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            return 1;                                                          \
        }                                                                      \
    } while (false)

namespace {

void write_packet(asio::ip::tcp::socket& socket,
                  kprotocol::PacketRegistry& registry,
                  const kprotocol::Packet& packet,
                  kprotocol::ProtocolVersion version) {
    asio::error_code ec;
    const auto bytes = registry.encode_packet(packet, version);
    asio::write(socket, asio::buffer(bytes), ec);
    if (ec) {
        throw std::runtime_error("client write failed: " + ec.message());
    }
}

} // namespace

int main() {
    std::cout << "server_login_tests:\n" << std::flush;
    try {

    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::register_baseline_packets(registry, translator);
    registry.register_schema(kprotocol::PacketSchema{
        .key = std::string(kprotocol::packet_keys::login_acknowledged),
        .state = kprotocol::PacketState::login,
        .direction = kprotocol::PacketDirection::serverbound,
        .field_sets = {{kprotocol::KnownVersion::v1_20_2, {}}},
        .ids = {{kprotocol::KnownVersion::v1_20_4, 3}, {kprotocol::KnownVersion::v1_21_1, 3}},
    });

    std::atomic<bool> saw_login_start{false};
    std::atomic<bool> saw_login_ack{false};
    std::mutex state_mutex;
    kprotocol::PacketState observed_state{kprotocol::PacketState::handshaking};

    kprotocol::MinecraftServer server(registry, translator);
    server.on_packet([&](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
        if (packet.key == kprotocol::packet_keys::login_start) {
            saw_login_start = true;
            const auto response = kprotocol::S02LoginSuccessPacket{
                .uuid = "00000000-0000-0000-0000-000000000001",
                .username = "TestPlayer",
            };
            (void)client.send_packet(response.to_packet());
        }
        if (packet.key_matches(kprotocol::packet_keys::login_acknowledged)) {
            saw_login_ack = true;
        }
        {
            std::lock_guard lock(state_mutex);
            observed_state = client.state();
        }
    });

    KPC_CHECK(server.start(0, kprotocol::ProtocolVersion::v1_21_1), "server.start");
    const auto port = server.listen_port();

    asio::io_context client_io;
    asio::ip::tcp::socket client_socket(client_io);
    asio::error_code ec;
    asio::connect(
        client_socket,
        std::array{asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)},
        ec);
    KPC_CHECK(!ec, "client connect");

    kprotocol::C00HandshakePacket handshake;
    handshake.protocol_version = 767;
    handshake.server_address = "localhost";
    handshake.server_port = 25565;
    handshake.next_state = 2;
    write_packet(client_socket, registry, handshake.to_packet(), kprotocol::ProtocolVersion::v1_21_1);

    write_packet(client_socket, registry,
                 kprotocol::C00LoginStartPacket{.username = "TestPlayer"}.to_packet(),
                 kprotocol::ProtocolVersion::v1_21_1);

    // Wait for login_success from server.
    std::vector<std::uint8_t> inbound;
    inbound.reserve(4096);
    std::array<std::uint8_t, 1024> buf{};
    bool got_login_success = false;
    for (int attempt = 0; attempt < 100 && !got_login_success; ++attempt) {
        client_socket.non_blocking(true);
        const std::size_t n = client_socket.read_some(asio::buffer(buf), ec);
        if (ec == asio::error::would_block) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        KPC_CHECK(!ec || ec == asio::error::eof, "client read");
        if (n == 0) {
            break;
        }
        inbound.insert(inbound.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(n));

        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        if (kprotocol::codec::try_decode_frame(inbound, consumed, frame)) {
            inbound.erase(inbound.begin(), inbound.begin() + static_cast<std::ptrdiff_t>(consumed));
            const auto decoded = registry.decode_packet(
                frame, kprotocol::ProtocolVersion::v1_21_1,
                kprotocol::PacketState::login, kprotocol::PacketDirection::clientbound);
            if (decoded.key == kprotocol::packet_keys::login_success) {
                got_login_success = true;
            }
        }
    }
    KPC_CHECK(got_login_success, "received login_success");
    KPC_CHECK(saw_login_start.load(), "server saw login_start");

    write_packet(
        client_socket,
        registry,
        kprotocol::CLoginAcknowledgedPacket{}.to_packet(),
        kprotocol::ProtocolVersion::v1_21_1);

    for (int attempt = 0; attempt < 100 && !saw_login_ack.load(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    KPC_CHECK(saw_login_ack.load(), "server saw login_acknowledged");

    {
        std::lock_guard lock(state_mutex);
        KPC_CHECK(observed_state == kprotocol::PacketState::configuration, "configuration state");
    }

    client_socket.close(ec);
    server.stop();
    std::cout << "  login -> configuration flow ok\n";
    std::cout << "All server login tests passed.\n";
    return 0;
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "EXCEPTION: %s\n", ex.what());
        return 1;
    }
}
