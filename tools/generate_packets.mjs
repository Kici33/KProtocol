#!/usr/bin/env node
// SPDX-License-Identifier: MIT
//
// kprotocol multi-version packet catalog generator.
//
// Reads PrismarineJS/minecraft-data (`npm install minecraft-data`) for one or
// more Minecraft Java Edition versions and emits a C++ source tree that
// registers PacketSchema entries with kprotocol::PacketRegistry.
//
// Usage:
//   node tools/generate_packets.mjs \
//     --out generated \
//     --versions 1.8,1.12.2,1.16.5,1.20.4,1.21.1
//
// Outputs (relative to --out):
//   include/kprotocol/generated/packet_keys.hpp   -- one constexpr per packet
//   registry/register_all_packets.cpp             -- registry registration body
//   coverage.json                                 -- per-version coverage stats
//
// Mapping strategy:
//   - All primitive minecraft-data types map to a kprotocol FieldType.
//   - Packets containing only mappable primitives are emitted with typed field
//     sets per version.
//   - Packets with complex suffixes keep their typed prefix and preserve the
//     remaining bytes as a named `tail` rest_buffer, so the packet key is still
//     registered instead of omitted.
//   - Packets missing a minecraft-data schema are registered with a named
//     `payload` rest_buffer instead of a whole-packet `raw` fallback.
//   - Use --raw-policy keep to restore the old opaque `raw` fallback, or
//     --raw-policy fail to stop generation at the first unmodeled packet.

import { promises as fs } from 'node:fs';
import path from 'node:path';
import process from 'node:process';

// ---------------------------------------------------------------------------
// Arg parsing
// ---------------------------------------------------------------------------

function parseArgs(argv) {
    const args = { out: 'generated', versions: [], rawPolicy: 'omit' };
    for (let i = 0; i < argv.length; ++i) {
        const a = argv[i];
        if (a === '--out')           args.out = argv[++i];
        else if (a === '--versions') args.versions = argv[++i].split(',').map(s => s.trim()).filter(Boolean);
        else if (a === '--raw-policy') args.rawPolicy = argv[++i];
        else if (a === '--help' || a === '-h') {
            console.log('Usage: node tools/generate_packets.mjs --out <dir> --versions v1,v2,...');
            console.log('       node tools/generate_packets.mjs --out <dir> --versions all-known');
            console.log('       --raw-policy omit|keep|fail  (default: omit)');
            process.exit(0);
        } else {
            console.error(`Unknown argument: ${a}`);
            process.exit(2);
        }
    }
    if (args.versions.length === 0) {
        console.error('No --versions supplied.');
        process.exit(2);
    }
    if (!['omit', 'keep', 'fail'].includes(args.rawPolicy)) {
        console.error(`Invalid --raw-policy "${args.rawPolicy}". Expected omit, keep, or fail.`);
        process.exit(2);
    }
    return args;
}

let RAW_POLICY = 'omit';

// ---------------------------------------------------------------------------
// Wire-version table: must mirror KPROTOCOL_FOR_EACH_KNOWN_VERSION in
// include/kprotocol/version.hpp. Update both at once when adding a new
// supported version. The "wire" number is what we emit as the int32 key.
// ---------------------------------------------------------------------------

const VERSION_TABLE = [
    { display: '1.8',    wire: 47,  enumerator: 'v1_8' },
    { display: '1.9',    wire: 107, enumerator: 'v1_9' },
    { display: '1.9.2',  wire: 109, enumerator: 'v1_9_2' },
    { display: '1.9.4',  wire: 110, enumerator: 'v1_9_4' },
    { display: '1.10',   wire: 210, enumerator: 'v1_10' },
    { display: '1.11',   wire: 315, enumerator: 'v1_11' },
    { display: '1.11.2', wire: 316, enumerator: 'v1_11_2' },
    { display: '1.12',   wire: 335, enumerator: 'v1_12' },
    { display: '1.12.1', wire: 338, enumerator: 'v1_12_1' },
    { display: '1.12.2', wire: 340, enumerator: 'v1_12_2' },
    { display: '1.13',   wire: 393, enumerator: 'v1_13' },
    { display: '1.13.2', wire: 404, enumerator: 'v1_13_2' },
    { display: '1.14',   wire: 477, enumerator: 'v1_14' },
    { display: '1.14.4', wire: 498, enumerator: 'v1_14_4' },
    { display: '1.15',   wire: 573, enumerator: 'v1_15' },
    { display: '1.15.2', wire: 578, enumerator: 'v1_15_2' },
    { display: '1.16',   wire: 735, enumerator: 'v1_16' },
    { display: '1.16.2', wire: 751, enumerator: 'v1_16_2' },
    { display: '1.16.5', wire: 754, enumerator: 'v1_16_5' },
    { display: '1.17',   wire: 755, enumerator: 'v1_17' },
    { display: '1.17.1', wire: 756, enumerator: 'v1_17_1' },
    { display: '1.18',   wire: 757, enumerator: 'v1_18' },
    { display: '1.18.2', wire: 758, enumerator: 'v1_18_2' },
    { display: '1.19',   wire: 759, enumerator: 'v1_19' },
    { display: '1.19.2', wire: 760, enumerator: 'v1_19_2' },
    { display: '1.19.3', wire: 761, enumerator: 'v1_19_3' },
    { display: '1.19.4', wire: 762, enumerator: 'v1_19_4' },
    { display: '1.20',   wire: 763, enumerator: 'v1_20' },
    { display: '1.20.2', wire: 764, enumerator: 'v1_20_2' },
    { display: '1.20.4', wire: 765, enumerator: 'v1_20_4' },
    { display: '1.20.5', wire: 766, enumerator: 'v1_20_5' },
    { display: '1.21',   wire: 767, enumerator: 'v1_21_1' }, // 1.21 & 1.21.1 share wire 767
    { display: '1.21.1', wire: 767, enumerator: 'v1_21_1' },
    { display: '1.21.3', wire: 768, enumerator: 'v1_21_3' },
    { display: '1.21.4', wire: 769, enumerator: 'v1_21_4' },
    { display: '1.21.5', wire: 770, enumerator: 'v1_21_5' },
    { display: '1.21.6', wire: 771, enumerator: 'v1_21_6' },
    { display: '1.21.7', wire: 772, enumerator: 'v1_21_7' },
    { display: '1.21.8', wire: 772, enumerator: 'v1_21_7' },
    { display: '1.21.9', wire: 773, enumerator: 'v1_21_9' },
    { display: '1.21.10', wire: 773, enumerator: 'v1_21_9' },
    { display: '1.21.11', wire: 774, enumerator: 'v1_21_11' },
];

