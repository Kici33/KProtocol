# KProtocol (C++20)

KProtocol is a modern Minecraft protocol library focused on:

- **universal packets** (same packet key and schema across versions)
- **multi-version packet IDs** (version-specific wire IDs hidden behind registry)
- **translation pipeline** (payload transforms between protocol versions)
- **packet-level TCP server runtime** (build protocol-only servers)
- **typed packet classes** (`CxxNamePacket` / `SxxNamePacket`)

## Current scope

The framework is production-style and extensible for full protocol coverage.  
This repository currently ships a **baseline packet set** (handshake/status/login/keepalive) and all extension points needed to register every remaining Minecraft packet.

## Build

Prerequisites:
- CMake 3.21+
- A C++20 toolchain (MSVC, clang-cl, GCC 10+)
- zlib (system install or vcpkg)
- On Windows: Visual Studio or appropriate build tools

```bash
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

By default the library compiles the committed multi-version packet catalog under
`generated/` (248 packet keys across 1.8, 1.12.2, 1.16.5, 1.20.4, 1.21.1).
Disable it with `-DKPROTOCOL_BUILD_GENERATED_PACKETS=OFF` if you only need the
hand-rolled baseline set.

### Regenerating the packet catalog

See [GENERATE.md](GENERATE.md) for full details. Quick version:

```bash
npm install minecraft-data
node tools/generate_packets.mjs \
    --out generated \
    --versions 1.8,1.12.2,1.16.5,1.20.4,1.21.1
```

Or via CMake (requires Node.js on PATH):

```bash
cmake --build build --target kprotocol_generate_packets
```

Coverage stats land in `generated/coverage.json`. At the baseline versions
most packets are emitted as opaque `rest_buffer` blobs today; fully typed
schemas grow as compound field support is added in later waves.


Second project (consumer) quickstart

1. Create a new CMake project and add this repo as a subdirectory, or build kprotocol as an installed library. Example minimal CMakeLists for consumer:

```cmake
cmake_minimum_required(VERSION 3.21)
project(kprotocol_consumer LANGUAGES CXX)
add_executable(example src/main.cpp)
add_subdirectory(../ cpp-pro_build) # or use find_package if installed
target_link_libraries(example PRIVATE kprotocol)
```

2. In consumer source, include `<kprotocol/kprotocol.hpp>` and link against `kprotocol`.

3. Example run (from consumer root):
```bash
cmake -S . -B build
cmake --build build --config Release
./build/example
```

## Architecture

1. `kprotocol::PacketRegistry`
   - Stores **PacketSchema** entries: one logical packet key with per-version
     wire IDs and per-version field layouts.
   - Version-aware lookup picks the highest declared field set `<=` the
     requested protocol version.
   - Encodes/decodes using typed `FieldType` values (VarInt, UUID,
     Position, `rest_buffer` for opaque blobs, …).
   - Legacy `PacketDefinition` registration is still supported and is
     lifted into a schema automatically.

2. `kprotocol::PacketTranslator`
   - Registers transforms for `(packet key, fromVersion, toVersion)`.
   - Converts packet payloads across versions while preserving universal packet keys.

3. `kprotocol::MinecraftServer`
   - Accepts TCP clients.
   - Decodes framed packets to universal packets.
   - Handles handshake version negotiation/state switching.
   - Sends packets translated to each client version.
   - Supports `ProtocolListener` hooks (`onPacketReceived`, `onPacketSent`, `onError`).

## Packet class naming

- **Client -> Server**: `C<hexPacketId><Name>Packet` (example: `C0DMessagePacket`)
- **Server -> Client**: `S<hexPacketId><Name>Packet` (example: `S06SetBlockPacket`)

Each packet class carries strongly-typed fields and exposes:
- `to_packet()`
- `from_packet(const Packet&)`

## Quick start

```cpp
#include "kprotocol/kprotocol.hpp"

kprotocol::PacketRegistry registry;
kprotocol::PacketTranslator translator;
kprotocol::register_baseline_packets(registry, translator);
#ifdef KPROTOCOL_GENERATED
kprotocol::register_generated_packets(registry);  // 248 keys from minecraft-data
#endif

kprotocol::MinecraftServer server(registry, translator);
class MyListener : public kprotocol::ProtocolListener {
public:
    void onPacketReceived(const kprotocol::ClientSession&, const kprotocol::Packet& packet) override {
        // inspect all inbound packets
    }
};

server.add_listener(std::make_shared<MyListener>());
server.on_packet([](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
    if (packet.key == kprotocol::packet_keys::ping_request) {
        const auto ping = kprotocol::C01PingRequestPacket::from_packet(packet);
        client.send_packet(kprotocol::S01PongResponsePacket{.payload = ping.payload}.to_packet());
    }
});
server.start(25565, kprotocol::ProtocolVersion::v1_21_1);
```

## Adding a new packet

Prefer `PacketSchema` when the wire layout differs by version:

```cpp
kprotocol::PacketSchema schema;
schema.key = "play.clientbound.my_packet";
schema.state = kprotocol::PacketState::play;
schema.direction = kprotocol::PacketDirection::clientbound;
schema.ids[kprotocol::ProtocolVersion::v1_20_4] = 0x42;
schema.field_sets[kprotocol::ProtocolVersion::v1_20_4] = {
    {"message", kprotocol::FieldType::string},
};
registry.register_schema(std::move(schema));
```

For a single layout across all versions, `register_definition` still works:

```cpp
registry.register_definition(kprotocol::PacketDefinition{
    .key = "chat_message",
    .state = kprotocol::PacketState::play,
    .direction = kprotocol::PacketDirection::serverbound,
    .fields = {
        {"message", kprotocol::FieldType::string}
    },
    .ids = {
        {kprotocol::ProtocolVersion::v1_8, 0x01},
        {kprotocol::ProtocolVersion::v1_12_2, 0x02},
        {kprotocol::ProtocolVersion::v1_16_5, 0x03},
        {kprotocol::ProtocolVersion::v1_20_4, 0x04},
        {kprotocol::ProtocolVersion::v1_21_1, 0x05}
    }
});
```

## Adding a translator

```cpp
translator.register_translation(
    "chat_message",
    kprotocol::ProtocolVersion::v1_8,
    kprotocol::ProtocolVersion::v1_21_1,
    [](const kprotocol::PacketFields& fields) {
        auto out = fields;
        // mutate fields as needed
        return out;
    });
```
