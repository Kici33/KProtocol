# KProtocol (C++20)

KProtocol is a modern Minecraft protocol library focused on:

- **universal packets** (same packet key and schema across versions)
- **multi-version packet IDs** (version-specific wire IDs hidden behind registry)
- **translation pipeline** (payload transforms between protocol versions)
- **packet-level TCP server runtime** (build protocol-only servers)
- **typed packet classes** (`CxxNamePacket` / `SxxNamePacket`)

## Current scope

The framework is production-style and extensible for full protocol coverage.  
This repository ships the committed generated Minecraft packet catalog, a
hand-written ergonomic packet layer for common flows, and extension points for
application-specific packets.

### What is covered

| Area | Status | Main API |
|---|---|---|
| Packet encode/decode | Generated schemas for all known packet IDs from 1.8 through 1.21.11 | `PacketRegistry`, `ProtocolRuntime` |
| Cross-version packet keys | One logical key per packet across versions | `kprotocol::packet_keys`, `kprotocol::generated::packet_keys` |
| Version translation | Implemented for common UI/block/demo flows; strict mode exposes missing rules | `PacketTranslator`, `TranslationRegistry` |
| NBT and field types | Real NBT blobs plus typed scalar, array, slot, UUID, position support | `FieldType`, `NBTBlob`, `NBTNamedTag` |
| Login/configuration/play states | Modern 1.20.2+ configuration phase is tracked | `MinecraftServer`, `configuration.hpp` |
| Runtime guardrails | Connection limits, frame buffer cap, idle/handshake timeouts, packet/byte rate caps | `ServerRuntimeOptions` |
| Install/export | CMake package exports public targets and installed consumer smoke test verifies `find_package` | `kprotocolConfig.cmake` |

### Boundaries

KProtocol is a protocol library and lightweight server runtime, not a complete
game server. It does not implement world/chunk simulation, permissions, 
persistence, inventory semantics, or full gameplay rules. 
Generated packets cover wire IDs and field shapes; complex packet semantics 
still need application-level validation and translation rules.

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
npm ci
npm run generate
cmake --preset release
cmake --build --preset release
ctest --preset release
```

For a network-free CMake configure, provide a local Asio checkout and use the
offline preset:

```bash
cmake --preset release-offline -DKPROTOCOL_ASIO_SOURCE_DIR=/path/to/asio
cmake --build --preset release-offline
ctest --preset release-offline
```

`KPROTOCOL_BUILD_GENERATED_PACKETS=ON` now fails fast when `generated/` is
missing or incomplete. Use `npm ci && npm run generate` to recreate it from the
locked `minecraft-data` package, or pass
`-DKPROTOCOL_BUILD_GENERATED_PACKETS=OFF` for a baseline-only build.

By default the library compiles the committed multi-version packet catalog under
`generated/` (packet keys across every known catalog version from 1.8 through
1.21.11, wire 47–774). Unknown future client wires (e.g. 1.26 when
minecraft-data adds it) resolve to the nearest compiled catalog anchor until you
regenerate.
Disable it with `-DKPROTOCOL_BUILD_GENERATED_PACKETS=OFF` if you only need the
hand-rolled baseline set.

### CMake options

| Option | Default | Notes |
|---|---:|---|
| `KPROTOCOL_BUILD_TESTS` | `ON` | Builds unit, integration, and install-consumer smoke tests. |
| `KPROTOCOL_BUILD_EXAMPLES` | `OFF` | Builds `examples/minimal_server.cpp` and `examples/play_demo_server.cpp`. |
| `KPROTOCOL_BUILD_GENERATED_PACKETS` | `ON` | Compiles the generated catalog; fails fast if `generated/` is incomplete. |
| `KPROTOCOL_FETCH_ASIO` | `ON` | Allows CMake to fetch standalone Asio when no local source path is supplied. |
| `KPROTOCOL_ASIO_SOURCE_DIR` | empty | Points to a local Asio checkout containing `asio/include`. |
| `KPROTOCOL_BUILD_REAL_CLIENT_TESTS` | `OFF` | Enables opt-in tests that run an external Minecraft client/bot command. |
| `KPROTOCOL_REAL_CLIENT_CMD` | empty | Command template used by the opt-in real-client test. |

### Regenerating the packet catalog

See [GENERATE.md](GENERATE.md) for full details. Quick version:

```bash
npm ci
npm run generate
npm run check:coverage
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
cmake --install build/release --prefix /path/to/kprotocol/install
cmake -S examples/consumer -B build/consumer \
  -DCMAKE_PREFIX_PATH=/path/to/kprotocol/install
