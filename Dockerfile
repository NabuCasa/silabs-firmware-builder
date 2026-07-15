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
        # Download QEMU 11.0.2 source
        && aria2c --checksum=sha-256=f1ab4eda29aa2e5c0b29ba38b7411c368b9b9bf822de6569c53ea8d4e04f980a -o qemu.tar.gz \
            https://gitlab.com/qemu-project/qemu/-/archive/v11.0.2/qemu-v11.0.2.tar.gz \
        && tar xzf qemu.tar.gz && rm qemu.tar.gz \
        && mv qemu-v11.0.2 qemu \
        && cd qemu \
        # Apply execve interception patch from conda-forge
        && aria2c -o execve.patch \
            https://raw.githubusercontent.com/conda-forge/qemu-execve-feedstock/988a9d79604e0a88cecc6bc4439625decdc308e9/recipe/patches/execve/apply-execve-JH.patch \
        && patch -p1 < execve.patch && rm execve.patch \
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

# Arm's official aarch64 toolchain was built WITHOUT libzstd and SiLabs uses
# zstd-compressed LTO bytecode in their precompiled SDK stack libraries. This prevents
# any compilation from succeeding on ARM64 hosts. We need to build our own minimal
# toolchain with zstd support to work around this. x86 is not affected.
FROM debian:trixie-slim AS zstd-gcc-builder
ARG TARGETARCH
RUN mkdir -p /opt/zstd-gcc \
    && if [ "$TARGETARCH" = "arm64" ]; then set -eux \
        && apt-get update && apt-get install -y --no-install-recommends \
            build-essential flex bison texinfo gawk libtool autoconf m4 \
            zlib1g-dev libzstd-dev wget file gettext bzip2 xz-utils ca-certificates git aria2 \
        && rm -rf /var/lib/apt/lists/* \
        && mkdir -p /build/src && cd /build \
        && aria2c --checksum=sha-256=e6405f20f8a817a50d92dbf7974d0ee77708dfdf9e79900a59c5d343b464ef9c -o src.tar.xz \
            https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.2.rel1/srcrel/arm-gnu-toolchain-src-snapshot-14.2.rel1.tar.xz \
        && tar -xJf src.tar.xz -C /build/src && rm src.tar.xz \
        && git clone --depth 1 --branch v1.1.0 \
            https://git.gitlab.arm.com/tooling/gnu-devtools-for-arm.git /build/src/gnu-devtools-for-arm \
        && ln -sf src/gnu-devtools-for-arm/build-gnu-toolchain.sh . \
        # The `start` stage normally creates `install/`. Running stages individually skips
        # it, so pre-create it or the first `do_config` can't write `install/.build_flags`.
        && mkdir -p /build/build-arm-none-eabi-armv7e-m/install \
        # Single multilib, no gdb. Through gcc2 so cc1plus (C++) is built too.
        && ./build-gnu-toolchain.sh --target=arm-none-eabi --with-arch=armv7e-m \
            --disable-multilib --disable-gdb \
            gmp mpfr mpc isl iconv binutils gcc1 newlib gcc2 \
        && for t in cc1 cc1plus lto1; do \
             cp "$(find /build -path '*/libexec/gcc/arm-none-eabi/*' -name "$t" | head -1)" /opt/zstd-gcc/; \
           done \
        # Strip debug info (Arm ships these stripped; unstripped they are ~340 MB each)
        && strip /opt/zstd-gcc/* \
        && rm -rf /build; \
    fi

# Python virtual environment for the firmware builder script
FROM debian:trixie-slim AS python-venv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /usr/bin/
COPY requirements.txt /tmp/
RUN UV_PYTHON_INSTALL_DIR=/opt/pythons uv venv -p 3.14 /opt/venv --no-cache \
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
    && aria2c --checksum=sha-256=2b9941216a3549aea6c5cc76565e2bc91ebfd9f41bec1e026341ce47c3aca1d0 -o slt.zip \
        https://www.silabs.com/documents/public/software/slt-cli-1.1.2-linux-x64.zip \
    && bsdtar -xf slt.zip -C /usr/bin && rm slt.zip \
    && chmod +x /usr/bin/slt \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        dpkg --add-architecture amd64 \
        && apt-get update \
        && apt-get install -y --no-install-recommends libc6:amd64 zlib1g:amd64 \
        && rm -rf /var/lib/apt/lists/* \
        # slt needs to be emulated. It executes conan_wrapper during installation so we need to use --execve to make it work
        && mv /usr/bin/slt /usr/bin/slt-bin \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static --execve /usr/bin/qemu-x86_64-static /usr/bin/slt-bin "$@"\n' > /usr/bin/slt \
        && chmod +x /usr/bin/slt \
        # Install conan
        && slt --non-interactive install conan \
        && mv /root/.silabs/slt/engines/conan/conan_engine /root/.silabs/slt/engines/conan/conan_engine-bin \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static /root/.silabs/slt/engines/conan/conan_engine-bin "$@"\n' > /root/.silabs/slt/engines/conan/conan_engine \
        && chmod +x /root/.silabs/slt/engines/conan/conan_engine \
        # Remove --execve from slt wrapper once we install conan, so native tools (tar, etc.) run without QEMU
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64-static /usr/bin/slt-bin "$@"\n' > /usr/bin/slt \
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
    && slt --non-interactive install \
        cmake/3.30.2 \
        ninja/1.12.1 \
        commander/1.24.1 \
        slc-cli/6.0.22 \
        simplicity-sdk/2026.6.0 \
        zap/2026.06.17 \
    # We don't currently use the LLVM toolchain that is pulled in as a default
    # dependency. Uninstall it to save space.
    && slt --non-interactive uninstall --force llvm-arm-toolchain-for-embedded \
    # Clean up download caches to reduce image size
    && rm -rf /root/.silabs/slt/installs/archive/*.zip \
              /root/.silabs/slt/installs/archive/*.tar.* \
              /root/.silabs/slt/installs/conan/p/*/d/ \
              /root/.silabs/slt/installs/conan/download_cache \
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
       # Needed at runtime by the zstd-enabled cc1/cc1plus/lto1 swapped in below (ARM64)
       libzstd1 \
    && rm -rf /var/lib/apt/lists/* \
    # Fix git permission error when building locally
    && git config --global --add safe.directory '*'

# Copy from parallel stages
COPY --from=python-venv /opt/pythons /opt/pythons
COPY --from=python-venv /opt/venv /opt/venv
COPY --from=qemu-execve-builder /usr/src/qemu/build/qemu-x86_64 /usr/bin/qemu-x86_64-static
COPY --from=slt-toolchain /usr/bin/slt* /usr/bin/
COPY --from=slt-toolchain /root/.silabs /root/.silabs
COPY --from=zstd-gcc-builder /opt/zstd-gcc /tmp/zstd-gcc
RUN if [ "$TARGETARCH" = "arm64" ]; then set -eux \
        && d="$(dirname "$(find /root/.silabs -path '*/libexec/gcc/arm-none-eabi/*' -name lto1 | head -1)")" \
        && cp /tmp/zstd-gcc/cc1 /tmp/zstd-gcc/cc1plus /tmp/zstd-gcc/lto1 "$d/"; \
    fi \
    && rm -rf /tmp/zstd-gcc

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV HOME=/root
ENV PATH="$PATH:/root/.silabs/slt/bin"

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