function findVersion(display) {
    return VERSION_TABLE.find(v => v.display === display);
}

function canonicalVersionDisplays() {
    const displayFromEnumerator = (enumerator) => enumerator
        .replace(/^v/, '')
        .replace(/_/g, '.');
    return VERSION_TABLE
        .filter(row => row.display === displayFromEnumerator(row.enumerator))
        .map(row => row.display);
}

// Mojang wire number -> KnownVersion enumerator (last duplicate wire wins).
const WIRE_TO_ENUMERATOR = new Map();
for (const row of VERSION_TABLE) {
    WIRE_TO_ENUMERATOR.set(row.wire, row.enumerator);
}

function knownVersionExpr(wire) {
    const enumerator = WIRE_TO_ENUMERATOR.get(wire);
    if (!enumerator) {
        throw new Error(`No KnownVersion enumerator for wire ${wire}`);
    }
    return `KnownVersion::${enumerator}`;
}

// ---------------------------------------------------------------------------
// Type mapping. Keys are minecraft-data primitive type names.
// ---------------------------------------------------------------------------

const PRIMITIVE_MAP = {
    'varint':     'var_int',
    'varlong':    'var_long',
    'bool':       'boolean',
    'string':     'string',
    'u8':         'u8',
    'i8':         'i8',
    'u16':        'u16_be',
    'i16':        'i16_be',
    'u32':        'u32_be',
    'i32':        'i32_be',
    'u64':        'u64_be',
    'i64':        'i64_be',
    'f32':        'f32_be',
    'f64':        'f64_be',
    'UUID':       'uuid',
    'position':   'position',
    'restBuffer': 'rest_buffer',
    'void':       null, // switch arm with no payload
};

// Named minecraft-data aliases that map to a single kprotocol FieldType.
const ALIAS_MAP = {
    slot: 'slot',
    Slot: 'slot',
    anonOptionalNbt: 'optional_nbt',
    anonymousNbt: 'optional_nbt',
    optionalNbt: 'optional_nbt',
    nbt: 'optional_nbt',
    entityMetadata: 'rest_buffer',
};

// Fields whose name collides with C++ keywords or our own struct fields. We
// remap them to a safe alternative so users can interact via Packet.fields.
const FIELD_NAME_REMAP = {
    'default': 'default_value',
    'class':   'class_name',
    'new':     'new_value',
    'private': 'private_field',
};

function sanitizeFieldName(name) {
    return FIELD_NAME_REMAP[name] || name;
}

// ---------------------------------------------------------------------------
// minecraft-data introspection
// ---------------------------------------------------------------------------

// Resolve a type reference within the protocol JSON. minecraft-data nests its
// type aliases inside the state container as well as the global "types" map.
// We follow chained aliases (`{name: type}`) until we hit either a primitive
// string or a compound array ([kind, params]).
function resolveType(type, stateTypes, globalTypes) {
    let cur = type;
    const seen = new Set();
    while (typeof cur === 'string') {
        if (seen.has(cur)) return cur; // cycle - bail
        seen.add(cur);
        if (Object.prototype.hasOwnProperty.call(PRIMITIVE_MAP, cur)) {
            return cur;
        }
        if (Object.prototype.hasOwnProperty.call(ALIAS_MAP, cur)) {
            return cur;
        }
        if (Object.prototype.hasOwnProperty.call(stateTypes, cur)) {
            cur = stateTypes[cur];
            continue;
        }
        if (Object.prototype.hasOwnProperty.call(globalTypes, cur)) {
            cur = globalTypes[cur];
            continue;
        }
        return cur;
    }
    return cur;
}

