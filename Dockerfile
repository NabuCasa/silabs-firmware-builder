# Simplicity Commander, slc-cli, ZAP, a JRE and a Python interpreter are published on
# Silicon Labs' update site as native builds for both architectures. Fetching them directly
# avoids `slt`, which is x86_64-only and would otherwise have to be emulated on ARM64.
FROM debian:trixie-slim AS silabs-tools
ARG TARGETARCH
RUN set -eux \
    && apt-get update && apt-get install -y --no-install-recommends \
        aria2 ca-certificates libarchive-tools \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /opt/silabs/commander /opt/silabs/slc-cli /opt/silabs/zap \
                /opt/silabs/java21 /opt/silabs/python \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        aria2c -q -o python.zip --checksum=sha-256=0bd0334fead1e3c2647b6c09b9801b214ab3b999e9e6a9d13553ff57c1e04bb2 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/python/3.10.3/python.3.10.gtk.linux.aarch64.zip \
        && aria2c -q -o slc.zip --checksum=sha-256=5d13e09e605e5dfb94abeb0773055647764bb30d746916f3fc3991a48282a6ba \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/slc-cli/6.0.22/slc-cli.linux.gtk.aarch64.zip \
        && aria2c -q -o zap.zip --checksum=sha-256=15c5263ea98a1162e655ad5fb154a4f0e62047b6382a889f9e986f570c30c45b \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/zap/2026.06.17/zap-linux-arm64.zip \
        && aria2c -q -o jre.zip --checksum=sha-256=61b4be111fe14d7a9138de920047489c679c7a697320cf69d28b23046edbaf73 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/java21/21.0.6/linux.aarch64_21.0.6.zip \
        && aria2c -q -o commander.tar.bz --checksum=sha-256=99fd45e5064b00ace957b4d12c00bb3c3b33845b4e65793fde99d4960004e091 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/commander/1.24.1/Commander_linux_aarch64_1v24p1b1980.tar.bz; \
       else \
        aria2c -q -o python.zip --checksum=sha-256=26f56b1cfa05b2b3b7dafb2c2a5e3c19498389c34dfe68be800f71f476c86363 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/python/3.10.3/python.3.10.3.gtk.linux.x86_64.zip \
        && aria2c -q -o slc.zip --checksum=sha-256=f05e078b369d7e7dbfa31bdd2ad8edf61f04a9291393deab4702137639ba7d19 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/slc-cli/6.0.22/slc-cli.linux.gtk.x86_64.zip \
        && aria2c -q -o zap.zip --checksum=sha-256=45537226973fb892894f7110288dd4a7db627728af8cd9fc7c27b658530e2b88 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/zap/2026.06.17/zap-linux-x64.zip \
        && aria2c -q -o jre.zip --checksum=sha-256=5382fa98bcc66fc3aef48792a3e84328eca16afa9bf97517527e92812516ee50 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/java21/21.0.6/linux.x86_64_21.0.6.zip \
        && aria2c -q -o commander.tar.bz --checksum=sha-256=3ba24eeaeb560e9db306a4d070e2bbe40b456701b4b87c53643a93ab1101b2c4 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/commander/1.24.1/Commander_linux_x86_64_1v24p1b1980.tar.bz; \
       fi \
    # slc runs the SDK's template generators with this Python, which it finds as an adapter
    # pack rather than on PATH
    && bsdtar -xf python.zip --strip-components=1 -C /opt/silabs/python \
    && bsdtar -xf slc.zip --strip-components=1 -C /opt/silabs/slc-cli \
    && bsdtar -xf zap.zip -C /opt/silabs/zap \
    && bsdtar -xf jre.zip --strip-components=1 -C /opt/silabs/java21 \
    && bsdtar -xf commander.tar.bz --strip-components=1 -C /opt/silabs/commander \
    && rm python.zip slc.zip zap.zip jre.zip commander.tar.bz

