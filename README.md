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

### Version model

- **`WireProtocol`** — client handshake number (any int).
- **`KnownVersion`** — dense index for generated schemas and block mappings.
- **`ProtocolVersion`** — legacy wire-number enum for older APIs.

See [GENERATE.md](GENERATE.md#protocol-version-types) for details. Prefer
`client_wire()` + `catalog_known_version()` on sessions over bare wire enums.

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
`generated/` (packet keys across every known catalog version from 1.8 through
1.21.11, wire 47–774). Unknown future client wires (e.g. 1.26 when
minecraft-data adds it) resolve to the nearest compiled catalog anchor until you
regenerate.
Disable it with `-DKPROTOCOL_BUILD_GENERATED_PACKETS=OFF` if you only need the
hand-rolled baseline set.

### Regenerating the packet catalog

See [GENERATE.md](GENERATE.md) for full details. Quick version:

```bash
npm install minecraft-data
node tools/generate_packets.mjs \
    --out generated \
    --versions all-known
```

Or via CMake (requires Node.js on PATH):

```bash
cmake --build build --target kprotocol_generate_packets
```

Coverage stats land in `generated/coverage.json`. All known packet IDs are
registered by default. Supported shapes get typed schemas (arrays, slots,
options, version-specific layouts, and simple action switches); complex suffixes
that still need native modeling are preserved as named `tail` fields, and
packets without a minecraft-data body schema use a named `payload` field.


Second project (consumer) quickstart

**Option A — add_subdirectory (in-tree)**

```cmake
cmake_minimum_required(VERSION 3.21)
project(my_server LANGUAGES CXX)
add_subdirectory(../KProtocol kprotocol_build)
add_executable(my_server main.cpp)
target_link_libraries(my_server PRIVATE kprotocol::kprotocol)
```

**Option B — find_package (installed build)**

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/kprotocol/install
cmake --build build
```

```cmake
cmake_minimum_required(VERSION 3.21)
project(kprotocol_consumer LANGUAGES CXX)
find_package(kprotocol CONFIG REQUIRED)
add_executable(example main.cpp)
target_link_libraries(example PRIVATE kprotocol::kprotocol)
```

See `examples/consumer/` for a minimal installed-consumer layout (`main.cpp` +
`CMakeLists.txt`). After `cmake --install` from the kprotocol build tree, point
`CMAKE_PREFIX_PATH` at the install prefix and configure that example.

### Play demo server

`examples/play_demo_server.cpp` is the reference play-state server. It accepts
any supported client version, completes offline login, and sends title, action
bar, scoreboard, block change, and entity metadata using the internal version
(1.21.1) with automatic wire translation:

```bash
cmake -S . -B build -DKPROTOCOL_BUILD_EXAMPLES=ON
cmake --build build --target kprotocol_play_demo
./build/kprotocol_play_demo   # listens on 25565
```

Connect with a 1.8 or 1.21 client in offline mode to receive the showcase.

## Architecture

1. `kprotocol::PacketRegistry`
   - Stores **PacketSchema** entries: one logical packet key with per-version
     wire IDs and per-version field layouts.
   - Version-aware lookup picks the highest declared field set `<=` the
     requested protocol version.
   - Encodes/decodes using typed `FieldType` values (VarInt, UUID,
     Position, real anonymous/named NBT blobs, `rest_buffer` for opaque
     suffixes, …).
   - Legacy `PacketDefinition` registration is still supported and is
     lifted into a schema automatically.

2. `kprotocol::PacketTranslator`
   - Registers transforms for `(packet key, fromVersion, toVersion)`.
   - Converts packet payloads across versions while preserving universal packet keys.
   - `translate_checked()` reports whether a rule was applied, identity, or
     missing; strict mode can throw on missing rules instead of silently passing
     semantically risky packets through unchanged.

3. `kprotocol::MinecraftServer`
   - Asio + C++20 coroutine TCP runtime (one `io_context`, no thread-per-client).
   - Accepts TCP clients, decodes framed packets, handles handshake version/state.
   - Optional compression threshold on `start()` mirrors Minecraft Set Compression.
   - Sends packets translated to each client version.
   - Runtime guardrails include connection limits, bounded inbound frame
     buffering, disconnect callbacks, close-on-packet-error behavior, and
     optional strict translation checks.
   - Supports `ProtocolListener` hooks (`onPacketReceived`, `onPacketSent`,
     `onDisconnect`, `onError`).

## Packet class naming

- **Client -> Server**: `C<hexPacketId><Name>Packet` (example: `C0DMessagePacket`)
- **Server -> Client**: `S<hexPacketId><Name>Packet` (example: `S06SetBlockPacket`)

Each packet class carries strongly-typed fields and exposes:
- `to_packet()`
- `from_packet(const Packet&)`

## Quick start

One include, one object — no manual registry wiring:

```cpp
#include "kprotocol/kprotocol.hpp"

kprotocol::ProtocolServer app;

app.server().on_packet([](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
    if (packet.key == kprotocol::packet_keys::ping_request) {
        const auto ping = kprotocol::C01PingRequestPacket::from_packet(packet);
        client.send_packet(kprotocol::S01PongResponsePacket{.payload = ping.payload}.to_packet());
    }
});

app.server().start(25565, kprotocol::ProtocolVersion::v1_21_1);
```

Cross-version UI and blocks without touching wire details:

```cpp
kprotocol::send_title(client, {
    .title = "Welcome",
    .subtitle = "KProtocol",
});

kprotocol::send_action_bar(client, "Hello!");
kprotocol::clear_title(client);
kprotocol::send_scoreboard_objective(client, "sidebar", "Scores");
kprotocol::send_scoreboard_score(client, "Player", "sidebar", 10);
kprotocol::send_scoreboard_display(client, "sidebar", 1);
// Or in one call:
kprotocol::send_scoreboard_sidebar(client, "myobj", "Scores", {
    {.entry = "Player", .value = 10},
}, 1);
kprotocol::send_block_change(client, kprotocol::ProtocolVersion::v1_21_1,
    kprotocol::Position{.x = 0, .y = 64, .z = 0}, "stone");
```

Title, action bar, and scoreboard helpers pick the correct wire shape per client
version (legacy `title`/`chat` on 1.8–1.16.5, split title + `action_bar` from
1.17+, NBT text components from 1.20.4+). Typed packet classes are in
`kprotocol/packets/play/play_packets.hpp` (`S45TitlePacket`, `S55ActionBarPacket`,
`S3BScoreboardObjectivePacket`, `S3CScoreboardScorePacket`, etc.). For
encode-only work without a server, use `kprotocol::ProtocolRuntime` instead of
`ProtocolServer`.

Lower-level manual setup is still available:

```cpp
kprotocol::PacketRegistry registry;
kprotocol::PacketTranslator translator;
kprotocol::initialize(registry, translator);
kprotocol::MinecraftServer server(registry, translator);

kprotocol::ServerRuntimeOptions runtime;
runtime.max_connections = 512;
runtime.max_inbound_buffer = 2 * 1024 * 1024 + 5;
runtime.disconnect_on_packet_error = true;
runtime.require_explicit_translations = true;
server.set_runtime_options(runtime);
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

const auto result = translator.translate_checked(packet,
    kprotocol::ProtocolVersion::v1_8,
    kprotocol::ProtocolVersion::v1_21_1);
if (result.missing()) {
    // Decide whether this packet is safe to pass through unchanged.
}
```