cmake --build build/consumer
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

Installed packages export the same public targets as the in-tree build:
`kprotocol::kprotocol`, `kprotocol::codec`, `kprotocol::core`,
`kprotocol::packets`, `kprotocol::server`, and `kprotocol::asio`. Standalone
Asio headers used by the server runtime are installed with the package, so
consumers do not need a separate Asio checkout just to use `kprotocol::server`.

The installed package also defines:

| Variable | Meaning |
|---|---|
| `kprotocol_DATA_DIR` | Installed `share/kprotocol` data directory. |
| `kprotocol_HAS_TRANSLATION_MAPPINGS` | True when the installed mapping blob is present. |
| `kprotocol_TRANSLATION_MAPPINGS` | Path to `translation_mappings.bin.gz` when installed. |

At runtime, block translation loads mappings from `KPROTOCOL_TRANSLATION_MAPPINGS`
when that environment variable is set, then from the source-tree data path for
in-tree builds, and finally from installed layouts such as
`<prefix>/share/kprotocol/translation_mappings.bin.gz` relative to the
executable. Applications with custom layouts can call
`TranslationRegistry::set_block_mappings_path(path)` and verify it with
`TranslationRegistry::block_mappings_available()`.

The default test suite includes `kprotocol_install_consumer_smoke`, which
installs into a temporary prefix and builds `examples/consumer` through
`find_package(kprotocol CONFIG REQUIRED)`.

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

### Real-client integration tests

Unit and loopback tests run by default. Real Minecraft client tests are opt-in
because they need an external client/bot runner and usually require assets or
credentials that do not belong in CI.

Configure CMake with `KPROTOCOL_BUILD_REAL_CLIENT_TESTS=ON` and provide a
client command. KProtocol starts `kprotocol_play_demo`, waits for TCP readiness,
then runs the command once per configured version:

```bash
cmake -S . -B build \
  -DKPROTOCOL_BUILD_REAL_CLIENT_TESTS=ON \
  -DKPROTOCOL_REAL_CLIENT_CMD="node ./my-real-client-check.mjs --host {host} --port {port} --version {version}" \
  -DKPROTOCOL_REAL_CLIENT_VERSIONS="1.8:47,1.21.1:767"

cmake --build build --target kprotocol_play_demo
ctest --test-dir build -R kprotocol_real_client_smoke --output-on-failure
```

The command receives placeholders (`{host}`, `{port}`, `{version}`, `{wire}`)
and matching environment variables (`KPROTOCOL_REAL_CLIENT_HOST`,
`KPROTOCOL_REAL_CLIENT_PORT`, `KPROTOCOL_REAL_CLIENT_VERSION`,
`KPROTOCOL_REAL_CLIENT_WIRE`). The client runner should connect in offline mode,
complete login/configuration as needed, verify it receives the play demo packets,
and exit `0` on success.

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
   - Login/security helpers cover encryption request/response packets,
     verify-token generation/checking, username validation, and Set Compression.
   - Sends packets translated to each client version.
   - Runtime guardrails include connection limits, bounded inbound frame
     buffering, optional idle/handshake timeouts, optional inbound packet/byte
     rate caps, disconnect callbacks, close-on-packet-error behavior, and
     optional strict translation checks.
   - Supports `ProtocolListener` hooks (`onPacketReceived`, `onPacketSent`,
     `onDisconnect`, `onError`).
   - Tracks modern login -> configuration -> play transitions, including
     `login_acknowledged` and `configuration.finish_configuration`.
   - Exposes a lightweight `GameplaySession` per client for profile, entity id,
     gamemode, dimension, position, and login/config/play lifecycle flags.

