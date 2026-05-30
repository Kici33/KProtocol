# Using Docker to build and test kprotocol

This repository includes a Dockerfile based on the official Node.js 20 slim
image. It installs the C++ build tools, runs `npm ci`, regenerates packets from
the locked `minecraft-data` version, builds the library, installs and verifies
the consumer package smoke test, and runs the full CTest suite.

Build and start a container (recommended for environments without CMake or a
C++ toolchain):

```bash
# build image (from repo root)
docker build -t kprotocol-build .

# start an interactive shell inside container
docker run --rm -it -v ${PWD}:/workspace kprotocol-build
```

The Docker image runs `npm run generate`, builds the project with CMake, and runs the test suite. Generated headers and registry sources are written under `generated/`.
The Docker context ignores the local `generated/` and `node_modules/`
directories, so the image verifies that generation and dependency install work
from a clean checkout.

## Notes
- The Docker build now fails fast if generation, compilation, or tests fail.
- `ctest` includes `kprotocol_install_consumer_smoke`, which installs the
  package to a temporary prefix and builds `examples/consumer` via
  `find_package(kprotocol CONFIG REQUIRED)`.
- For CI, run the same `docker build -t kprotocol-build .` command or mirror
  the Dockerfile steps.
