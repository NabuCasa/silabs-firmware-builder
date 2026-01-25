# Build QEMU with execve interception patch for running x86_64 binaries on ARM64.
# This patched QEMU intercepts execve() syscalls and wraps child processes with QEMU,
# which is necessary because binfmt_misc is not available during Docker builds.
#
# The purpose of this patch is to allow us to emulate `slt` and `conan`, the new Silicon
# Labs tooling for setting up development tools. These tools are not available for ARM64
# Linux but strangely all other packages are, meaning we can just emulate the downloading
# part and still have native performance for all compilation.
#
# We use upstream QEMU with the execve interception patch from conda-forge/qemu-execve-feedstock.
# This patch adds --execve <path> option (or QEMU_EXECVE env var) to intercept execve() calls.
# See https://github.com/conda-forge/qemu-execve-feedstock for the patch source.
#
# We can remove this once SiLabs releases builds of `slt` and `conan` for ARM64 Linux.
FROM --platform=$BUILDPLATFORM debian:bookworm-slim AS qemu-execve-builder
ARG TARGETARCH
WORKDIR /usr/src
RUN if [ "$TARGETARCH" = "arm64" ]; then \
        apt-get -q update \
        && apt-get -qqy install \
            build-essential \
            zlib1g-dev \
            libpixman-1-dev \
            python3 \
            python3-venv \
            libglib2.0-dev \
            pkg-config \
            ninja-build \
            aria2 \
            ca-certificates \
            git \
        && rm -rf /var/lib/apt/lists/* \
        # Download QEMU 10.2.0 source
        && aria2c --checksum=sha-256=6d7046777ce992e5ce88c413ebe3d1f57b1833e7d8200724097c9b74507e196e -o qemu.tar.gz \
            https://gitlab.com/qemu-project/qemu/-/archive/v10.2.0/qemu-v10.2.0.tar.gz \
        && tar xzf qemu.tar.gz && rm qemu.tar.gz \
        && mv qemu-v10.2.0 qemu \
        && cd qemu \
        # Apply execve interception patch from conda-forge
        && aria2c -o execve.patch \
            https://raw.githubusercontent.com/conda-forge/qemu-execve-feedstock/7d9a37bb5498831e0eb9a1cff5c0c6f3d49cb1f7/recipe/patches/apply-execve-JH.patch \
        && patch -p1 < execve.patch && rm execve.patch \
        # Skip vsyscall page setup when reserved_va is enabled
        && sed -i '/Cannot allocate vsyscall page/{s/.*/        return true;/;n;d}' linux-user/x86_64/elfload.c \
        && ./configure \
            --target-list=x86_64-linux-user \
            --static \
            --disable-pie \
            --disable-docs \
            --disable-tools \
            --disable-capstone \
            --disable-guest-agent \
        && make -j $(nproc); \
    else \
        # Dummy file to copy for x86_64
        mkdir -p /usr/src/qemu/build && touch /usr/src/qemu/build/qemu-x86_64; \
    fi

