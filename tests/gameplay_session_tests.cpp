// Gameplay/session model tests.

#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/gameplay_session.hpp"
#include "kprotocol/packets/packet_keys.hpp"
#include "kprotocol/play_demo.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/server.hpp"
#include "kprotocol/translation.hpp"

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
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

void write_packet(
    asio::ip::tcp::socket& socket,
    kprotocol::PacketRegistry& registry,
    const kprotocol::Packet& packet,
    const kprotocol::ProtocolVersion version) {

    asio::error_code ec;
    const auto bytes = registry.encode_packet(packet, version);
    asio::write(socket, asio::buffer(bytes), ec);
    if (ec) {
        throw std::runtime_error("client write failed: " + ec.message());
    }
}

void test_login_profile_and_authoritative_state() {
    std::cout << "  login profile and authoritative state... ";

    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::register_baseline_packets(registry, translator);

    std::atomic<bool> saw_login{false};
    kprotocol::GameplaySession observed;

    kprotocol::MinecraftServer server(registry, translator);
    server.on_packet([&](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
        if (packet.key == kprotocol::packet_keys::login_start) {
            saw_login = true;

            const auto before = client.gameplay_session();
            KPC_CHECK(before.profile.username == "SessionPlayer", "login username captured");
            KPC_CHECK(before.login_started, "login started");

            client.set_entity_id(42);
            client.set_game_mode(kprotocol::GameMode::creative);
            client.set_dimension("minecraft:the_nether");
            client.set_location(kprotocol::PlayerLocation{
                .x = 1.25,
                .y = 65.0,
                .z = -4.5,
                .yaw = 90.0F,
                .pitch = 10.0F,
                .on_ground = true,
                .known = true,
            });
            KPC_CHECK(kprotocol::send_offline_login_success(
                          client,
                          "SessionPlayer",
                          "00000000-0000-0000-0000-000000000042"),
                      "offline login success");
            client.mark_joined_game();
            observed = client.gameplay_session();
        }
    });

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

    kprotocol::C00HandshakePacket handshake;
    handshake.protocol_version = 767;
    handshake.server_address = "localhost";
    handshake.server_port = 25565;
    handshake.next_state = 2;
    write_packet(socket, registry, handshake.to_packet(), kprotocol::ProtocolVersion::v1_21_1);

    write_packet(
        socket,
        registry,
        kprotocol::C00LoginStartPacket{.username = "SessionPlayer"}.to_packet(),
        kprotocol::ProtocolVersion::v1_21_1);

    for (int attempt = 0; attempt < 100 && !saw_login.load(); ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    KPC_CHECK(saw_login.load(), "server saw login_start");
    KPC_CHECK(observed.profile.username == "SessionPlayer", "profile username");
    KPC_CHECK(observed.profile.uuid == "00000000-0000-0000-0000-000000000042", "profile uuid");
    KPC_CHECK(observed.login_complete, "login complete");
    KPC_CHECK(observed.joined_game, "joined game");
    KPC_CHECK(observed.entity_id == 42, "entity id");
    KPC_CHECK(observed.game_mode == kprotocol::GameMode::creative, "game mode");
    KPC_CHECK(observed.dimension == "minecraft:the_nether", "dimension");
    KPC_CHECK(observed.location.known, "location known");
    KPC_CHECK(observed.location.x == 1.25, "location x");
    KPC_CHECK(observed.location.on_ground, "on ground");

    socket.close(ec);
    server.stop();
    std::cout << "ok\n";
}

} // namespace

int main() {
    std::cout << "gameplay_session_tests:\n";
    try {
        test_login_profile_and_authoritative_state();
    } catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << '\n';
        return 1;
    }
    std::cout << "All gameplay/session tests passed.\n";
    return 0;
}
