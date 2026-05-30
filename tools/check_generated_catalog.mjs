#!/usr/bin/env node
// SPDX-License-Identifier: MIT

import { promises as fs } from 'node:fs';
import process from 'node:process';

const root = process.argv[2] || '.';
const coveragePath = `${root}/generated/coverage.json`;
const keysHeaderPath = `${root}/generated/include/kprotocol/generated/packet_keys.hpp`;
const registryPath = `${root}/generated/registry/register_all_packets.cpp`;

function fail(message) {
    console.error(`Generated catalog check failed: ${message}`);
    process.exit(1);
}

function sanitizeIdentifier(key) {
    return key.replace(/[^A-Za-z0-9_]/g, '_');
}

function sortedCopy(values) {
    return [...values].sort((a, b) => a.localeCompare(b));
}

function sameSet(left, right) {
    if (left.size !== right.size) {
        return false;
    }
    for (const value of left) {
        if (!right.has(value)) {
            return false;
        }
    }
    return true;
}

function firstDifference(left, right) {
    const missing = sortedCopy([...left].filter(value => !right.has(value))).slice(0, 8);
    const extra = sortedCopy([...right].filter(value => !left.has(value))).slice(0, 8);
    return { missing, extra };
}

const [coverageText, keysHeader, registrySource] = await Promise.all([
    fs.readFile(coveragePath, 'utf8'),
    fs.readFile(keysHeaderPath, 'utf8'),
    fs.readFile(registryPath, 'utf8'),
]);

const coverage = JSON.parse(coverageText);
const versions = coverage.versions || [];
const keys = coverage.keys || [];

if (versions.length === 0) {
    fail('coverage.versions is empty');
}
if (keys.length === 0) {
    fail('coverage.keys is empty');
}

const seenWires = new Set();
let previousWire = -Infinity;
for (const version of versions) {
    if (typeof version.display !== 'string' || version.display.length === 0) {
        fail('a version row has no display name');
    }
    if (!Number.isInteger(version.wire)) {
        fail(`version ${version.display} has non-integer wire`);
    }
    if (version.wire <= previousWire) {
        fail(`version wires are not strictly increasing near ${version.display}`);
    }
    previousWire = version.wire;
    if (seenWires.has(version.wire)) {
        fail(`duplicate wire ${version.wire}`);
    }
    seenWires.add(version.wire);

    const total = version.totalPackets || 0;
    const full = version.fullSchemas || 0;
    const raw = version.rawOnly || 0;
    const unsupported = version.unsupported || 0;
    if (total <= 0) {
        fail(`version ${version.display} has no packets`);
    }
    if (full + raw + unsupported !== total) {
        fail(`version ${version.display} totals do not add up`);
    }
}

const coverageKeySet = new Set();
const identifierSet = new Set();
const versionTotals = new Map(versions.map(version => [String(version.wire), {
    total: 0,
    full: 0,
    raw: 0,
    unsupported: 0,
}]));

for (const row of keys) {
    if (typeof row.key !== 'string' || row.key.length === 0) {
        fail('a key row has no packet key');
    }
    if (coverageKeySet.has(row.key)) {
        fail(`duplicate packet key ${row.key}`);
    }
    coverageKeySet.add(row.key);

    const identifier = sanitizeIdentifier(row.key);
    if (!/^[A-Za-z_][A-Za-z0-9_]*$/.test(identifier)) {
        fail(`packet key ${row.key} does not sanitize to a valid C++ identifier`);
    }
    if (identifierSet.has(identifier)) {
        fail(`duplicate generated C++ identifier ${identifier}`);
    }
    identifierSet.add(identifier);

    const ids = row.ids || {};
    const statuses = row.statusByWire || {};
    if (Object.keys(ids).length === 0) {
        fail(`packet key ${row.key} has no wire IDs`);
    }
    for (const [wire, id] of Object.entries(ids)) {
        if (!seenWires.has(Number(wire))) {
            fail(`packet key ${row.key} references unknown wire ${wire}`);
        }
        if (!Number.isInteger(id) || id < 0) {
            fail(`packet key ${row.key} has invalid ID ${id} for wire ${wire}`);
        }
    }
    for (const [wire, status] of Object.entries(statuses)) {
        const totals = versionTotals.get(wire);
        if (!totals) {
            fail(`packet key ${row.key} references unknown status wire ${wire}`);
        }
        totals.total += 1;
        if (status === 'ok') {
            totals.full += 1;
        } else if (status === 'rawOnly') {
            totals.raw += 1;
        } else if (status === 'unsupported') {
            totals.unsupported += 1;
        } else {
            fail(`packet key ${row.key} has unknown status ${status}`);
        }
    }
}

for (const version of versions) {
    const totals = versionTotals.get(String(version.wire));
    if (totals.total !== version.totalPackets ||
            totals.full !== version.fullSchemas ||
            totals.raw !== version.rawOnly ||
            totals.unsupported !== version.unsupported) {
        fail(`per-key status counts do not match totals for ${version.display}`);
    }
}

const headerKeyMatches = [...keysHeader.matchAll(
    /inline constexpr std::string_view\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*"([^"]+)";/g,
)];
const headerKeySet = new Set(headerKeyMatches.map(match => match[2]));
const headerIdentifierSet = new Set(headerKeyMatches.map(match => match[1]));

if (headerKeyMatches.length !== headerKeySet.size) {
    fail('generated packet_keys.hpp contains duplicate key values');
}
if (headerKeyMatches.length !== headerIdentifierSet.size) {
    fail('generated packet_keys.hpp contains duplicate identifiers');
}
if (!sameSet(coverageKeySet, headerKeySet)) {
    const { missing, extra } = firstDifference(coverageKeySet, headerKeySet);
    fail(`packet_keys.hpp does not match coverage keys; missing=${missing.join(',')} extra=${extra.join(',')}`);
}

for (const key of coverageKeySet) {
    const identifier = sanitizeIdentifier(key);
    if (!headerIdentifierSet.has(identifier)) {
        fail(`packet_keys.hpp missing identifier ${identifier} for ${key}`);
    }
}

const registryKeySet = new Set(
    [...registrySource.matchAll(/schema\.key\s*=\s*"([^"]+)";/g)].map(match => match[1]),
);
if (!sameSet(coverageKeySet, registryKeySet)) {
    const { missing, extra } = firstDifference(coverageKeySet, registryKeySet);
    fail(`register_all_packets.cpp does not match coverage keys; missing=${missing.join(',')} extra=${extra.join(',')}`);
}

if (!/void\s+register_generated_packets\s*\(\s*PacketRegistry&\s+registry\s*\)/.test(registrySource)) {
    fail('register_all_packets.cpp does not define register_generated_packets(PacketRegistry&)');
}

console.log(
    `Generated catalog ok: ${versions.length} versions, ${coverageKeySet.size} packet keys`,
);
