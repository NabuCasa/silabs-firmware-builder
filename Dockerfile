# syntax=docker/dockerfile:1

# Keep the tag date and snapshot ID equal: the dated image is built from that snapshot,
# and an index older than the rootfs produces dependency conflicts.
ARG DEBIAN_TAG=trixie-20260713-slim
ARG DEBIAN_DIGEST=sha256:020c0d20b9880058cbe785a9db107156c3c75c2ac944a6aa7ab59f2add76a7bd
ARG DEBIAN_SNAPSHOT=20260713T000000Z

FROM debian:${DEBIAN_TAG}@${DEBIAN_DIGEST} AS trixie-stable
ARG DEBIAN_SNAPSHOT
RUN set -eux \
    && printf '%s\n' \
        "APT::Snapshot \"${DEBIAN_SNAPSHOT}\";" \
        'Acquire::Check-Valid-Until "false";' \
        'Acquire::Snapshots::URI::Host::deb.debian.org "http://snapshot.debian.org/archive/@PATH@/@SNAPSHOTID@/";' \
        'Acquire::Snapshots::URI::Origin::Debian "http://snapshot.debian.org/archive/debian/@SNAPSHOTID@/";' \
        'Acquire::Snapshots::URI::Override::Label::Debian-Security "http://snapshot.debian.org/archive/debian-security/@SNAPSHOTID@/";' \
        > /etc/apt/apt.conf.d/80snapshot \
    && apt-get update

# slc-cli, ZAP, a JRE and a Python interpreter are published on Silicon Labs' update
# site as native builds for both architectures. Fetching them directly avoids `slt`,
# which is x86_64-only and would otherwise have to be emulated on ARM64.
FROM trixie-stable AS silabs-tools
ARG TARGETARCH
RUN set -eux \
    && apt-get install -y --no-install-recommends \
        aria2 ca-certificates libarchive-tools \
    && mkdir -p /opt/silabs/slc-cli /opt/silabs/zap /opt/silabs/java21 /opt/silabs/python \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        aria2c -q -o python.zip --checksum=sha-256=0bd0334fead1e3c2647b6c09b9801b214ab3b999e9e6a9d13553ff57c1e04bb2 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/python/3.10.3/python.3.10.gtk.linux.aarch64.zip \
        && aria2c -q -o slc.zip --checksum=sha-256=5d13e09e605e5dfb94abeb0773055647764bb30d746916f3fc3991a48282a6ba \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/slc-cli/6.0.22/slc-cli.linux.gtk.aarch64.zip \
        && aria2c -q -o zap.zip --checksum=sha-256=15c5263ea98a1162e655ad5fb154a4f0e62047b6382a889f9e986f570c30c45b \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/zap/2026.06.17/zap-linux-arm64.zip \
        && aria2c -q -o jre.zip --checksum=sha-256=61b4be111fe14d7a9138de920047489c679c7a697320cf69d28b23046edbaf73 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/java21/21.0.6/linux.aarch64_21.0.6.zip; \
       else \
        aria2c -q -o python.zip --checksum=sha-256=26f56b1cfa05b2b3b7dafb2c2a5e3c19498389c34dfe68be800f71f476c86363 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/python/3.10.3/python.3.10.3.gtk.linux.x86_64.zip \
        && aria2c -q -o slc.zip --checksum=sha-256=f05e078b369d7e7dbfa31bdd2ad8edf61f04a9291393deab4702137639ba7d19 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/slc-cli/6.0.22/slc-cli.linux.gtk.x86_64.zip \
        && aria2c -q -o zap.zip --checksum=sha-256=45537226973fb892894f7110288dd4a7db627728af8cd9fc7c27b658530e2b88 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/zap/2026.06.17/zap-linux-x64.zip \
        && aria2c -q -o jre.zip --checksum=sha-256=5382fa98bcc66fc3aef48792a3e84328eca16afa9bf97517527e92812516ee50 \
            https://updates.silabs.com/studio/v6/updates/update_site/archives/java21/21.0.6/linux.x86_64_21.0.6.zip; \
       fi \
    # slc runs the SDK's template generators with this Python, which it finds as an adapter
    # pack rather than on PATH
    && bsdtar -xf python.zip --strip-components=1 -C /opt/silabs/python \
    && bsdtar -xf slc.zip --strip-components=1 -C /opt/silabs/slc-cli \
    && bsdtar -xf zap.zip -C /opt/silabs/zap \
    && bsdtar -xf jre.zip --strip-components=1 -C /opt/silabs/java21 \
    && rm python.zip slc.zip zap.zip jre.zip