// Map a (possibly resolved) field type to a kprotocol FieldType enumerator.
// Returns { ok: true, fieldType } on success, or { ok: false, reason } if the
// type is compound / unsupported.
function mapFieldType(type, stateTypes, globalTypes) {
    if (typeof type === 'string' && Object.prototype.hasOwnProperty.call(ALIAS_MAP, type)) {
        return { ok: true, fieldType: ALIAS_MAP[type] };
    }

    const resolved = resolveType(type, stateTypes, globalTypes);

    if (typeof resolved === 'string') {
        if (resolved in ALIAS_MAP) {
            return { ok: true, fieldType: ALIAS_MAP[resolved] };
        }
        if (typeof type === 'string' && resolved === 'native' && type in ALIAS_MAP) {
            return { ok: true, fieldType: ALIAS_MAP[type] };
        }
        const mapped = PRIMITIVE_MAP[resolved];
        if (mapped === null) {
            return { ok: true, fieldType: null, voidField: true };
        }
        if (mapped) return { ok: true, fieldType: mapped };
        return { ok: false, reason: `unmapped primitive: ${resolved}` };
    }

    if (Array.isArray(resolved)) {
        const [kind] = resolved;
        // Buffer with a count - emit as length-prefixed byte_array if the
        // count type is varint (which matches our byte_array on the wire).
        if (kind === 'buffer') {
            const params = resolved[1] || {};
            if (params.countType === 'varint') {
                return { ok: true, fieldType: 'byte_array' };
            }
            return { ok: false, reason: `unsupported buffer countType: ${params.countType}` };
        }
        if (kind === 'restBuffer') {
            return { ok: true, fieldType: 'rest_buffer' };
        }
        if (kind === 'array') {
            const params = resolved[1] || {};
            if ((params.countType || 'varint') !== 'varint') {
                return { ok: false, reason: `unsupported array countType: ${params.countType}` };
            }
            const elemMapped = mapFieldType(params.type, stateTypes, globalTypes);
            if (!elemMapped.ok) {
                return { ok: false, reason: `array element: ${elemMapped.reason}` };
            }
            if (elemMapped.fieldType === 'var_int') {
                return { ok: true, fieldType: 'var_int_array' };
            }
            if (elemMapped.fieldType === 'u8') {
                return { ok: true, fieldType: 'byte_array' };
            }
            if (elemMapped.fieldType === 'var_long') {
                return { ok: true, fieldType: 'var_long_array' };
            }
            if (elemMapped.fieldType === 'i64_be') {
                return { ok: true, fieldType: 'i64_array' };
            }
            if (elemMapped.fieldType === 'string') {
                return { ok: true, fieldType: 'string_array' };
            }
            if (elemMapped.fieldType === 'uuid') {
                return { ok: true, fieldType: 'uuid_array' };
            }
            if (elemMapped.fieldType === 'slot') {
                return { ok: true, fieldType: 'slot_array' };
            }
            if (elemMapped.fieldType === 'optional_nbt') {
                return { ok: true, fieldType: 'optional_nbt_array' };
            }
            if (elemMapped.fieldType === 'byte_array') {
                return { ok: true, fieldType: 'byte_array' };
            }
            return { ok: false, reason: `unsupported array element type: ${elemMapped.fieldType}` };
        }
        if (kind === 'option') {
            const optionPayload = resolved[1];
            if (optionPayload !== undefined && (!optionPayload.fields || typeof optionPayload === 'string' || Array.isArray(optionPayload))) {
                const innerMapped = mapFieldType(optionPayload, stateTypes, globalTypes);
                if (innerMapped.ok && innerMapped.fieldType) {
                    return {
                        ok: true,
                        expandOption: true,
                        innerType: innerMapped.fieldType,
                    };
                }
            }
            const params = optionPayload || {};
            const innerFields = params.fields || [];
            if (innerFields.length === 1) {
                const innerMapped = mapFieldType(innerFields[0], stateTypes, globalTypes);
                if (innerMapped.ok && innerMapped.fieldType) {
                    return {
                        ok: true,
                        expandOption: true,
                        innerType: innerMapped.fieldType,
                    };
                }
            }
            return { ok: false, reason: 'unsupported option shape' };
        }
        if (kind === 'bitfield') {
            return { ok: true, fieldType: 'u64_be' };
        }
        if (kind === 'bitflags' || kind === 'mapper') {
            const params = resolved[1] || {};
            const innerMapped = mapFieldType(params.type, stateTypes, globalTypes);
            if (innerMapped.ok && innerMapped.fieldType) {
                return { ok: true, fieldType: innerMapped.fieldType };
            }
            return { ok: false, reason: `${kind} underlying type: ${innerMapped.reason || 'unsupported'}` };
        }
        if (kind === 'container') {
            return { ok: false, reason: 'nested container field (not flattened)' };
        }
        if (kind === 'switch') {
            return { ok: false, reason: `switch type`, isSwitch: true };
        }
        if (kind === 'entityMetadataLoop') {
            return { ok: true, fieldType: 'rest_buffer' };
        }
        return { ok: false, reason: `compound type: ${kind}` };
    }

    return { ok: false, reason: 'unknown type shape' };
}

