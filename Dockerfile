FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    curl \
    git \
    gnupg \
    lsb-release \
    software-properties-common \
    wget \
    zlib1g-dev \
    && curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y --no-install-recommends \
        build-essential \
        nodejs \
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
