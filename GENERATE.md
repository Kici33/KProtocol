# Generating the multi-version packet catalog

The `kprotocol` library ships with a generator-first packet catalog. The full
catalog under `generated/` is auto-produced from
[PrismarineJS/minecraft-data](https://github.com/PrismarineJS/minecraft-data)
and committed to the repository so that consumers can build immediately
without Node.js.

## What gets generated

The generator emits three artifacts under the `--out` directory:

- `include/kprotocol/generated/packet_keys.hpp` - one
  `inline constexpr std::string_view` per packet key, so users can avoid
  stringly-typed lookups (e.g.
  `kprotocol::generated::packet_keys::login_serverbound_encryption_begin`).
- `registry/register_all_packets.cpp` - implementation of
  `kprotocol::register_generated_packets(PacketRegistry&)`. Call this once at
  startup to register every supported (state, direction, version) tuple.
- `coverage.json` - per-version stats and per-key status (`ok` vs `rawOnly`).
  Useful for tracking how much of the wire format is fully typed vs handled
  as an opaque `rest_buffer` blob.

## Regenerating

Prerequisites: Node.js 18+ on PATH.

```bash
# 1. Install the upstream data package (one-time)
npm install minecraft-data

# 2. Regenerate every canonical version in
#    KPROTOCOL_FOR_EACH_KNOWN_VERSION (see include/kprotocol/version.hpp).
node tools/generate_packets.mjs \
    --out generated \
    --versions all-known
```

The CMake build exposes a convenience target that performs the same step:

```bash
cmake --build <build-dir> --target kprotocol_generate_packets
```

The target uses the versions listed in the `KPROTOCOL_GENERATE_VERSIONS`
CMake cache variable.

## Type mapping

The generator translates minecraft-data's protocol types into kprotocol's
`FieldType` enum. Primitives map directly:

| minecraft-data | kprotocol FieldType   |
|---|---|
| `varint`, `varlong`, `bool`, `string` | `var_int`, `var_long`, `boolean`, `string` |
| `u8`, `i8`, `u16`, `i16`, `u32`, `i32`, `u64`, `i64` | `u8`, `i8`, `u16_be`, `i16_be`, `u32_be`, `i32_be`, `u64_be`, `i64_be` |
| `f32`, `f64` | `f32_be`, `f64_be` |
| `UUID`, `position` | `uuid`, `position` |
| `[buffer, {countType: varint}]` | `byte_array` |
| `[array, {countType: varint, type: ...}]` for selected primitives | `var_int_array`, `var_long_array`, `i64_array`, `string_array`, `uuid_array`, `slot_array`, or `byte_array` |
| `restBuffer` | `rest_buffer` |

Simple `switch` fields whose branch is selected by an integer field are emitted
as conditional `FieldSpec` entries. For example, a boss-bar `title` field can be
typed but only read/written when `action` is `0` or `3`.
`bitflags` and `mapper` fields are emitted as their underlying integer type.
Optional scalar values become a `*_present` boolean plus an optional value field;
optional containers are flattened into prefixed optional fields when their inner
fields are mappable.

Packets that still require unsupported compound shapes (`mapper`, deep nested
containers, chunk data, complex NBT, etc.) fall back to a single `rest_buffer`
field named `raw` so the packet can still round-trip byte-exact as an opaque
blob. These entries are flagged `rawOnly` in `coverage.json`.

## Disabling the committed catalog

If you only need the hand-rolled baseline packets and want to keep your link
units small, pass `-DKPROTOCOL_BUILD_GENERATED_PACKETS=OFF`. The generator
output is then excluded from the build and the `register_generated_packets`
symbol is unavailable.

## Licensing

The upstream data is MIT-licensed by PrismarineJS. The generator script
itself (`tools/generate_packets.mjs`) is part of kprotocol and follows the
project license.

## Protocol version types

KProtocol uses three related types (see `include/kprotocol/version.hpp`):

| Type | Role |
|---|---|
| `WireProtocol` | Raw Mojang protocol number from the handshake (`protocol_version` field). May be unknown (future client). |
| `KnownVersion` | Dense catalog index (`0` … `count-1`). Keys generated `PacketSchema` maps (`field_sets`, `ids`). |
| `ProtocolVersion` | Legacy enum whose enumerator **value equals the wire number**. Kept for existing APIs; prefer `KnownVersion` + `WireProtocol` in new code. |

Session path: `ClientSession::client_wire()` returns the announced wire;
`protocol_version()` returns the **catalog anchor** (`catalog_anchor_for`) used
for registry encode/decode. Handshake handling stores `WireProtocol` only.

Helpers: `to_known_version`, `catalog_anchor_known_for`, `try_from_wire`.

## Block ID translation tables

Block state remapping tables live in `tools/translation_mappings.json`.
Regenerate from minecraft-data (by block name bridge):

```bash
node tools/generate_translation_mappings.mjs
npm run embed:translation
node tools/embed_block_registry.mjs
node tools/embed_metadata_registry.mjs
```

`embed:translation` writes `data/translation_mappings.bin.gz` (~7.5 MB, gzip).
Commit the `.bin.gz` only; the raw `.bin` is gitignored. At runtime
`translation_mappings_loader.cpp` gunzips the blob and binary-searches
`(KnownVersion from, KnownVersion to, block id)` — pair indices match
`KPROTOCOL_FOR_EACH_KNOWN_VERSION` order.

CMake defines `KPROTOCOL_BLOCK_MAPPINGS_GZ` when the file is present and
installs it under `share/kprotocol/`.