function looksLikeSlotContainer(containerFields) {
    if (!Array.isArray(containerFields) || containerFields.length < 2) {
        return false;
    }
    const presentField = containerFields[0];
    const switchField = containerFields[1];
    if (presentField?.name !== 'present' || presentField?.type !== 'bool') {
        return false;
    }
    const switchType = switchField?.type;
    return Array.isArray(switchType)
        && switchType[0] === 'switch'
        && switchType[1]?.compareTo === 'present'
        && switchType[1]?.fields?.false === 'void';
}

// Flatten a minecraft-data container field list into kprotocol FieldSpec entries.
// When `tailOnComplex` is true, fields that cannot be flattened (action switches,
// nested containers, etc.) are represented as a single trailing `rest_buffer` named
// `tail` so the packet can still round-trip.
function flattenContainerFields(containerFields, stateTypes, globalTypes, tailOnComplex = false) {
    const fields = [];
    const pushConditionalSwitchFields = (fieldName, params) => {
        if (!params.compareTo || params.default !== 'void' || !params.fields) {
            return { ok: false, reason: 'unsupported switch' };
        }

        const branchGroups = new Map();
        for (const [caseValue, branchType] of Object.entries(params.fields)) {
            if (branchType === 'void') {
                continue;
            }
            const mapped = mapFieldType(branchType, stateTypes, globalTypes);
            if (!mapped.ok || !mapped.fieldType || mapped.expandOption || mapped.voidField) {
                return { ok: false, reason: mapped.reason || 'unsupported switch branch' };
            }
            const parsedCaseValue = Number.parseInt(caseValue, 10);
            if (!Number.isFinite(parsedCaseValue)) {
                return { ok: false, reason: 'non-numeric switch case' };
            }
            const key = mapped.fieldType;
            const group = branchGroups.get(key) || {
                name: fieldName,
                type: mapped.fieldType,
                condition_field: sanitizeFieldName(params.compareTo),
                condition_values: [],
            };
            group.condition_values.push(parsedCaseValue);
            branchGroups.set(key, group);
        }

        if (branchGroups.size === 0) {
            return { ok: true, fields: [] };
        }
        return { ok: true, fields: [...branchGroups.values()] };
    };

    for (const fieldEntry of containerFields) {
        const fname = sanitizeFieldName(fieldEntry.name || 'anon');
        const resolved = resolveType(fieldEntry.type, stateTypes, globalTypes);

        if (typeof resolved === 'string' && ALIAS_MAP[resolved] === 'slot') {
            fields.push({ name: fname, type: 'slot' });
            continue;
        }
        if (typeof resolved === 'string' && ALIAS_MAP[resolved] === 'rest_buffer') {
            fields.push({ name: fname, type: 'rest_buffer' });
            continue;
        }
        if (Array.isArray(resolved) && resolved[0] === 'container' && looksLikeSlotContainer(resolved[1])) {
            fields.push({ name: fname, type: 'slot' });
            continue;
        }
        if (Array.isArray(resolved) && resolved[0] === 'array') {
            const params = resolved[1] || {};
            const elemResolved = resolveType(params.type, stateTypes, globalTypes);
            if (Array.isArray(elemResolved) && elemResolved[0] === 'container') {
                const inner = flattenContainerFields(elemResolved[1] || [], stateTypes, globalTypes, false);
                if (inner.ok && inner.fields.length > 0) {
                    fields.push({ name: fname, type: 'byte_array' });
                    continue;
                }
            }
        }
        if (Array.isArray(resolved) && resolved[0] === 'option') {
            const optionPayload = resolveType(resolved[1], stateTypes, globalTypes);
            if (Array.isArray(optionPayload) && optionPayload[0] === 'container') {
                const inner = flattenContainerFields(optionPayload[1] || [], stateTypes, globalTypes, false);
                if (!inner.ok) {
                    if (tailOnComplex) {
                        fields.push({ name: 'tail', type: 'rest_buffer' });
                        return { ok: true, fields };
                    }
                    return { ok: false, reason: `field ${fieldEntry.name}: option container: ${inner.reason}`, fields: [] };
                }
                const presentName = fname + '_present';
                fields.push({ name: presentName, type: 'boolean' });
                for (const innerField of inner.fields) {
                    fields.push({
                        ...innerField,
                        name: fname + '_' + innerField.name,
                        optional_if: presentName,
                    });
                }
                continue;
            }
        }
        if (Array.isArray(resolved) && resolved[0] === 'container') {
            const inner = flattenContainerFields(resolved[1] || [], stateTypes, globalTypes, tailOnComplex);
            if (!inner.ok) {
                if (tailOnComplex) {
                    fields.push({ name: 'tail', type: 'rest_buffer' });
                    return { ok: true, fields };
                }
                return { ok: false, reason: `field ${fieldEntry.name}: ${inner.reason}`, fields: [] };
            }
            if (fieldEntry.anon) {
                fields.push(...inner.fields);
            } else {
                for (const innerField of inner.fields) {
                    fields.push({
                        ...innerField,
                        name: fname + '_' + innerField.name,
                    });
                }
            }
            continue;
        }

        const mapped = mapFieldType(fieldEntry.type, stateTypes, globalTypes);
        if (!mapped.ok) {
            if (Array.isArray(resolved) && resolved[0] === 'switch') {
                const params = resolved[1] || {};
                if (params.compareTo === 'present' && params.fields?.false === 'void') {
                    const trueBranch = params.fields.true;
                    const innerFields = (Array.isArray(trueBranch) && trueBranch[0] === 'container')
                        ? (trueBranch[1] || [])
                        : [];
                    for (const inner of innerFields) {
                        const innerName = sanitizeFieldName(inner.name);
                        const innerMapped = mapFieldType(inner.type, stateTypes, globalTypes);
                        if (!innerMapped.ok || !innerMapped.fieldType) {
                            if (tailOnComplex) {
                                fields.push({ name: 'tail', type: 'rest_buffer' });
                                return { ok: true, fields };
                            }
                            return { ok: false, reason: `field ${inner.name}: ${innerMapped.reason}`, fields: [] };
                        }
                        fields.push({
                            name: innerName,
                            type: innerMapped.fieldType,
                            optional_if: 'present',
                        });
                    }
                    continue;
                }
                const switched = pushConditionalSwitchFields(fname, params);
                if (switched.ok) {
                    fields.push(...switched.fields);
                    continue;
                }
                if (tailOnComplex) {
                    fields.push({ name: 'tail', type: 'rest_buffer' });
                    return { ok: true, fields };
                }
                return { ok: false, reason: `field ${fieldEntry.name}: ${switched.reason}`, fields: [] };
            }
            if (tailOnComplex) {
                fields.push({ name: 'tail', type: 'rest_buffer' });
                return { ok: true, fields };
            }
            return { ok: false, reason: `field ${fieldEntry.name}: ${mapped.reason}`, fields: [] };
        }
        if (mapped.voidField) {
            continue;
        }
        if (mapped.expandOption) {
            const presentName = fname + '_present';
            fields.push({ name: presentName, type: 'boolean' });
            fields.push({ name: fname, type: mapped.innerType, optional_if: presentName });
            continue;
        }
        if (Array.isArray(resolved) && resolved[0] === 'container' && fieldEntry.anon) {
            const inner = flattenContainerFields(resolved[1] || [], stateTypes, globalTypes, tailOnComplex);
            if (!inner.ok) {
                if (tailOnComplex) {
                    fields.push({ name: 'tail', type: 'rest_buffer' });
                    return { ok: true, fields };
                }
                return inner;
            }
            fields.push(...inner.fields);
            continue;
        }
        if (Array.isArray(resolved) && resolved[0] === 'container') {
            const inner = flattenContainerFields(resolved[1] || [], stateTypes, globalTypes, tailOnComplex);
            if (!inner.ok) {
                if (tailOnComplex) {
                    fields.push({ name: 'tail', type: 'rest_buffer' });
                    return { ok: true, fields };
                }
                return { ok: false, reason: `field ${fieldEntry.name}: ${inner.reason}`, fields: [] };
            }
            fields.push(...inner.fields);
            continue;
        }
        if (Array.isArray(resolved) && resolved[0] === 'switch') {
            const params = resolved[1] || {};
            if (params.compareTo === 'present' && params.fields?.false === 'void') {
                const trueBranch = params.fields.true;
                const innerFields = (Array.isArray(trueBranch) && trueBranch[0] === 'container')
                    ? (trueBranch[1] || [])
                    : [];
                for (const inner of innerFields) {
                    const innerName = sanitizeFieldName(inner.name);
                    const innerMapped = mapFieldType(inner.type, stateTypes, globalTypes);
                    if (!innerMapped.ok || !innerMapped.fieldType) {
                        if (tailOnComplex) {
                            fields.push({ name: 'tail', type: 'rest_buffer' });
                            return { ok: true, fields };
                        }
                        return { ok: false, reason: `field ${inner.name}: ${innerMapped.reason}`, fields: [] };
                    }
                    fields.push({
                        name: innerName,
                        type: innerMapped.fieldType,
                        optional_if: 'present',
                    });
                }
                continue;
            }
            const switched = pushConditionalSwitchFields(fname, params);
            if (switched.ok) {
                fields.push(...switched.fields);
                continue;
            }
            if (tailOnComplex) {
                fields.push({ name: 'tail', type: 'rest_buffer' });
                return { ok: true, fields };
            }
            return { ok: false, reason: `field ${fieldEntry.name}: ${switched.reason}`, fields: [] };
        }
        fields.push({ name: fname, type: mapped.fieldType });
    }
    return { ok: true, fields };
}