# The SDK is a conan package, but conan is not needed to fetch one: the recipe revision,
# package id and package revision are all discoverable over its REST API. Those revisions
# are content hashes, so the URL is already pinned to exactly these bytes.
FROM trixie-stable AS silabs-sdk
ARG SDK_VERSION=2026.6.1
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8
RUN set -eux \
    && apt-get install -y --no-install-recommends \
        aria2 ca-certificates jq libarchive-tools \
    && cd /tmp \
    && SDK="https://conan.silabs.net/v2/conans/simplicity-sdk/$SDK_VERSION/silabs/_" \
    && aria2c -q -o rrev.json "$SDK/latest" \
    && RREV=$(jq -r .revision rrev.json) \
    && aria2c -q -o pkgs.json "$SDK/revisions/$RREV/search" \
    && PKG=$(jq -r 'keys[0]' pkgs.json) \
    && aria2c -q -o prev.json "$SDK/revisions/$RREV/packages/$PKG/latest" \
    && PREV=$(jq -r .revision prev.json) \
    && aria2c -q -x4 -o sdk.tgz "$SDK/revisions/$RREV/packages/$PKG/revisions/$PREV/files/conan_package.tgz" \
    && mkdir -p "/opt/silabs/sdks/simplicity_sdk_$SDK_VERSION" \
    && bsdtar -xf sdk.tgz -C "/opt/silabs/sdks/simplicity_sdk_$SDK_VERSION" \
    && rm -f rrev.json pkgs.json prev.json sdk.tgz

# Arm publishes both toolchains that Silicon Labs repackages, including Arm Toolchain for Embedded (LLVM)
FROM trixie-stable AS arm-toolchains
ARG TARGETARCH
RUN set -eux \
    && apt-get install -y --no-install-recommends \
        aria2 ca-certificates libarchive-tools \
    && mkdir -p /opt/toolchains/llvm-arm-none-eabi \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        aria2c -q -o atfe.tar.xz --checksum=sha-256=dfd93d7c79f26667f4baf7f388966aa4cbfd938bc5cbcf0ae064553faf3e9604 \
            https://github.com/arm/arm-toolchain/releases/download/release-21.1.1-ATfE/ATfE-21.1.1-Linux-AArch64.tar.xz; \
       else \
        aria2c -q -o atfe.tar.xz --checksum=sha-256=fd7fcc2eb4c88c53b71c45f9c6aa83317d45da5c1b51b0720c66f1ac70151e6e \
            https://github.com/arm/arm-toolchain/releases/download/release-21.1.1-ATfE/ATfE-21.1.1-Linux-x86_64.tar.xz; \
       fi \
    && aria2c -q -o nano.tar.xz --checksum=sha-256=7b70739b5f5ec0172b379de458daa97f063aa90f7eb1c5f543e2923a72dfce42 \
        https://github.com/arm/arm-toolchain/releases/download/release-21.1.1-ATfE/ATfE-newlib-nano-overlay-21.1.1.tar.xz \
    && aria2c -q -o newlib.tar.xz --checksum=sha-256=d9750863c5561c05a57f6df6019efea87e9206c0eef34c4e6441f339824cc908 \
        https://github.com/arm/arm-toolchain/releases/download/release-21.1.1-ATfE/ATfE-newlib-overlay-21.1.1.tar.xz \
    && bsdtar -xf atfe.tar.xz --strip-components=1 -C /opt/toolchains/llvm-arm-none-eabi \
    && bsdtar -xf nano.tar.xz -C /opt/toolchains/llvm-arm-none-eabi \
    && bsdtar -xf newlib.tar.xz -C /opt/toolchains/llvm-arm-none-eabi \
    && rm atfe.tar.xz nano.tar.xz newlib.tar.xz

# Arm ships no libclang. bindgen parses the SDK headers with the upstream build of the
# same LLVM release, placed in the toolchain so that it resolves Arm's builtin headers.
RUN set -eux \
    && cd /tmp \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        aria2c -q -o llvm.tar.xz --checksum=sha-256=2764bb49ad4dab93226328d6374ca4466799bdc18372c544d8f6ebc1aa0c28a9 \
            https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.1/LLVM-21.1.1-Linux-ARM64.tar.xz; \
       else \
        aria2c -q -o llvm.tar.xz --checksum=sha-256=fe9886992273e469fbd664851cbee2f125b383664694684923a41af1c71b9632 \
            https://github.com/llvm/llvm-project/releases/download/llvmorg-21.1.1/LLVM-21.1.1-Linux-X64.tar.xz; \
       fi \
    && bsdtar -xf llvm.tar.xz --strip-components=2 -C /opt/toolchains/llvm-arm-none-eabi/lib 'LLVM-*/lib/libclang.so*' \
    && rm llvm.tar.xz

