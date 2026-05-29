// Production connection-management smoke tests.

#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/server.hpp"
#include "kprotocol/translation.hpp"

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            throw std::runtime_error(msg);                                     \
        }                                                                      \
    } while (false)

namespace {

struct DisconnectRecorder final : kprotocol::ProtocolListener {
    std::atomic<int>* count{};
    std::string* last_reason{};

    DisconnectRecorder(std::atomic<int>& disconnect_count, std::string& reason)
        : count(&disconnect_count), last_reason(&reason) {}

    void onDisconnect(const kprotocol::ClientSession&, const std::string& reason) override {
        *last_reason = reason;
        ++(*count);
    }
};

void test_handshake_timeout_closes_idle_peer() {
    std::cout << "  handshake timeout closes idle peer... ";

    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::register_baseline_packets(registry, translator);

    kprotocol::MinecraftServer server(registry, translator);
    kprotocol::ServerRuntimeOptions options;
    options.handshake_timeout_ms = 100;
    options.idle_timeout_ms = 0;
    KPC_CHECK(server.set_runtime_options(options), "set runtime options");

    std::atomic<int> disconnects{0};
    std::string last_reason;
    server.add_listener(std::make_shared<DisconnectRecorder>(disconnects, last_reason));

    KPC_CHECK(server.start(0), "server.start");
    const auto port = server.listen_port();

    asio::io_context client_io;
    asio::ip::tcp::socket socket(client_io);
    asio::error_code ec;
    asio::connect(
        socket,
        std::array{asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)},
        ec);
    KPC_CHECK(!ec, "client connect");

    for (int attempt = 0; attempt < 40 && disconnects.load() == 0; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    KPC_CHECK(disconnects.load() >= 1, "disconnect listener");
    KPC_CHECK(last_reason == "handshake timeout" || last_reason == "connection closed",
              "disconnect reason");

    socket.close(ec);
    server.stop();
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "server_connection_management_tests:\n";
    try {
        test_handshake_timeout_closes_idle_peer();
    } catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "All connection-management tests passed.\n";
    return 0;
}