// Extract per-packet schemas from a single state/direction.
// Returns an array of { wireName, fields:[{name, type}], status, reason? }.
function extractDirection(directionEntry, globalTypes) {
    const result = [];
    const stateTypes = directionEntry?.types || {};

    const unsupportedPacket = (wireName, id, reason) => {
        if (RAW_POLICY === 'fail') {
            throw new Error(`Unsupported packet ${wireName}: ${reason}`);
        }
        if (RAW_POLICY === 'keep') {
            return {
                wireName, id,
                fields: [{ name: 'raw', type: 'rest_buffer' }],
                status: 'rawOnly', reason,
            };
        }
        return {
            wireName, id,
            fields: [],
            status: 'unsupported', reason,
        };
    };

    const opaquePayloadPacket = (wireName, id, reason) => {
        if (RAW_POLICY === 'fail') {
            throw new Error(`Unsupported packet ${wireName}: ${reason}`);
        }
        if (RAW_POLICY === 'keep') {
            return unsupportedPacket(wireName, id, reason);
        }
        return {
            wireName, id,
            fields: [{ name: 'payload', type: 'rest_buffer' }],
            status: 'ok', reason,
        };
    };

    // The packet-id mapper lives at "packet". If absent, this direction has
    // no packets in this version.
    const packetWrapper = stateTypes.packet;
    if (!Array.isArray(packetWrapper) || packetWrapper[0] !== 'container') {
        return result;
    }
    const nameField = (packetWrapper[1] || []).find(f => f.name === 'name');
    if (!nameField || !Array.isArray(nameField.type) || nameField.type[0] !== 'mapper') {
        return result;
    }
    const mapper = nameField.type[1];
    const mappings = mapper.mappings || {};

    for (const [hexId, wireName] of Object.entries(mappings)) {
        const id = Number.parseInt(hexId, 16);
        if (!Number.isFinite(id)) {
            continue;
        }

        const containerNames = [`packet_${wireName}`, `packet_common_${wireName}`];
        let container = null;
        for (const containerName of containerNames) {
            if (Array.isArray(stateTypes[containerName])) {
                container = stateTypes[containerName];
                break;
            }
            if (Array.isArray(globalTypes[containerName])) {
                container = globalTypes[containerName];
                break;
            }
        }
        if (!Array.isArray(container)) {
            result.push(opaquePayloadPacket(wireName, id, 'minecraft-data has no packet schema'));
            continue;
        }
        if (container[0] === 'container') {
            const containerFields = container[1] || [];
            const useTail = true;
            const flattened = flattenContainerFields(containerFields, stateTypes, globalTypes, useTail);
            if (!flattened.ok) {
                result.push(unsupportedPacket(wireName, id, flattened.reason));
            } else {
                // This packet contains an array of structured known-pack
                // entries. byte_array would add a second VarInt length and
                // corrupt the element count, so preserve the encoded array.
                if (wireName === 'select_known_packs') {
                    flattened.fields = [{ name: 'packs', type: 'rest_buffer' }];
                } else if (wireName === 'registry_data' &&
                           flattened.fields.some(field => field.name === 'entries')) {
                    flattened.fields = [
                        { name: 'id', type: 'string' },
                        { name: 'entries', type: 'rest_buffer' },
                    ];
                } else if (wireName === 'tags') {
                    flattened.fields = [{ name: 'tags', type: 'rest_buffer' }];
                }
                result.push({ wireName, id, fields: flattened.fields, status: 'ok' });
            }
        } else {
            result.push(opaquePayloadPacket(wireName, id, `top-level type is ${container[0]}`));
        }
    }
    return result;
}

