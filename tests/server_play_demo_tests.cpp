// End-to-end play demo: after login, server sends block/title/scoreboard/metadata
// to 1.8 (wire 47) and 1.21.1 (wire 767) clients from one internal version.

#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/initialize.hpp"
#include "kprotocol/packets/packet_keys.hpp"
#include "kprotocol/play_demo.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/server.hpp"

#include "kprotocol/generated/packet_keys.hpp"

#include <asio/connect.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

#define KPC_CHECK(cond, msg)                                                  \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL: %s (%s:%d) %s\n", #cond, __FILE__,     \
                         __LINE__, (msg));                                     \
            throw std::runtime_error(msg);                                     \
        }                                                                      \
    } while (false)

namespace {

constexpr kprotocol::ProtocolVersion kInternalVersion = kprotocol::ProtocolVersion::v1_21_1;

void write_packet(asio::ip::tcp::socket& socket,
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

std::set<std::string> decode_clientbound_keys(
    std::vector<std::uint8_t>& inbound,
    kprotocol::PacketRegistry& registry,
    const kprotocol::ProtocolVersion version,
    kprotocol::PacketState state) {

    std::set<std::string> keys;
    while (true) {
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        if (!kprotocol::codec::try_decode_frame(inbound, consumed, frame)) {
            break;
        }
        inbound.erase(inbound.begin(), inbound.begin() + static_cast<std::ptrdiff_t>(consumed));
        try {
            const auto decoded = registry.decode_packet(
                frame, version, state, kprotocol::PacketDirection::clientbound);
            keys.insert(decoded.key);
        } catch (const std::exception&) {
            // Ignore undecodable frames while draining configuration/login.
        }
    }
    return keys;
}

void read_some(asio::ip::tcp::socket& socket, std::vector<std::uint8_t>& inbound) {
    asio::error_code ec;
    std::array<std::uint8_t, 2048> buf{};
    socket.non_blocking(true);
    const std::size_t n = socket.read_some(asio::buffer(buf), ec);
    if (ec == asio::error::would_block) {
        return;
    }
    if (ec) {
        throw std::runtime_error("client read failed: " + ec.message());
    }
    inbound.insert(inbound.end(), buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(n));
}

std::set<std::string> collect_play_keys(
    asio::ip::tcp::socket& socket,
    kprotocol::PacketRegistry& registry,
    const kprotocol::ProtocolVersion version,
    const int max_attempts = 200) {

    std::vector<std::uint8_t> inbound;
    inbound.reserve(8192);
    std::set<std::string> keys;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        read_some(socket, inbound);
        auto batch = decode_clientbound_keys(inbound, registry, version, kprotocol::PacketState::play);
        keys.insert(batch.begin(), batch.end());
        if (keys.contains("play.clientbound.block_change") &&
            keys.contains("play.clientbound.scoreboard_display_objective") &&
            keys.contains("play.clientbound.scoreboard_objective") &&
            keys.contains("play.clientbound.scoreboard_score") &&
            keys.contains("play.clientbound.entity_metadata") &&
            (keys.contains("play.clientbound.title") ||
             keys.contains("play.clientbound.set_title_text")) &&
            (keys.contains("play.clientbound.chat") ||
             keys.contains("play.clientbound.action_bar"))) {
            return keys;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return keys;
}

void register_login_ack_schema(kprotocol::PacketRegistry& registry) {
    registry.register_schema(kprotocol::PacketSchema{
        .key = std::string(kprotocol::generated::packet_keys::login_serverbound_login_acknowledged),
        .state = kprotocol::PacketState::login,
        .direction = kprotocol::PacketDirection::serverbound,
        .field_sets = {{kprotocol::ProtocolVersion::v1_20_2, {}}},
        .ids = {{kprotocol::ProtocolVersion::v1_20_4, 3}, {kprotocol::ProtocolVersion::v1_21_1, 3}},
    });
}

kprotocol::MinecraftServer make_play_demo_server(kprotocol::PacketRegistry& registry,
                                                 kprotocol::PacketTranslator& translator) {
    kprotocol::MinecraftServer server(registry, translator);
    server.on_packet([](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
        if (packet.key == kprotocol::packet_keys::login_start) {
            KPC_CHECK(kprotocol::send_offline_login_success(client, "PlayDemo"), "offline login");
            if (kprotocol::protocol_number(client.protocol_version()) <
                kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_20_2)) {
                KPC_CHECK(kprotocol::send_play_demo(client, kInternalVersion), "play demo 1.8");
            }
            return;
        }
        if (packet.key ==
            std::string(kprotocol::generated::packet_keys::configuration_serverbound_finish_configuration)) {
            KPC_CHECK(kprotocol::send_play_demo(client, kInternalVersion), "play demo 1.21");
        }
    });
    return server;
}

void run_modern_client_flow(
    kprotocol::PacketRegistry& registry,
    const std::uint16_t port,
    const kprotocol::ProtocolVersion version,
    const std::int32_t wire) {

    asio::io_context client_io;
    asio::ip::tcp::socket socket(client_io);
    asio::error_code ec;
    asio::connect(
        socket,
        std::array{asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)},
        ec);
    KPC_CHECK(!ec, "connect");

    kprotocol::C00HandshakePacket handshake;
    handshake.protocol_version = wire;
    handshake.server_address = "localhost";
    handshake.server_port = 25565;
    handshake.next_state = 2;
    write_packet(socket, registry, handshake.to_packet(), version);

    write_packet(
        socket,
        registry,
        kprotocol::C00LoginStartPacket{.username = "PlayDemo"}.to_packet(),
        version);

    std::vector<std::uint8_t> inbound;
    bool got_login_success = false;
    for (int attempt = 0; attempt < 100 && !got_login_success; ++attempt) {
        read_some(socket, inbound);
        auto keys = decode_clientbound_keys(inbound, registry, version, kprotocol::PacketState::login);
        if (keys.contains(kprotocol::packet_keys::login_success)) {
            got_login_success = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    KPC_CHECK(got_login_success, "login_success");

    kprotocol::Packet login_ack;
    login_ack.key = std::string(kprotocol::generated::packet_keys::login_serverbound_login_acknowledged);
    login_ack.state = kprotocol::PacketState::login;
    login_ack.direction = kprotocol::PacketDirection::serverbound;
    write_packet(socket, registry, login_ack, version);

    kprotocol::Packet finish;
    finish.key = std::string(kprotocol::generated::packet_keys::configuration_serverbound_finish_configuration);
    finish.state = kprotocol::PacketState::configuration;
    finish.direction = kprotocol::PacketDirection::serverbound;
    write_packet(socket, registry, finish, version);

    const auto keys = collect_play_keys(socket, registry, version);
    KPC_CHECK(keys.contains("play.clientbound.block_change"), "block_change");
    KPC_CHECK(keys.contains("play.clientbound.scoreboard_display_objective"), "scoreboard display");
    KPC_CHECK(keys.contains("play.clientbound.scoreboard_objective"), "scoreboard objective");
    KPC_CHECK(keys.contains("play.clientbound.scoreboard_score"), "scoreboard score");
    KPC_CHECK(keys.contains("play.clientbound.entity_metadata"), "entity_metadata");
    KPC_CHECK(
        keys.contains("play.clientbound.set_title_text") || keys.contains("play.clientbound.title"),
        "title");
    KPC_CHECK(keys.contains("play.clientbound.action_bar"), "action_bar");

    socket.close(ec);
}

void run_legacy_client_flow(
    kprotocol::PacketRegistry& registry,
    const std::uint16_t port) {

    asio::io_context client_io;
    asio::ip::tcp::socket socket(client_io);
    asio::error_code ec;
    asio::connect(
        socket,
        std::array{asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), port)},
        ec);
    KPC_CHECK(!ec, "connect");

    kprotocol::C00HandshakePacket handshake;
    handshake.protocol_version = 47;
    handshake.server_address = "localhost";
    handshake.server_port = 25565;
    handshake.next_state = 2;
    write_packet(socket, registry, handshake.to_packet(), kprotocol::ProtocolVersion::v1_8);

    write_packet(
        socket,
        registry,
        kprotocol::C00LoginStartPacket{.username = "PlayDemo"}.to_packet(),
        kprotocol::ProtocolVersion::v1_8);

    const auto keys = collect_play_keys(socket, registry, kprotocol::ProtocolVersion::v1_8);
    KPC_CHECK(keys.contains("play.clientbound.block_change"), "block_change");
    KPC_CHECK(keys.contains("play.clientbound.scoreboard_display_objective"), "scoreboard display");
    KPC_CHECK(keys.contains("play.clientbound.scoreboard_objective"), "scoreboard objective");
    KPC_CHECK(keys.contains("play.clientbound.scoreboard_score"), "scoreboard score");
    KPC_CHECK(keys.contains("play.clientbound.entity_metadata"), "entity_metadata");
    KPC_CHECK(keys.contains("play.clientbound.title"), "title");
    KPC_CHECK(keys.contains("play.clientbound.chat"), "action bar chat");

    socket.close(ec);
}

} // namespace

int main() {
    std::cout << "server_play_demo_tests:\n";
    try {
        kprotocol::PacketRegistry registry;
        kprotocol::PacketTranslator translator;
        kprotocol::initialize(registry, translator);
        register_login_ack_schema(registry);

        auto server = make_play_demo_server(registry, translator);
        KPC_CHECK(server.start(0, kInternalVersion), "server.start");
        const auto port = server.listen_port();

        std::cout << "  1.8 client receives play demo... ";
        run_legacy_client_flow(registry, port);
        std::cout << "ok\n";

        std::cout << "  1.21.1 client receives play demo... ";
        run_modern_client_flow(
            registry, port, kprotocol::ProtocolVersion::v1_21_1, 767);
        std::cout << "ok\n";

        server.stop();
    } catch (const std::exception& ex) {
        std::cerr << "EXCEPTION: " << ex.what() << '\n';
        return 1;
    }

    std::cout << "All server play demo tests passed.\n";
    return 0;
}
