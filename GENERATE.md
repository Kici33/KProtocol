Generating full packet catalog (1.8 → latest)

This repository includes a generator script that uses PrismarineJS/minecraft-data to emit
C++ typed packet classes and packet registry mappings across multiple Java Edition versions.

Prerequisites
- Node.js and npm installed

Generate steps
1. Install dependency:
   npm install minecraft-data
2. Run the generator (example covering major versions):
   node generate_packets.js --out generated --versions 1.8,1.12.2,1.16.5,1.20.4,1.21.1

What it does
- Produces a `generated/` tree with `packets/` (C++ header stubs) and `registry/register_all_packets.cpp` (registration skeleton).
- The output requires manual verification: field encodings vary across protocol versions and must be mapped correctly.

Integration
- After generation, inspect `generated/packets/*.hpp` and expand typed fields and conversion logic.
- Merge generated headers into `include/kprotocol/` and generated registration code into `src/` or call `register_generated_packets(registry)` from your initialization code.

Notes
- PrismarineJS/minecraft-data is MIT-licensed. Some other sources (wiki.vg) are under CC BY-SA which may affect redistribution.
- This generator is a best-effort scaffold — automated generation will save time but manual review is required to ensure correctness across all protocol versions.
