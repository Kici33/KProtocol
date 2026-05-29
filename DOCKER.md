Using Docker to build and test kprotocol

This repository includes a Dockerfile based on the official Node.js 20 slim image. It installs the C++ build tools, regenerates packets, builds the library, and runs tests.

Build and start a container (recommended for environments without CMake or a C++ toolchain):

```bash
# build image (from repo root)
docker build -t kprotocol-build .

# start an interactive shell inside container
docker run --rm -it -v ${PWD}:/workspace kprotocol-build
```

The Docker image runs `npm run generate`, builds the project with CMake, and runs the test suite. Generated headers and registry sources are written under `generated/`.

Notes
- The Docker build now fails fast if generation, compilation, or tests fail.
- For CI a GitHub Actions workflow is included at .github/workflows/ci.yml to run the same steps on push/pull_request.
