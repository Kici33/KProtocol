// Wave 3: loopback smoke test for the Asio coroutine server runtime.
//
// Verifies:
//   - MinecraftServer binds an ephemeral port and accepts connections
//   - handshake + status_request round-trip produces status_response
//   - server.stop() shuts down cleanly

#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/packets/packet_keys.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/server.hpp"
#include "kprotocol/translation.hpp"

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <atomic>
#include <iostream>
#include <memory>
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

int main() {
    std::cout << "server_smoke_tests:\n";

    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::register_baseline_packets(registry, translator);

    kprotocol::MinecraftServer server(registry, translator);
    kprotocol::ServerRuntimeOptions options;
    options.max_connections = 4;
    options.max_inbound_buffer = 4096;
    options.disconnect_on_packet_error = true;
    KPC_CHECK(server.set_runtime_options(options), "set runtime options before start");
    KPC_CHECK(server.runtime_options().max_connections == 4, "runtime options max_connections");
    KPC_CHECK(server.runtime_options().max_inbound_buffer == 4096, "runtime options inbound buffer");

    struct DisconnectCounter final : kprotocol::ProtocolListener {
        std::atomic<int>* count{};
        explicit DisconnectCounter(std::atomic<int>& value) : count(&value) {}
        void onDisconnect(const kprotocol::ClientSession&, const std::string&) override {
            ++(*count);
        }
    };
    std::atomic<int> disconnects{0};
    server.add_listener(std::make_shared<DisconnectCounter>(disconnects));

    server.on_packet([](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
        if (packet.key == kprotocol::packet_keys::status_request) {
            const auto response = kprotocol::S00StatusResponsePacket{
                .json_response = R"({"version":{"name":"KProtocol","protocol":767},"players":{"max":0,"online":0},"description":{"text":"smoke"}})"
            };
            (void)client.send_packet(response.to_packet());
        }
    });

    KPC_CHECK(server.start(0, kprotocol::ProtocolVersion::v1_21_1), "server.start");
    KPC_CHECK(!server.set_runtime_options(options), "runtime options locked while running");
    const auto port = server.listen_port();
    KPC_CHECK(port != 0, "ephemeral port assigned");
    std::cout << "  listening on port " << port << "... ";

    asio::io_context client_io;
    asio::ip::tcp::socket client_socket(client_io);
    asio::error_code ec;
    asio::connect(
        client_socket,
        std::array{asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)},
        ec);
    KPC_CHECK(!ec, "client connect");
    KPC_CHECK(server.active_connections() <= 1, "active connection count");

    kprotocol::C00HandshakePacket handshake;
    handshake.protocol_version = 767;
    handshake.server_address = "localhost";
    handshake.server_port = 25565;
    handshake.next_state = 1;
    const auto hs_bytes = registry.encode_packet(
        handshake.to_packet(), kprotocol::ProtocolVersion::v1_21_1);
    asio::write(client_socket, asio::buffer(hs_bytes), ec);
    KPC_CHECK(!ec, "write handshake");

    const auto status_req_bytes = registry.encode_packet(
        kprotocol::C00StatusRequestPacket{}.to_packet(),
        kprotocol::ProtocolVersion::v1_21_1);
    asio::write(client_socket, asio::buffer(status_req_bytes), ec);
    KPC_CHECK(!ec, "write status_request");

    std::vector<std::uint8_t> inbound;
    inbound.reserve(4096);
    std::array<std::uint8_t, 1024> buf{};
    bool got_response = false;

    for (int attempt = 0; attempt < 50 && !got_response; ++attempt) {
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
            const auto decoded = registry.decode_packet(
                frame, kprotocol::ProtocolVersion::v1_21_1,
                kprotocol::PacketState::status, kprotocol::PacketDirection::clientbound);
            KPC_CHECK(decoded.key == kprotocol::packet_keys::status_response, "status_response key");
            got_response = true;
        }
    }
    KPC_CHECK(got_response, "received status_response");

    client_socket.close(ec);
    for (int attempt = 0; attempt < 50 && disconnects.load() == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    KPC_CHECK(disconnects.load() >= 1, "disconnect listener called");
    server.stop();
    std::cout << "ok\n";
    std::cout << "All server smoke tests passed.\n";
    return 0;
}
