FROM node:20-bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    g++ \
    git \
    make \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace

COPY package.json package-lock.json ./
RUN npm ci

COPY . .

RUN npm run generate

RUN cmake -S . -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DKPROTOCOL_BUILD_EXAMPLES=OFF \
    && cmake --build build --config Release -j$(nproc)

RUN ctest --test-dir build --output-on-failure

CMD ["/bin/bash"]
