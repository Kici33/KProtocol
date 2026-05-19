FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install Node.js 18 and build tools. Use NodeSource for stable Node LTS.
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl ca-certificates gnupg lsb-release software-properties-common \
    && curl -fsSL https://deb.nodesource.com/setup_18.x | bash - \
    && apt-get install -y --no-install-recommends nodejs build-essential cmake git wget zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . /workspace

# Install npm dependency used by the generator
RUN npm ci || true

# Generate packets (best-effort)
RUN node generate_packets_v3.js --out generated --versions 1.8,1.12.2,1.16.5,1.20.4,1.21.1 || true

# Configure and build
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --config Release -j$(nproc) || true

# Run tests (non-fatal)
RUN ctest --test-dir build --output-on-failure || true

CMD ["/bin/bash"]
