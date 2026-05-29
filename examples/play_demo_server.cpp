#include "kprotocol/kprotocol.hpp"

#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
#include "kprotocol/generated/packet_keys.hpp"
#endif

namespace {

constexpr kprotocol::ProtocolVersion kInternalVersion = kprotocol::ProtocolVersion::v1_21_1;

void handle_play_demo(
    const kprotocol::ClientSession& client,
    const kprotocol::Packet& packet) {

    if (packet.key == kprotocol::packet_keys::login_start) {
        if (!kprotocol::send_offline_login_success(client, "DemoPlayer")) {
            std::cerr << "login failed for " << client.remote_address() << '\n';
            return;
        }
        if (kprotocol::protocol_number(client.protocol_version()) <
            kprotocol::protocol_number(kprotocol::ProtocolVersion::v1_20_2)) {
            if (!kprotocol::send_play_demo(client, kInternalVersion)) {
                std::cerr << "play demo failed for " << client.remote_address() << '\n';
            }
        }
        return;
    }

#if defined(KPROTOCOL_HAS_GENERATED_CATALOG)
    if (packet.key == std::string(kprotocol::generated::packet_keys::configuration_serverbound_finish_configuration)) {
        if (!kprotocol::send_play_demo(client, kInternalVersion)) {
            std::cerr << "play demo failed for " << client.remote_address() << '\n';
        }
    }
#endif
}

} // namespace

int main(int argc, char** argv) {
    std::uint16_t port = 25565;
    if (const char* env_port = std::getenv("KPROTOCOL_PORT"); env_port != nullptr) {
        port = static_cast<std::uint16_t>(std::stoi(env_port));
    }
    if (argc > 1) {
        port = static_cast<std::uint16_t>(std::stoi(argv[1]));
    }

    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::initialize(registry, translator);

    kprotocol::MinecraftServer server(registry, translator);
    server.on_error([](const std::string& error) {
        std::cerr << "[KProtocol] " << error << '\n';
    });

    server.on_packet([](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
        if (packet.key == kprotocol::packet_keys::status_request) {
            const auto response = kprotocol::S00StatusResponsePacket{
                .json_response = R"({"version":{"name":"KProtocol Play Demo","protocol":767},"players":{"max":100,"online":0},"description":{"text":"Connect and login to receive title, action bar, scoreboard, block change, and entity metadata."}})"
            };
            (void)client.send_packet(response.to_packet());
        } else if (packet.key == kprotocol::packet_keys::ping_request) {
            const auto ping = kprotocol::C01PingRequestPacket::from_packet(packet);
            (void)client.send_packet(kprotocol::S01PongResponsePacket{.payload = ping.payload}.to_packet());
        } else {
            handle_play_demo(client, packet);
        }
    });

    if (!server.start(port, kInternalVersion)) {
        std::cerr << "Failed to start play demo server\n";
        return 1;
    }

    std::cout << "Play demo server on port " << server.listen_port()
              << " (internal " << kprotocol::name_of(kInternalVersion) << ")\n";
    std::cout << "Login in offline mode to receive the play-state packet showcase.\n";

    while (server.running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
