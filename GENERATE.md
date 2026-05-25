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

# 2. Regenerate. Versions must be a comma-separated list of values that
#    appear in kprotocol's KPROTOCOL_FOR_EACH_KNOWN_VERSION (see
#    include/kprotocol/version.hpp).
node tools/generate_packets.mjs \
    --out generated \
    --versions 1.8,1.12.2,1.13,1.14,1.16.5,1.17,1.18,1.19,1.20.2,1.20.4,1.21.1,1.21.4,1.21.5
```

The CMake build exposes a convenience target that performs the same step:

```bash
cmake --build <build-dir> --target kprotocol_generate_packets
```

The target uses the versions listed in the `KPROTOCOL_GENERATE_VERSIONS`
CMake cache variable (default: `1.8,1.12.2,1.16.5,1.20.4,1.21.1`).

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
| `restBuffer` | `rest_buffer` |

Any packet whose container references compound types (`switch`, `array`,
`option`, nested `container`, `mapper`, ...) cannot be fully modeled in
Wave 2. The generator falls back to a single `rest_buffer` field named
`raw` so the packet can still round-trip byte-exact as an opaque blob. These
entries are flagged `rawOnly` in `coverage.json`; a later wave will extend
`FieldType` with compound primitives so coverage can grow.

## Disabling the committed catalog

If you only need the hand-rolled baseline packets and want to keep your link
units small, pass `-DKPROTOCOL_BUILD_GENERATED_PACKETS=OFF`. The generator
output is then excluded from the build and the `register_generated_packets`
symbol is unavailable.

## Licensing

The upstream data is MIT-licensed by PrismarineJS. The generator script
itself (`tools/generate_packets.mjs`) is part of kprotocol and follows the
project license.

## Block ID translation tables

Block state remapping tables live in `tools/translation_mappings.json`.
Regenerate from minecraft-data (by block name bridge):

```bash
node tools/generate_translation_mappings.mjs
node tools/embed_translation_mappings.mjs
```

The embedded copy in `src/translation_mappings_data.cpp` is loaded automatically
when `kprotocol::initialize()` runs.