4. `kprotocol::ProtocolRuntime` and `kprotocol::ProtocolServer`
   - `ProtocolRuntime` owns an initialized registry/translator pair.
   - `ProtocolServer` combines `ProtocolRuntime` with `MinecraftServer` for
     the common "start a protocol server" path.

## Test Matrix

Default CTest coverage includes codec/property tests, generated packet coverage,
versioned schema tests, NBT tests, translation tests, server smoke tests,
runtime guardrail tests, login/configuration flow tests, gameplay session tests,
play-demo integration tests, UI/block helper tests, and installed-consumer
packaging smoke tests.

Useful commands:

```bash
npm run check:coverage
ctest --preset release
ctest --preset release -R kprotocol_install_consumer_smoke --output-on-failure
docker build -t kprotocol-build .
```

Real-client tests are intentionally opt-in because they require an external
client runner and usually machine-specific assets or credentials.

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
runtime.handshake_timeout_ms = 10'000;   // optional; 0 disables
runtime.idle_timeout_ms = 120'000;       // optional; 0 disables
runtime.max_packets_per_second = 200;    // optional; 0 disables
runtime.max_bytes_per_second = 2'000'000;// optional; 0 disables
runtime.disconnect_on_packet_error = true;
runtime.require_explicit_translations = true;
server.set_runtime_options(runtime);
```

Gameplay handlers can use the session model as their durable per-player state:

```cpp
server.on_packet([](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
    if (packet.key == kprotocol::packet_keys::login_start) {
        auto state = client.gameplay_session();
        client.set_entity_id(1);
        client.set_game_mode(kprotocol::GameMode::survival);
        client.set_dimension("minecraft:overworld");
        client.set_location({.x = 0.0, .y = 64.0, .z = 0.0, .known = true});
        client.mark_joined_game();
    }
});
```

Modern clients (1.20.2+) enter a configuration phase before play. KProtocol has
typed packets and helpers for the common phase boundary:

```cpp
server.on_packet([](const kprotocol::ClientSession& client, const kprotocol::Packet& packet) {
    if (packet.key_matches(kprotocol::packet_keys::login_acknowledged)) {
        kprotocol::send_feature_flags(client);
        kprotocol::send_configuration_finish(client);
    }
    if (packet.key_matches(kprotocol::packet_keys::configuration_finish_serverbound)) {
        kprotocol::send_play_demo(client, kprotocol::ProtocolVersion::v1_21_1);
    }
});
```

For registry payloads, use `SRegistryDataPacket`: `codec` is used by 1.20.2
style schemas, while `id` + `entries` is used by newer registry-data packets.
Applications that need a real client to enter a world must still send
version-appropriate registry/tag/join packets with semantically valid contents;
the helper covers the phase boundary, not world bootstrap policy.

Login/security helpers are available when implementing online-mode or custom
authentication flows:

```cpp
#include "kprotocol/login_security.hpp"

auto challenge = kprotocol::LoginSecurityChallenge{
    .server_id = "",
    .public_key = public_key_der,
    .verify_token = kprotocol::generate_verify_token(),
};

kprotocol::send_login_encryption_request(client, challenge);
// After decrypting the client's C01EncryptionResponsePacket verifyToken:
// kprotocol::verify_login_token(response, challenge.verify_token)
kprotocol::send_login_set_compression(client, 256);
```

`send_login_encryption_request` and `verify_login_token` help with the packet
and token pieces of online-mode login. They do not contact Mojang services or
decrypt RSA payloads for you; applications own private-key handling, session
server verification, profile properties, bans, and rate-limit policy.

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

## License

KProtocol is distributed under the MIT License. See [LICENSE](LICENSE).