# The toolchain pinned in rust-toolchain.toml
FROM trixie-stable AS rust-toolchain
ARG TARGETARCH
ENV RUSTUP_HOME=/opt/rust/rustup
ENV CARGO_HOME=/opt/rust/cargo
COPY rust-toolchain.toml /tmp/rust/
RUN set -eux \
    && apt-get install -y --no-install-recommends \
        aria2 ca-certificates \
    && cd /tmp/rust \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        aria2c -q -o rustup-init --checksum=sha-256=15f6e4ce9f583b929c996c91562bad6d4454f3281de858b02cdfdef615fac433 \
            https://static.rust-lang.org/rustup/archive/1.29.1/aarch64-unknown-linux-gnu/rustup-init; \
       else \
        aria2c -q -o rustup-init --checksum=sha-256=dda7234360b7f578ca8b0ddcb80145646fa61a67c1720a5abc7051b35c9fcb71 \
            https://static.rust-lang.org/rustup/archive/1.29.1/x86_64-unknown-linux-gnu/rustup-init; \
       fi \
    && chmod +x rustup-init \
    && ./rustup-init -y --no-modify-path --profile minimal --default-toolchain none \
    && /opt/rust/cargo/bin/rustup toolchain install \
    && rm -rf /tmp/rust

# Python virtual environment for the firmware builder script
FROM trixie-stable AS python-venv
COPY --from=ghcr.io/astral-sh/uv:0.12.1 /uv /uvx /usr/bin/
COPY pyproject.toml uv.lock /tmp/project/
RUN UV_PYTHON_INSTALL_DIR=/opt/pythons UV_PROJECT_ENVIRONMENT=/opt/venv \
    uv sync --project /tmp/project --frozen --no-dev --no-cache -p 3.14.6

# This stage ships, so the index is bind-mounted rather than inherited: `FROM
# trixie-stable` would carry its 41 MB of lists into the image permanently.
FROM debian:${DEBIAN_TAG}@${DEBIAN_DIGEST}
ARG TARGETARCH

ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Install only runtime packages
COPY --from=trixie-stable /etc/apt/apt.conf.d/80snapshot /etc/apt/apt.conf.d/80snapshot
RUN --mount=type=bind,from=trixie-stable,source=/var/lib/apt/lists,target=/var/lib/apt/lists,rw \
    set -e \
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
       # Needed at runtime by the zstd-enabled cc1/cc1plus/lto1 swapped in below (ARM64)
       libzstd1 \
       # Host compiler and linker for cargo build scripts and proc macros
       gcc \
       libc6-dev

# Copy from parallel stages
COPY --from=python-venv /opt/pythons /opt/pythons
COPY --from=python-venv /opt/venv /opt/venv
COPY --from=silabs-tools /opt/silabs /opt/silabs
COPY --from=arm-toolchains /opt/toolchains /opt/toolchains
COPY --from=rust-toolchain /opt/rust /opt/rust
COPY --from=silabs-sdk /opt/silabs/sdks /opt/silabs/sdks
RUN set -eux \
    && mkdir -p /opt/silabs/bin \
    && ln -s /opt/silabs/java21/jre/bin/java /opt/silabs/bin/java \
    && ln -s /opt/silabs/zap/zap /opt/silabs/bin/zap \
    # slc uses $(dirname "$0") to find slc.jar, so it needs a wrapper rather than a symlink
    && printf '#!/bin/sh\nexec /opt/silabs/slc-cli/slc "$@"\n' > /opt/silabs/bin/slc \
    && chmod +x /opt/silabs/bin/slc \
    && git config --system --add safe.directory '*'

ENV HOME=/root
ENV RUSTUP_HOME=/opt/rust/rustup
ENV CARGO_HOME=/opt/rust/cargo
ENV PATH="$PATH:/opt/silabs/bin:/opt/rust/cargo/bin"
# bindgen's libclang, from the toolchain
ENV LIBCLANG_PATH=/opt/toolchains/llvm-arm-none-eabi/lib

# Firmware builds run offline: cache every Rust component's dependencies. The workspace
# is generated from the components' slcc files, so this needs the whole tree.
RUN --mount=type=bind,source=.,target=/tmp/repo \
    set -eux \
    && cd /tmp/repo \
    && /opt/venv/bin/python3 -m tools.rust_workspace fetch --workspace /tmp/rust-universe \
    && rm -rf /tmp/rust-universe

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "-m", "tools.build_project"]
