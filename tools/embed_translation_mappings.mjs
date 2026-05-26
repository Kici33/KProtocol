#!/usr/bin/env node
// Embeds tools/translation_mappings.json as a compact binary blob for runtime lookup.
import { promises as fs } from 'node:fs';
import path from 'node:path';
import process from 'node:process';
import { gzipSync } from 'node:zlib';

const MAGIC = 0x4d42504b; // 'KPBM' little-endian
const FORMAT_VERSION = 1;

// Must match KPROTOCOL_FOR_EACH_KNOWN_VERSION order in include/kprotocol/version.hpp
// (same list as tools/generate_packets.mjs). known_index == static_cast<uint16_t>(KnownVersion).
const VERSION_TABLE = [
    { display: '1.8',     enumerator: 'v1_8' },
    { display: '1.9',     enumerator: 'v1_9' },
    { display: '1.9.2',   enumerator: 'v1_9_2' },
    { display: '1.9.4',   enumerator: 'v1_9_4' },
    { display: '1.10',    enumerator: 'v1_10' },
    { display: '1.11',    enumerator: 'v1_11' },
    { display: '1.11.2',  enumerator: 'v1_11_2' },
    { display: '1.12',    enumerator: 'v1_12' },
    { display: '1.12.1',  enumerator: 'v1_12_1' },
    { display: '1.12.2',  enumerator: 'v1_12_2' },
    { display: '1.13',    enumerator: 'v1_13' },
    { display: '1.13.2',  enumerator: 'v1_13_2' },
    { display: '1.14',    enumerator: 'v1_14' },
    { display: '1.14.4',  enumerator: 'v1_14_4' },
    { display: '1.15',    enumerator: 'v1_15' },
    { display: '1.15.2',  enumerator: 'v1_15_2' },
    { display: '1.16',    enumerator: 'v1_16' },
    { display: '1.16.2',  enumerator: 'v1_16_2' },
    { display: '1.16.5',  enumerator: 'v1_16_5' },
    { display: '1.17',    enumerator: 'v1_17' },
    { display: '1.17.1',  enumerator: 'v1_17_1' },
    { display: '1.18',    enumerator: 'v1_18' },
    { display: '1.18.2',  enumerator: 'v1_18_2' },
    { display: '1.19',    enumerator: 'v1_19' },
    { display: '1.19.2',  enumerator: 'v1_19_2' },
    { display: '1.19.3',  enumerator: 'v1_19_3' },
    { display: '1.19.4',  enumerator: 'v1_19_4' },
    { display: '1.20',    enumerator: 'v1_20' },
    { display: '1.20.2',  enumerator: 'v1_20_2' },
    { display: '1.20.4',  enumerator: 'v1_20_4' },
    { display: '1.20.5',  enumerator: 'v1_20_5' },
    { display: '1.21.1',  enumerator: 'v1_21_1' },
    { display: '1.21.3',  enumerator: 'v1_21_3' },
    { display: '1.21.4',  enumerator: 'v1_21_4' },
    { display: '1.21.5',  enumerator: 'v1_21_5' },
    { display: '1.21.6',  enumerator: 'v1_21_6' },
    { display: '1.21.7',  enumerator: 'v1_21_7' },
    { display: '1.21.9',  enumerator: 'v1_21_9' },
    { display: '1.21.11', enumerator: 'v1_21_11' },
];

VERSION_TABLE.forEach((row, index) => {
    row.known_index = index;
});

const DISPLAY_TO_KNOWN = new Map(VERSION_TABLE.map(v => [v.display, v.known_index]));

function parsePairKey(key) {
    const idx = key.indexOf('_to_');
    if (idx < 0) return null;
    return { from: key.slice(0, idx), to: key.slice(idx + 4) };
}

function writeU32(view, offset, value) {
    view.setUint32(offset, value, true);
}

function writeU16(view, offset, value) {
    view.setUint16(offset, value, true);
}

function buildBinary(data) {
    const pairs = [];
    for (const [key, mapping] of Object.entries(data)) {
        const parsed = parsePairKey(key);
        if (!parsed) continue;
        const fromIdx = DISPLAY_TO_KNOWN.get(parsed.from);
        const toIdx = DISPLAY_TO_KNOWN.get(parsed.to);
        if (fromIdx === undefined || toIdx === undefined) continue;
        const entries = Object.entries(mapping)
            .map(([from, to]) => ({ from: Number(from), to: Number(to) }))
            .sort((a, b) => a.from - b.from);
        pairs.push({ fromIdx, toIdx, entries });
    }
    pairs.sort((a, b) => (a.fromIdx - b.fromIdx) || (a.toIdx - b.toIdx));

    const headerBytes = 16;
    const directoryBytes = pairs.length * 12;
    let entryBytes = 0;
    for (const p of pairs) entryBytes += p.entries.length * 8;

    const buffer = new ArrayBuffer(headerBytes + directoryBytes + entryBytes);
    const view = new DataView(buffer);
    const u8 = new Uint8Array(buffer);

    writeU32(view, 0, MAGIC);
    writeU32(view, 4, FORMAT_VERSION);
    writeU32(view, 8, pairs.length);
    writeU32(view, 12, 0);

    let entryOffset = 0;
    let dirOffset = headerBytes;
    const entriesStart = headerBytes + directoryBytes;

    for (const pair of pairs) {
        writeU16(view, dirOffset, pair.fromIdx);
        writeU16(view, dirOffset + 2, pair.toIdx);
        writeU32(view, dirOffset + 4, entryOffset);
        writeU32(view, dirOffset + 8, pair.entries.length);
        dirOffset += 12;

        let off = entriesStart + entryOffset;
        for (const e of pair.entries) {
            view.setInt32(off, e.from, true);
            view.setInt32(off + 4, e.to, true);
            off += 8;
        }
        entryOffset += pair.entries.length * 8;
    }

    return { buffer: u8, pairs: pairs.length, entries: entryBytes / 8 };
}

async function main() {
    const root = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1')), '..');
    const input = path.join(root, 'tools', 'translation_mappings.json');
    const dataDir = path.join(root, 'data');
    const binPath = path.join(dataDir, 'translation_mappings.bin');
    const gzPath = path.join(dataDir, 'translation_mappings.bin.gz');

    const raw = await fs.readFile(input, 'utf8');
    const data = JSON.parse(raw);
    const { buffer, pairs, entries } = buildBinary(data);

    await fs.mkdir(dataDir, { recursive: true });
    await fs.writeFile(binPath, buffer);
    const gz = gzipSync(buffer, { level: 9 });
    await fs.writeFile(gzPath, gz);

    console.log(`Wrote ${binPath} (${buffer.length} bytes, ${pairs} pairs, ${entries} entries).`);
    console.log(`Wrote ${gzPath} (${gz.length} bytes, ${(100 * gz.length / buffer.length).toFixed(1)}% of raw).`);
}

main().catch(err => {
    console.error(err);
    process.exit(1);
});
