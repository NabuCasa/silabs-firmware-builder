# Build QEMU with execve interception patch for running x86_64 binaries on ARM64.
# This patched QEMU intercepts execve() syscalls and wraps child processes with QEMU,
# which is necessary because binfmt_misc is not available during Docker builds.
# https://github.com/balena-io/qemu
FROM debian:bookworm AS qemu-execve-builder
RUN apt-get -q update \
    && apt-get -qqy install \
        build-essential \
        zlib1g-dev \
        libpixman-1-dev \
        python3 \
        libglib2.0-dev \
        pkg-config \
        ninja-build \
        git \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /usr/src
RUN git clone --depth 1 --branch balena-7.0.0 https://github.com/balena-io/qemu.git \
    && cd qemu \
    && ./configure --target-list=x86_64-linux-user --static --disable-pie \
    && make -j $(nproc)

FROM debian:bookworm-slim AS gecko-sdk-v4.5.0
RUN apt-get update && apt-get install -y --no-install-recommends aria2 ca-certificates libarchive-tools \
    && rm -rf /var/lib/apt/lists/* \
    && aria2c --checksum=sha-256=b5b2b2410eac0c9e2a72320f46605ecac0d376910cafded5daca9c1f78e966c8 -o /tmp/sdk.zip \
        https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.5.0/gecko-sdk.zip \
    && mkdir /out && bsdtar -xf /tmp/sdk.zip -C /out \
    && rm /tmp/sdk.zip

# Note: we are intentionally using bookworm instead of trixie because of an incompatibility
# between recent QEMU releases and Go: https://github.com/golang/go/issues/69255
FROM debian:bookworm-slim
ARG TARGETARCH

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Copy patched QEMU with execve interception (only used on ARM64)
COPY --from=qemu-execve-builder /usr/src/qemu/build/qemu-x86_64 /usr/bin/qemu-x86_64-static

# Set up slt and conan
RUN set -e \
    && apt-get update && apt-get install -y --no-install-recommends \
        aria2 \
        bzip2 \
        ca-certificates \
        curl \
        libarchive-tools \
        unzip \
    && rm -rf /var/lib/apt/lists/* \
    # slt-cli is x64 only but runs fine with QEMU
    && curl -sL -o /tmp/slt.zip "https://www.silabs.com/documents/public/software/slt-cli-1.1.0-linux-x64.zip" \
    && unzip -q /tmp/slt.zip -d /usr/bin && chmod +x /usr/bin/slt && rm /tmp/slt.zip \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        dpkg --add-architecture amd64 \
        && apt-get update \
        && apt-get install -y --no-install-recommends libc6:amd64 zlib1g:amd64 \
        && rm -rf /var/lib/apt/lists/* \
        # slt-cli and conan are x86_64 only, need QEMU on ARM64
        # The -execve flag tells QEMU to intercept execve() and wrap child processes
        && mv /usr/bin/slt /usr/bin/slt-bin \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static -execve /usr/bin/slt-bin "$@"\n' > /usr/bin/slt \
        && chmod +x /usr/bin/slt \
        # Install conan
        && slt install conan \
        # Wrap conan binaries with explicit QEMU for subsequent RUN steps
        && mv /root/.silabs/slt/engines/conan/conan_engine /root/.silabs/slt/engines/conan/conan_engine-bin \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static /root/.silabs/slt/engines/conan/conan_engine-bin "$@"\n' > /root/.silabs/slt/engines/conan/conan_engine \
        && chmod +x /root/.silabs/slt/engines/conan/conan_engine \
        && mv /root/.silabs/slt/engines/conan/conan/conan /root/.silabs/slt/engines/conan/conan/conan-bin \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static /root/.silabs/slt/engines/conan/conan/conan-bin "$@"\n' > /root/.silabs/slt/engines/conan/conan/conan \
        && chmod +x /root/.silabs/slt/engines/conan/conan/conan \
        # Remove -execve from slt wrapper so native tools (tar, etc.) run without QEMU
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static /usr/bin/slt-bin "$@"\n' > /usr/bin/slt \
        # Patch slt to select ARM64 packages for subsequent installs
        && sed -i 's/amd6/arm6/g' /usr/bin/slt-bin \
        # Force conan to use the ARM64 profile for downloading packages
        && cp /root/.silabs/slt/installs/conan/profiles/linux_arm64 /root/.silabs/slt/installs/conan/profiles/default; \
    else \
        slt install conan; \
    fi

# Install toolchain via slt
RUN set -e \
    && apt-get update && apt-get install -y --no-install-recommends jq && rm -rf /var/lib/apt/lists/* \
    && slt install \
        simplicity-sdk/2025.6.2 \
        commander/1.22.0 \
        slc-cli/6.0.15 \
        cmake/3.30.2 \
        ninja/1.12.1 \
        zap/2025.12.02 \
    # Patch ZAP apack.json to add missing linux.aarch64 executable definitions
    # Remove once zap is bumped to 2026.x.x
    && ZAP_PATH="$(slt where zap)" \
    && jq '.executable["zap:linux.aarch64"]     = {"exe": "zap",     "optional": true} \
         | .executable["zap-cli:linux.aarch64"] = {"exe": "zap-cli", "optional": true}' \
        "$ZAP_PATH/apack.json" > /tmp/apack.json && mv /tmp/apack.json "$ZAP_PATH/apack.json" \
    # Clean up download caches to reduce image size
    && rm -rf /root/.silabs/slt/installs/archive/*.zip \
              /root/.silabs/slt/installs/archive/*.tar.* \
              /root/.silabs/slt/installs/conan/p/*/d/ \
    # Create stable symlinks/wrappers for PATH using slt where
    && mkdir -p /root/.silabs/slt/bin \
    && ln -s "$(slt where java21)/jre/bin/java" /root/.silabs/slt/bin/java \
    && ln -s "$(slt where commander)/commander" /root/.silabs/slt/bin/commander \
    && ln -s "$(slt where cmake)/bin/cmake" /root/.silabs/slt/bin/cmake \
    && ln -s "$(slt where ninja)/ninja" /root/.silabs/slt/bin/ninja \
    # slc needs a wrapper script because it uses $(dirname "$0") to find slc.jar
    && printf '#!/bin/sh\nexec "%s/slc" "$@"\n' "$(slt where slc-cli)" > /root/.silabs/slt/bin/slc \
    && chmod +x /root/.silabs/slt/bin/slc

# Python virtual environment for the firmware builder script
COPY requirements.txt /tmp/
RUN set -e \
    && apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl \
    && curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR="/usr/bin" sh \
    && UV_PYTHON_INSTALL_DIR=/opt/pythons uv venv -p 3.13 /opt/venv --no-cache \
    && uv pip install --python /opt/venv -r /tmp/requirements.txt \
    && rm -rf /var/lib/apt/lists/*

# Install runtime packages
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       git \
       libstdc++6 \
       libgl1 \
       libpng16-16 \
       libpcre2-16-0 \
       libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Copy Gecko SDK from the independent downloader stage
COPY --from=gecko-sdk-v4.5.0 /out /gecko_sdk_4.5.0

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV HOME=/root
ENV PATH="$PATH:/root/.silabs/slt/bin"

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
