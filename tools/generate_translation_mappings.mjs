#!/usr/bin/env node
// Generate block state ID translation tables from minecraft-data (by block name bridge).
import { promises as fs } from 'node:fs';
import path from 'node:path';
import process from 'node:process';

const VERSIONS = [
    '1.8', '1.12.2', '1.13', '1.14', '1.16.5', '1.17', '1.18', '1.19',
    '1.20.2', '1.20.4', '1.21.1', '1.21.4', '1.21.5', '1.21.6', '1.21.7',
    '1.21.9', '1.21.11',
];

function defaultStateIdByName(inst) {
    const out = new Map();
    for (const block of inst.blocksArray || []) {
        if (!block?.name) continue;
        out.set(block.name, block.defaultState);
    }
    return out;
}

function stateIdToBlockName(inst, stateId) {
    const block = inst.blocksByStateId?.[String(stateId)];
    return block?.name ?? null;
}

async function main() {
    const mcd = (await import('minecraft-data')).default;

    const perVersion = new Map();
    for (const v of VERSIONS) {
        const inst = mcd(v);
        perVersion.set(v, {
            inst,
            defaultByName: defaultStateIdByName(inst),
            maxState: Object.keys(inst.blocksByStateId || {}).length,
        });
        console.log(`  ${v}: ${perVersion.get(v).defaultByName.size} blocks`);
    }

    const out = {};
    for (const from of VERSIONS) {
        for (const to of VERSIONS) {
            if (from === to) continue;
            const key = `${from}_to_${to}`;
            const fromData = perVersion.get(from);
            const toData = perVersion.get(to);
            const mapping = {};

            for (let id = 0; id < fromData.maxState; ++id) {
                const name = stateIdToBlockName(fromData.inst, id);
                if (!name) continue;
                const targetDefault = toData.defaultByName.get(name);
                if (targetDefault !== undefined) {
                    mapping[String(id)] = targetDefault;
                }
            }

            out[key] = mapping;
            const diffs = Object.entries(mapping).filter(([a, b]) => Number(a) !== Number(b)).length;
            console.log(`  ${key}: ${Object.keys(mapping).length} mapped (${diffs} differ)`);
        }
    }

    const root = path.resolve(path.dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1')), '..');
    await fs.writeFile(path.join(root, 'tools', 'translation_mappings.json'), JSON.stringify(out, null, 2));
    console.log('Wrote tools/translation_mappings.json');
}

main().catch(err => {
    console.error(err);
    process.exit(1);
});
