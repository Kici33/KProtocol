#pragma once

#include "kprotocol/registry.hpp"
#include "kprotocol/translation.hpp"

// Packet key constants
#include "kprotocol/packets/packet_keys.hpp"

// Typed packet classes (segregated by packet key prefix)
#include "kprotocol/packets/handshake/C00HandshakePacket.hpp"
#include "kprotocol/packets/status/C00StatusRequestPacket.hpp"
#include "kprotocol/packets/status/S00StatusResponsePacket.hpp"
#include "kprotocol/packets/ping/C01PingRequestPacket.hpp"
#include "kprotocol/packets/pong/S01PongResponsePacket.hpp"
#include "kprotocol/packets/login/C00LoginStartPacket.hpp"
#include "kprotocol/packets/login/S02LoginSuccessPacket.hpp"
#include "kprotocol/packets/login/S00LoginDisconnectPacket.hpp"
#include "kprotocol/packets/keep_alive/C15KeepAlivePacket.hpp"
#include "kprotocol/packets/keep_alive/S24KeepAlivePacket.hpp"

namespace kprotocol {

void register_baseline_packets(PacketRegistry& registry, PacketTranslator& translator);

} // namespace kprotocol