// minecraft-data uses "toServer" / "toClient" - map to our enums.
const DIRECTION_MAP = {
    toServer: 'serverbound',
    toClient: 'clientbound',
};

const STATE_MAP = {
    handshaking:   'handshaking',
    status:        'status',
    login:         'login',
    play:          'play',
    configuration: 'configuration',
};

// ---------------------------------------------------------------------------
// Aggregation: walk all (version, state, direction, packet) tuples and produce
// a key-centric map suitable for emitting one register_schema call per key.
//
// Key format: "<state>.<direction>.<wireName>"
//
// PacketSchema:
//   key: string
//   state, direction
//   field_sets: Map<wire, Field[]>
//   ids:        Map<wire, id>
//   coverage:   Map<wire, "ok"|"rawOnly"|"unsupported">
// ---------------------------------------------------------------------------

function aggregateAcrossVersions(perVersion) {
    const byKey = new Map();
    for (const { display, wire, packets } of perVersion) {
        for (const pkt of packets) {
            if (pkt.status !== 'ok' && RAW_POLICY === 'omit') {
                continue;
            }
            const key = `${pkt.state}.${pkt.direction}.${pkt.wireName}`;
            let schema = byKey.get(key);
            if (!schema) {
                schema = {
                    key,
                    state: pkt.state,
                    direction: pkt.direction,
                    wireName: pkt.wireName,
                    fieldSets: new Map(),
                    ids: new Map(),
                    coverage: new Map(),
                    rawReasons: new Map(),
                };
                byKey.set(key, schema);
            }
            schema.ids.set(wire, pkt.id);
            schema.coverage.set(wire, pkt.status);
            if (pkt.reason) {
                schema.rawReasons.set(wire, pkt.reason);
            }
            schema.fieldSets.set(wire, pkt.fields);
            schema.displayByWire ??= new Map();
            schema.displayByWire.set(wire, display);
        }
    }
    return byKey;
}

