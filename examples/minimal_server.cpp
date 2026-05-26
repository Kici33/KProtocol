#include "kprotocol/kprotocol.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

class LoggingProtocolListener final : public kprotocol::ProtocolListener {
public:
    void onPacketReceived(const kprotocol::ClientSession& client, const kprotocol::Packet& packet) override {
        std::cout << "[recv] " << client.remote_address() << " => " << packet.key << '\n';
    }

    void onPacketSent(const kprotocol::ClientSession& client, const kprotocol::Packet& packet) override {
        std::cout << "[sent] " << client.remote_address() << " <= " << packet.key << '\n';
    }
};

int main() {
    kprotocol::ProtocolServer app;
    app.server().add_listener(std::make_shared<LoggingProtocolListener>());

    app.server().on_error([](const std::string& error) {
        std::cerr << "[KProtocol] " << error << '\n';
    });

    app.server().on_packet([](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
        if (packet.key == kprotocol::packet_keys::status_request) {
            const auto response = kprotocol::S00StatusResponsePacket{
                .json_response = R"({"version":{"name":"KProtocol","protocol":767},"players":{"max":100,"online":0},"description":{"text":"KProtocol Server"}})"
            };
            (void)client.send_packet(response.to_packet());
        } else if (packet.key == kprotocol::packet_keys::ping_request) {
            const auto ping = kprotocol::C01PingRequestPacket::from_packet(packet);
            const auto pong = kprotocol::S01PongResponsePacket{.payload = ping.payload};
            (void)client.send_packet(pong.to_packet());
        }
    });

    if (!app.server().start(25565, kprotocol::ProtocolVersion::v1_21_1)) {
        std::cerr << "Failed to start server\n";
        return 1;
    }

    std::cout << "Server running on port 25565\n";
    while (app.server().running()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}
