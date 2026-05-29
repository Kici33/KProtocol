#!/usr/bin/env node
// SPDX-License-Identifier: MIT

import { promises as fs } from 'node:fs';
import process from 'node:process';

const coveragePath = process.argv[2] || 'generated/coverage.json';

const text = await fs.readFile(coveragePath, 'utf8');
const coverage = JSON.parse(text);

const totals = (coverage.versions || []).reduce(
    (acc, version) => {
        acc.total += version.totalPackets || 0;
        acc.full += version.fullSchemas || 0;
        acc.raw += version.rawOnly || 0;
        acc.unsupported += version.unsupported || 0;
        return acc;
    },
    { total: 0, full: 0, raw: 0, unsupported: 0 },
);

if (totals.raw !== 0 || totals.unsupported !== 0) {
    console.error(
        `Packet coverage check failed: rawOnly=${totals.raw}, unsupported=${totals.unsupported}`,
    );
    process.exit(1);
}

console.log(
    `Packet coverage ok: ${totals.full}/${totals.total} registered, rawOnly=0, unsupported=0`,
);