# The new `slt` tool does not provide Gecko SDK builds so we have to download it ourselves.
FROM debian:trixie-slim AS gecko-sdk-v4.5.0
RUN apt-get update && apt-get install -y --no-install-recommends aria2 ca-certificates libarchive-tools \
    && rm -rf /var/lib/apt/lists/* \
    && aria2c --checksum=sha-256=b5b2b2410eac0c9e2a72320f46605ecac0d376910cafded5daca9c1f78e966c8 -o sdk.zip \
        https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.5.0/gecko-sdk.zip \
    && mkdir /out && bsdtar -xf sdk.zip -C /out \
    && rm sdk.zip

# Python virtual environment for the firmware builder script
FROM debian:trixie-slim AS python-venv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /usr/bin/
COPY requirements.txt /tmp/
RUN UV_PYTHON_INSTALL_DIR=/opt/pythons uv venv -p 3.13 /opt/venv --no-cache \
    && uv pip install --python /opt/venv -r /tmp/requirements.txt

# Install slt and all toolchain packages (depends on QEMU for ARM64)
FROM debian:trixie-slim AS slt-toolchain
ARG TARGETARCH

# Copy patched QEMU with execve interception (only used on ARM64)
COPY --from=qemu-execve-builder /usr/src/qemu/build/qemu-x86_64 /usr/bin/qemu-x86_64-static

# Set up slt and conan
RUN set -e \
    && apt-get update && apt-get install -y --no-install-recommends \
        aria2 \
        ca-certificates \
        # Required by conan
        libarchive-tools \
        bzip2 \
        unzip \
    && rm -rf /var/lib/apt/lists/* \
    # slt-cli is x64 only but runs fine with QEMU
    && aria2c --checksum=sha-256=8c2dd5091c15d5dd7b8fc978a512c49d9b9c5da83d4d0b820cfe983b38ef3612 -o slt.zip \
        https://www.silabs.com/documents/public/software/slt-cli-1.1.0-linux-x64.zip \
    && bsdtar -xf slt.zip -C /usr/bin && rm slt.zip \
    && chmod +x /usr/bin/slt \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        dpkg --add-architecture amd64 \
        && apt-get update \
        && apt-get install -y --no-install-recommends libc6:amd64 zlib1g:amd64 \
        && rm -rf /var/lib/apt/lists/* \
        # slt needs to be emulated. It executes conan_wrapper during installation so we need to use --execve to make it work
        # -R limits address space to 47 bits, fixing Go binaries that assume 48-bit sign-extended addresses
        && mv /usr/bin/slt /usr/bin/slt-bin \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static -R 0x800000000000 --execve /usr/bin/qemu-x86_64-static /usr/bin/slt-bin "$@"\n' > /usr/bin/slt \
        && chmod +x /usr/bin/slt \
        # Install conan
        && slt --non-interactive install conan \
        && mv /root/.silabs/slt/engines/conan/conan_engine /root/.silabs/slt/engines/conan/conan_engine-bin \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static -R 0x800000000000 /root/.silabs/slt/engines/conan/conan_engine-bin "$@"\n' > /root/.silabs/slt/engines/conan/conan_engine \
        && chmod +x /root/.silabs/slt/engines/conan/conan_engine \
        # Remove --execve from slt wrapper once we install conan, so native tools (tar, etc.) run without QEMU
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static -R 0x800000000000 /usr/bin/slt-bin "$@"\n' > /usr/bin/slt \
        # Patch slt to select ARM64 packages for subsequent installs
        && sed -i 's/amd6/arm6/g' /usr/bin/slt-bin \
        # Force conan to use the ARM64 profile for downloading packages
        && cp /root/.silabs/slt/installs/conan/profiles/linux_arm64 /root/.silabs/slt/installs/conan/profiles/default \
        # Replace bundled conan with native conan 2.21.0, it uses Python to extract archives which is slow to emulate
        && aria2c --checksum=sha-256=2f356826c4c633f24355f4cb1d54a980a23c1912c0bcab54a771913af3b753b5 -o conan-2.21.0.tgz \
            https://github.com/conan-io/conan/releases/download/2.21.0/conan-2.21.0-linux-aarch64.tgz \
        && rm -rf /root/.silabs/slt/engines/conan/conan \
        && mkdir /root/.silabs/slt/engines/conan/conan \
        && bsdtar -xf conan-2.21.0.tgz --strip-components=1 -C /root/.silabs/slt/engines/conan/conan \
        && rm conan-2.21.0.tgz; \
    else \
        slt --non-interactive install conan; \
    fi

# Install toolchain via slt
RUN set -e \
    && apt-get update && apt-get install -y --no-install-recommends jq && rm -rf /var/lib/apt/lists/* \
    && slt --non-interactive install \
        cmake/3.30.2 \
        ninja/1.12.1 \
        commander/1.22.0 \
        slc-cli/6.0.15 \
        simplicity-sdk/2025.6.2 \
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
    # Create stable symlinks and wrappers to make the tools available in PATH
    && mkdir -p /root/.silabs/slt/bin \
    && ln -s "$(slt where java21)/jre/bin/java" /root/.silabs/slt/bin/java \
    && ln -s "$(slt where commander)/commander" /root/.silabs/slt/bin/commander \
    && ln -s "$(slt where cmake)/bin/cmake" /root/.silabs/slt/bin/cmake \
    && ln -s "$(slt where ninja)/ninja" /root/.silabs/slt/bin/ninja \
    # slc needs a wrapper script because it uses $(dirname "$0") to find slc.jar
    && printf '#!/bin/sh\nexec "%s/slc" "$@"\n' "$(slt where slc-cli)" > /root/.silabs/slt/bin/slc \
    && chmod +x /root/.silabs/slt/bin/slc

# Final image
FROM debian:trixie-slim
ARG TARGETARCH

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Install only runtime packages
RUN set -e \
    # Install x86_64 libraries for QEMU on ARM64
    && if [ "$TARGETARCH" = "arm64" ]; then \
        dpkg --add-architecture amd64 \
        && apt-get update \
        && apt-get install -y --no-install-recommends libc6:amd64 zlib1g:amd64; \
    else \
        apt-get update; \
    fi \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       git \
       libstdc++6 \
       libgl1 \
       libpng16-16 \
       libpcre2-16-0 \
       libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Copy from parallel stages
COPY --from=gecko-sdk-v4.5.0 /out /gecko_sdk_4.5.0
COPY --from=python-venv /opt/pythons /opt/pythons
COPY --from=python-venv /opt/venv /opt/venv
COPY --from=qemu-execve-builder /usr/src/qemu/build/qemu-x86_64 /usr/bin/qemu-x86_64-static
COPY --from=slt-toolchain /usr/bin/slt* /usr/bin/
COPY --from=slt-toolchain /root/.silabs /root/.silabs

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV HOME=/root
ENV PATH="$PATH:/root/.silabs/slt/bin"

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