// Two field arrays are "equal" iff they have the same length and matching
// (name, type) pairs.
function fieldsEqual(a, b) {
    if (a.length !== b.length) return false;
    for (let i = 0; i < a.length; ++i) {
        if (a[i].name !== b[i].name || a[i].type !== b[i].type
            || (a[i].optional_if || '') !== (b[i].optional_if || '')
            || (a[i].condition_field || '') !== (b[i].condition_field || '')
            || JSON.stringify(a[i].condition_values || []) !== JSON.stringify(b[i].condition_values || [])) {
            return false;
        }
    }
    return true;
}

// Collapse adjacent versions that share an identical field set. The output is
// a sparse Map<wire, fields> where each entry marks the start of a contiguous
// run; the registry uses upper_bound to pick the entry for any version >=
// that key.
function collapseFieldSets(fieldSets) {
    const ordered = [...fieldSets.entries()].sort((a, b) => a[0] - b[0]);
    const collapsed = new Map();
    let prev = null;
    for (const [wire, fields] of ordered) {
        if (prev === null || !fieldsEqual(prev, fields)) {
            collapsed.set(wire, fields);
            prev = fields;
        }
    }
    return collapsed;
}

// ---------------------------------------------------------------------------
// C++ emission
// ---------------------------------------------------------------------------

function cppIdentForKey(key) {
    return key.replace(/[^A-Za-z0-9]/g, '_');
}

function cppStringLiteral(s) {
    return `"${s.replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`;
}

function emitPacketKeysHeader(schemas) {
    const sorted = [...schemas.values()].sort((a, b) => a.key.localeCompare(b.key));
    const lines = [
        '// AUTOGENERATED FILE. DO NOT EDIT BY HAND.',
        '// Regenerate with `node tools/generate_packets.mjs --out generated --versions ...`.',
        '#pragma once',
        '',
        '#include <string_view>',
        '',
        'namespace kprotocol::generated::packet_keys {',
        '',
    ];
    for (const s of sorted) {
        lines.push(`inline constexpr std::string_view ${cppIdentForKey(s.key)} = ${cppStringLiteral(s.key)};`);
    }
    lines.push('', '} // namespace kprotocol::generated::packet_keys', '');
    return lines.join('\n');
}

function emitFieldsLiteral(fields, indent) {
    if (fields.length === 0) return '{}';
    const pad = ' '.repeat(indent);
    const inner = fields.map(f => {
        if (f.condition_field) {
            const values = (f.condition_values || []).join(', ');
            return `${pad}    {${cppStringLiteral(f.name)}, kprotocol::FieldType::${f.type}, ${cppStringLiteral(f.optional_if || '')}, ${cppStringLiteral(f.condition_field)}, {${values}}}`;
        }
        const opt = f.optional_if
            ? `, ${cppStringLiteral(f.optional_if)}`
            : '';
        return `${pad}    {${cppStringLiteral(f.name)}, kprotocol::FieldType::${f.type}${opt}}`;
    }).join(',\n');
    return `{\n${inner}\n${pad}}`;
}

function emitSchemaBlock(s, indent) {
    const pad = ' '.repeat(indent);
    const collapsedFields = collapseFieldSets(s.fieldSets);
    const lines = [];
    lines.push(`${pad}{`);
    lines.push(`${pad}    PacketSchema schema;`);
    lines.push(`${pad}    schema.key = ${cppStringLiteral(s.key)};`);
    lines.push(`${pad}    schema.state = PacketState::${s.state};`);
    lines.push(`${pad}    schema.direction = PacketDirection::${s.direction};`);
    const orderedIds = [...s.ids.entries()].sort((a, b) => a[0] - b[0]);
    for (const [wire, id] of orderedIds) {
        lines.push(`${pad}    schema.ids.emplace(${knownVersionExpr(wire)}, ${id});`);
    }
    const orderedFs = [...collapsedFields.entries()].sort((a, b) => a[0] - b[0]);
    for (const [wire, fields] of orderedFs) {
        const literal = emitFieldsLiteral(fields, indent + 4);
        lines.push(`${pad}    schema.field_sets.emplace(${knownVersionExpr(wire)}, std::vector<FieldSpec>${literal});`);
    }
    lines.push(`${pad}    registry.register_schema(std::move(schema));`);
    lines.push(`${pad}}`);
    return lines;
}

function emitSchemasSource(schemas) {
    const sorted = [...schemas.values()].sort((a, b) => a.key.localeCompare(b.key));
    const batchSize = 32;
    const lines = [
        '// AUTOGENERATED FILE. DO NOT EDIT BY HAND.',
        '// Regenerate with `node tools/generate_packets.mjs --out generated --versions ...`.',
        '#include "kprotocol/generated.hpp"',
        '#include "kprotocol/packet.hpp"',
        '#include "kprotocol/registry.hpp"',
        '#include "kprotocol/version.hpp"',
        '',
        '#include <map>',
        '#include <vector>',
        '',
        'namespace kprotocol {',
        '',
    ];

    for (let batch = 0; batch * batchSize < sorted.length; ++batch) {
        const slice = sorted.slice(batch * batchSize, (batch + 1) * batchSize);
        lines.push(`static void register_generated_packets_batch_${batch}(PacketRegistry& registry) {`);
        for (const s of slice) {
            lines.push(...emitSchemaBlock(s, 4));
        }
        lines.push('}');
        lines.push('');
    }

    lines.push('void register_generated_packets(PacketRegistry& registry) {');
    for (let batch = 0; batch * batchSize < sorted.length; ++batch) {
        lines.push(`    register_generated_packets_batch_${batch}(registry);`);
    }
    lines.push('}');
    lines.push('');
    lines.push('} // namespace kprotocol');
    lines.push('');
    return lines.join('\n');
}

