#include "kprotocol/baseline_packets.hpp"
#include "kprotocol/codec.hpp"
#include "kprotocol/registry.hpp"
#include "kprotocol/types.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    {
        std::vector<std::uint8_t> bytes;
        kprotocol::codec::write_var_int(bytes, 300);
        std::size_t offset = 0;
        const auto value = kprotocol::codec::read_var_int(bytes, offset);
        assert(value == 300);
        assert(offset == bytes.size());
    }

    {
        const std::vector<std::uint8_t> payload{0x01, 0x02, 0x03};
        const auto frame = kprotocol::codec::encode_frame(0x2A, payload);

        kprotocol::codec::EncodedFrame decoded;
        std::size_t consumed = 0;
        const bool ok = kprotocol::codec::try_decode_frame(frame, consumed, decoded);
        assert(ok);
        assert(consumed == frame.size());
        assert(decoded.packet_id == 0x2A);
        assert(decoded.payload == payload);
    }

    {
        kprotocol::PacketRegistry registry;
        kprotocol::PacketTranslator translator;
        kprotocol::register_baseline_packets(registry, translator);

        const auto handshake_packet = kprotocol::C00HandshakePacket{
            .protocol_version = 767,
            .server_address = "localhost",
            .server_port = static_cast<std::uint16_t>(25565),
            .next_state = 1
        };
        const auto handshake = handshake_packet.to_packet();

        const auto encoded = registry.encode_packet(handshake, kprotocol::ProtocolVersion::v1_21_1);
        kprotocol::codec::EncodedFrame frame;
        std::size_t consumed = 0;
        const bool ok = kprotocol::codec::try_decode_frame(encoded, consumed, frame);
        assert(ok);

        const auto decoded = registry.decode_packet(
            frame,
            kprotocol::ProtocolVersion::v1_21_1,
            kprotocol::PacketState::handshaking,
            kprotocol::PacketDirection::serverbound);

        assert(decoded.key == handshake.key);
        const auto typed = kprotocol::C00HandshakePacket::from_packet(decoded);
        assert(typed.protocol_version == 767);
        assert(typed.server_address == "localhost");
        assert(typed.server_port == 25565);
        assert(typed.next_state == 1);
    }

    // Slot encoding roundtrip
    {
        kprotocol::types::Slot s;
        s.present = true;
        s.item_id = 123;
        s.count = 64;
        s.nbt.data = {0x0A, 0x00};

        const auto blob = kprotocol::types::slot_to_bytes(s);
        const auto s2 = kprotocol::types::slot_from_bytes(blob);
        assert(s2.present == s.present);
        assert(s2.item_id == s.item_id);
        assert(s2.count == s.count);
        assert(s2.nbt.data == s.nbt.data);
    }

    return 0;
}
