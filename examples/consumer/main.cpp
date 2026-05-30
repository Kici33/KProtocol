#include "kprotocol/kprotocol.hpp"

#include <iostream>

int main() {
#ifdef KPROTOCOL_CONSUMER_TRANSLATION_MAPPINGS
    kprotocol::TranslationRegistry::set_block_mappings_path(
        KPROTOCOL_CONSUMER_TRANSLATION_MAPPINGS);
    if (!kprotocol::TranslationRegistry::block_mappings_available()) {
        std::cerr << "kprotocol consumer example: translation mappings are not loadable\n";
        return 1;
    }
#endif

    kprotocol::ProtocolRuntime runtime;

    const auto handshake = kprotocol::C00HandshakePacket{
        .protocol_version = 767,
        .server_address = "localhost",
        .server_port = 25565,
        .next_state = 1,
    };

    const auto bytes = runtime.registry.encode_packet(
        handshake.to_packet(), kprotocol::ProtocolVersion::v1_21_1);

    std::cout << "kprotocol consumer example: encoded handshake frame ("
              << bytes.size() << " bytes), registry has "
              << runtime.registry.size() << " packet keys\n";
    return 0;
}