function emitCoverageJson(perVersion, schemas) {
    const out = {
        versions: [],
        keys: [],
        unsupported: [],
    };
    for (const { display, wire, packets } of perVersion) {
        const okCount = packets.filter(p => p.status === 'ok').length;
        const rawCount = packets.filter(p => p.status === 'rawOnly').length;
        const unsupportedCount = packets.filter(p => p.status === 'unsupported').length;
        out.versions.push({
            display,
            wire,
            totalPackets: packets.length,
            fullSchemas: okCount,
            rawOnly: rawCount,
            unsupported: unsupportedCount,
        });
        for (const pkt of packets) {
            if (pkt.status === 'unsupported') {
                out.unsupported.push({
                    display,
                    wire,
                    key: `${pkt.state}.${pkt.direction}.${pkt.wireName}`,
                    reason: pkt.reason || 'unsupported packet shape',
                });
            }
        }
    }
    for (const schema of [...schemas.values()].sort((a, b) => a.key.localeCompare(b.key))) {
        const status = {};
        const rawReasons = {};
        for (const [wire, st] of schema.coverage.entries()) {
            status[wire] = st;
        }
        for (const [wire, reason] of schema.rawReasons.entries()) {
            rawReasons[wire] = reason;
        }
        out.keys.push({
            key: schema.key,
            state: schema.state,
            direction: schema.direction,
            ids: Object.fromEntries(schema.ids),
            statusByWire: status,
            ...(Object.keys(rawReasons).length > 0 ? { rawReasonByWire: rawReasons } : {}),
        });
    }
    return JSON.stringify(out, null, 2);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

async function main() {
    const args = parseArgs(process.argv.slice(2));
    RAW_POLICY = args.rawPolicy;
    if (args.versions.length === 1 && args.versions[0] === 'all-known') {
        args.versions = canonicalVersionDisplays();
    }

    let mcd;
    try {
        mcd = (await import('minecraft-data')).default;
    } catch (err) {
        console.error('Failed to import minecraft-data. Did you `npm install minecraft-data`?');
        console.error(err.message);
        process.exit(3);
    }

    const perVersion = [];
    for (const display of args.versions) {
        const vinfo = findVersion(display);
        if (!vinfo) {
            console.error(`Version "${display}" is not in kprotocol's known-version table. ` +
                          `Add it to KPROTOCOL_FOR_EACH_KNOWN_VERSION first.`);
            process.exit(4);
        }
        const inst = mcd(display);
        if (!inst || !inst.protocol) {
            console.error(`minecraft-data has no protocol for "${display}".`);
            process.exit(5);
        }
        const protocol = inst.protocol;
        const globalTypes = protocol.types || {};

        const packets = [];
        for (const [stateName, stateEntry] of Object.entries(protocol)) {
            if (!STATE_MAP[stateName]) continue;
            for (const [dirName, dirEntry] of Object.entries(stateEntry || {})) {
                if (!DIRECTION_MAP[dirName]) continue;
                const extracted = extractDirection(dirEntry, globalTypes);
                for (const pkt of extracted) {
                    packets.push({
                        ...pkt,
                        state: STATE_MAP[stateName],
                        direction: DIRECTION_MAP[dirName],
                    });
                }
            }
        }
        perVersion.push({ display, wire: vinfo.wire, packets });
        const okCount = packets.filter(p => p.status === 'ok').length;
        const rawCount = packets.filter(p => p.status === 'rawOnly').length;
        const unsupportedCount = packets.filter(p => p.status === 'unsupported').length;
        console.log(`  ${display.padEnd(8)} wire=${String(vinfo.wire).padStart(3)} packets=${packets.length} (${okCount} full, ${rawCount} raw, ${unsupportedCount} unsupported)`);
    }

    const schemas = aggregateAcrossVersions(perVersion);

    await fs.mkdir(path.join(args.out, 'include', 'kprotocol', 'generated'), { recursive: true });
    await fs.mkdir(path.join(args.out, 'registry'), { recursive: true });

    await fs.writeFile(
        path.join(args.out, 'include', 'kprotocol', 'generated', 'packet_keys.hpp'),
        emitPacketKeysHeader(schemas));
    await fs.writeFile(
        path.join(args.out, 'registry', 'register_all_packets.cpp'),
        emitSchemasSource(schemas));
    await fs.writeFile(
        path.join(args.out, 'coverage.json'),
        emitCoverageJson(perVersion, schemas));

    console.log(`Wrote ${schemas.size} packet keys to ${args.out}.`);
}

main().catch(err => {
    console.error(err);
    process.exit(1);
});
