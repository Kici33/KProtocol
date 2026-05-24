#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/kprotocol.hpp"

#include <iostream>

int main() {
    kprotocol::PacketRegistry registry;
    kprotocol::PacketTranslator translator;
    kprotocol::register_baseline_packets(registry, translator);
#ifdef KPROTOCOL_HAS_GENERATED_CATALOG
    kprotocol::register_generated_packets(registry);
#endif

    const auto handshake = kprotocol::C00HandshakePacket{
        .protocol_version = 767,
        .server_address = "localhost",
        .server_port = 25565,
        .next_state = 1,
    };

    const auto bytes = registry.encode_packet(
        handshake.to_packet(), kprotocol::ProtocolVersion::v1_21_1);

    std::cout << "kprotocol consumer example: encoded handshake frame ("
              << bytes.size() << " bytes), registry has "
              << registry.size() << " packet keys\n";
    return 0;
}
