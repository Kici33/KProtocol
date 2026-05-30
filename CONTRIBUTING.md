# Contributing

Thanks for helping improve KProtocol.

## Development Setup

Prerequisites:

- CMake 3.21+
- A C++20 compiler
- Node.js 20+
- zlib

Typical local workflow:

```bash
npm ci
npm run generate
npm run check:coverage
cmake --preset release
cmake --build --preset release
ctest --preset release
```

For network-free CMake configure, use:

```bash
cmake --preset release-offline -DKPROTOCOL_ASIO_SOURCE_DIR=/path/to/asio
```

## Generated Files

The packet catalog under `generated/` is committed. When changing the generator
or the locked `minecraft-data` version, regenerate and review the generated diff:

```bash
npm run generate
npm run check:coverage
```

Block translation data is generated separately as described in `GENERATE.md`.

## Pull Request Checklist

- Keep changes focused.
- Add or update tests for behavior changes.
- Run the relevant CTest suite before opening a PR.
- Do not commit local build directories, logs, or `node_modules`.
- Preserve public API compatibility unless the change is intentionally breaking.

## Style

Use the repository `.clang-format` and `.editorconfig` settings. The project is
C++20, and public headers should stay usable by installed consumers through the
exported CMake targets.
