#!/usr/bin/env node
// Opt-in real-client integration harness.
//
// The harness starts kprotocol_play_demo, waits until it accepts TCP
// connections, then invokes a user-supplied real client command once per
// requested Minecraft version. The client command should connect, complete
// offline-mode login, observe the play demo packets, and exit 0 on success.

import net from 'node:net';
import process from 'node:process';
import { spawn } from 'node:child_process';

const DEFAULT_VERSIONS = '1.8:47,1.21.1:767';

function parseArgs(argv) {
  const out = {
    server: '',
    clientCmd: process.env.KPROTOCOL_REAL_CLIENT_CMD || '',
    host: process.env.KPROTOCOL_REAL_CLIENT_HOST || '127.0.0.1',
    port: Number(process.env.KPROTOCOL_REAL_CLIENT_PORT || '25565'),
    versions: process.env.KPROTOCOL_REAL_CLIENT_VERSIONS || DEFAULT_VERSIONS,
    timeoutMs: Number(process.env.KPROTOCOL_REAL_CLIENT_TIMEOUT_MS || '60000'),
  };

  for (let i = 0; i < argv.length; ++i) {
    const arg = argv[i];
    const next = () => {
      if (i + 1 >= argv.length) {
        throw new Error(`Missing value for ${arg}`);
      }
      return argv[++i];
    };
    if (arg === '--server') out.server = next();
    else if (arg === '--client-cmd') out.clientCmd = next();
    else if (arg === '--host') out.host = next();
    else if (arg === '--port') out.port = Number(next());
    else if (arg === '--versions') out.versions = next();
    else if (arg === '--timeout-ms') out.timeoutMs = Number(next());
    else if (arg === '--help' || arg === '-h') {
      printHelp();
      process.exit(0);
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }
  return out;
}

function printHelp() {
  console.log(`Usage:
  node tools/real_client_smoke.mjs --server <kprotocol_play_demo> --client-cmd <command>

Options:
  --host <host>          Server bind/connect host. Default: 127.0.0.1
  --port <port>          Server port. Default: 25565
  --versions <list>      Comma list of name:wire pairs. Default: ${DEFAULT_VERSIONS}
  --timeout-ms <ms>      Per-client timeout. Default: 60000

The client command may use placeholders:
  {host} {port} {version} {wire}

The same values are also provided as environment variables:
  KPROTOCOL_REAL_CLIENT_HOST
  KPROTOCOL_REAL_CLIENT_PORT
  KPROTOCOL_REAL_CLIENT_VERSION
  KPROTOCOL_REAL_CLIENT_WIRE
`);
}

function parseVersions(value) {
  return value.split(',')
    .map(v => v.trim())
    .filter(Boolean)
    .map(entry => {
      const [version, wire] = entry.split(':');
      if (!version || !wire || Number.isNaN(Number(wire))) {
        throw new Error(`Invalid version entry: ${entry}`);
      }
      return { version, wire: Number(wire) };
    });
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function waitForTcp(host, port, timeoutMs) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const ok = await new Promise(resolve => {
      const socket = net.createConnection({ host, port });
      socket.once('connect', () => {
        socket.destroy();
        resolve(true);
      });
      socket.once('error', () => resolve(false));
      socket.setTimeout(500, () => {
        socket.destroy();
        resolve(false);
      });
    });
    if (ok) return;
    await sleep(100);
  }
  throw new Error(`Server did not accept TCP connections on ${host}:${port}`);
}

function startServer(serverPath, host, port) {
  const child = spawn(serverPath, [String(port)], {
    env: {
      ...process.env,
      KPROTOCOL_PORT: String(port),
      KPROTOCOL_REAL_CLIENT_HOST: host,
    },
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  child.stdout.on('data', chunk => process.stdout.write(`[server] ${chunk}`));
  child.stderr.on('data', chunk => process.stderr.write(`[server] ${chunk}`));
  return child;
}

function renderCommand(template, values) {
  return template
    .replaceAll('{host}', values.host)
    .replaceAll('{port}', String(values.port))
    .replaceAll('{version}', values.version)
    .replaceAll('{wire}', String(values.wire));
}

function runClient(template, values, timeoutMs) {
  return new Promise((resolve, reject) => {
    const command = renderCommand(template, values);
    console.log(`[client:${values.version}] ${command}`);
    const child = spawn(command, {
      shell: true,
      env: {
        ...process.env,
        KPROTOCOL_REAL_CLIENT_HOST: values.host,
        KPROTOCOL_REAL_CLIENT_PORT: String(values.port),
        KPROTOCOL_REAL_CLIENT_VERSION: values.version,
        KPROTOCOL_REAL_CLIENT_WIRE: String(values.wire),
      },
      stdio: 'inherit',
    });

    const timer = setTimeout(() => {
      child.kill();
      reject(new Error(`Real client timed out for ${values.version}`));
    }, timeoutMs);

    child.on('exit', code => {
      clearTimeout(timer);
      if (code === 0) resolve();
      else reject(new Error(`Real client failed for ${values.version} with exit code ${code}`));
    });
    child.on('error', err => {
      clearTimeout(timer);
      reject(err);
    });
  });
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (!args.server) {
    throw new Error('--server is required');
  }
  if (!args.clientCmd) {
    throw new Error('--client-cmd or KPROTOCOL_REAL_CLIENT_CMD is required');
  }
  if (!Number.isInteger(args.port) || args.port <= 0 || args.port > 65535) {
    throw new Error(`Invalid port: ${args.port}`);
  }

  const versions = parseVersions(args.versions);
  const server = startServer(args.server, args.host, args.port);
  try {
    await waitForTcp(args.host, args.port, args.timeoutMs);
    for (const version of versions) {
      await runClient(args.clientCmd, {
        host: args.host,
        port: args.port,
        version: version.version,
        wire: version.wire,
      }, args.timeoutMs);
    }
    console.log('Real-client integration smoke passed.');
  } finally {
    server.kill();
  }
}

main().catch(err => {
  console.error(err?.stack || err);
  process.exit(1);
});