# The SDK is a conan package, but conan is not needed to fetch one: the recipe revision,
# package id and package revision are all discoverable over its REST API. Those revisions
# are content hashes, so the URL is already pinned to exactly these bytes.
FROM debian:trixie-slim AS silabs-sdk
ARG SDK_VERSION=2026.6.1
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
RUN set -eux \
    && apt-get update && apt-get install -y --no-install-recommends \
        aria2 ca-certificates jq libarchive-tools \
    && rm -rf /var/lib/apt/lists/* \
    && cd /tmp \
    && SDK="https://conan.silabs.net/v2/conans/simplicity-sdk/$SDK_VERSION/silabs/_" \
    && aria2c -q -o rrev.json "$SDK/latest" \
    && RREV=$(jq -r .revision rrev.json) \
    && aria2c -q -o pkgs.json "$SDK/revisions/$RREV/search" \
    && PKG=$(jq -r 'keys[0]' pkgs.json) \
    && aria2c -q -o prev.json "$SDK/revisions/$RREV/packages/$PKG/latest" \
    && PREV=$(jq -r .revision prev.json) \
    && aria2c -q -x4 -o sdk.tgz "$SDK/revisions/$RREV/packages/$PKG/revisions/$PREV/files/conan_package.tgz" \
    && mkdir -p "/simplicity_sdk_$SDK_VERSION" \
    && bsdtar -xf sdk.tgz -C "/simplicity_sdk_$SDK_VERSION" \
    && rm -f rrev.json pkgs.json prev.json sdk.tgz

# arm-none-eabi comes from xPack rather than Arm directly: it is the same GCC 14.2.1
# 20241119, but built against libzstd (which it bundles), and SiLabs ships
# zstd-compressed LTO bytecode in their precompiled stack libraries. Arm's own binaries
# omit zstd for AARCH64, which would otherwise mean rebuilding cc1/cc1plus/lto1 from
# source here.
FROM debian:trixie-slim AS arm-toolchains
ARG TARGETARCH
RUN set -eux \
    && apt-get update && apt-get install -y --no-install-recommends \
        aria2 ca-certificates libarchive-tools \
    && rm -rf /var/lib/apt/lists/* \
    && mkdir -p /opt/toolchains/gcc-arm-none-eabi /opt/toolchains/llvm-arm-none-eabi \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        aria2c -q -o gcc.tar.gz --checksum=sha-256=a1ac95c8d9347020d61e387e644a2c1806556b77162958a494d2f5f3d5fe7053 \
            https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v14.2.1-1.1/xpack-arm-none-eabi-gcc-14.2.1-1.1-linux-arm64.tar.gz \
        && aria2c -q -o atfe.tar.xz --checksum=sha-256=dfd93d7c79f26667f4baf7f388966aa4cbfd938bc5cbcf0ae064553faf3e9604 \
            https://github.com/arm/arm-toolchain/releases/download/release-21.1.1-ATfE/ATfE-21.1.1-Linux-AArch64.tar.xz; \
       else \
        aria2c -q -o gcc.tar.gz --checksum=sha-256=ed8c7d207a85d00da22b90cf80ab3b0b2c7600509afadf6b7149644e9d4790a6 \
            https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v14.2.1-1.1/xpack-arm-none-eabi-gcc-14.2.1-1.1-linux-x64.tar.gz \
        && aria2c -q -o atfe.tar.xz --checksum=sha-256=fd7fcc2eb4c88c53b71c45f9c6aa83317d45da5c1b51b0720c66f1ac70151e6e \
            https://github.com/arm/arm-toolchain/releases/download/release-21.1.1-ATfE/ATfE-21.1.1-Linux-x86_64.tar.xz; \
       fi \
    && aria2c -q -o nano.tar.xz --checksum=sha-256=7b70739b5f5ec0172b379de458daa97f063aa90f7eb1c5f543e2923a72dfce42 \
        https://github.com/arm/arm-toolchain/releases/download/release-21.1.1-ATfE/ATfE-newlib-nano-overlay-21.1.1.tar.xz \
    && aria2c -q -o newlib.tar.xz --checksum=sha-256=d9750863c5561c05a57f6df6019efea87e9206c0eef34c4e6441f339824cc908 \
        https://github.com/arm/arm-toolchain/releases/download/release-21.1.1-ATfE/ATfE-newlib-overlay-21.1.1.tar.xz \
    && bsdtar -xf gcc.tar.gz --strip-components=1 -C /opt/toolchains/gcc-arm-none-eabi \
    && bsdtar -xf atfe.tar.xz --strip-components=1 -C /opt/toolchains/llvm-arm-none-eabi \
    && bsdtar -xf nano.tar.xz -C /opt/toolchains/llvm-arm-none-eabi \
    && bsdtar -xf newlib.tar.xz -C /opt/toolchains/llvm-arm-none-eabi \
    && rm gcc.tar.gz atfe.tar.xz nano.tar.xz newlib.tar.xz

# Python virtual environment for the firmware builder script
FROM debian:trixie-slim AS python-venv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /usr/bin/
COPY requirements.txt /tmp/
RUN UV_PYTHON_INSTALL_DIR=/opt/pythons uv venv -p 3.14 /opt/venv --no-cache \
    && uv pip install --python /opt/venv -r /tmp/requirements.txt

# Final image
FROM debian:trixie-slim
ARG TARGETARCH
ARG SDK_VERSION=2026.6.1

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Install only runtime packages
RUN set -e \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       git \
       cmake \
       ninja-build \
       libstdc++6 \
       libgl1 \
       libpng16-16 \
       libpcre2-16-0 \
       libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/* \
    # Fix git permission error when building locally
    && git config --global --add safe.directory '*'

# Copy from parallel stages
COPY --from=python-venv /opt/pythons /opt/pythons
COPY --from=python-venv /opt/venv /opt/venv
COPY --from=silabs-tools /opt/silabs /opt/silabs
COPY --from=arm-toolchains /opt/toolchains /opt/toolchains
COPY --from=silabs-sdk /simplicity_sdk_${SDK_VERSION} /simplicity_sdk_${SDK_VERSION}
RUN set -eux \
    && mkdir -p /opt/silabs/bin \
    && ln -s /opt/silabs/java21/jre/bin/java /opt/silabs/bin/java \
    && ln -s /opt/silabs/commander/commander /opt/silabs/bin/commander \
    && ln -s /opt/silabs/zap/zap /opt/silabs/bin/zap \
    # slc uses $(dirname "$0") to find slc.jar, so it needs a wrapper rather than a symlink
    && printf '#!/bin/sh\nexec /opt/silabs/slc-cli/slc "$@"\n' > /opt/silabs/bin/slc \
    && chmod +x /opt/silabs/bin/slc

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV HOME=/root
ENV PATH="$PATH:/opt/silabs/bin"

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
