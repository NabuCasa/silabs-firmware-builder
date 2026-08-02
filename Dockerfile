# FEX emulates the x86_64-only `slt` and `conan` binaries on ARM64. Debian has no
# fex-emu package and the Ubuntu PPA keeps only the newest build per series, so we take
# Fedora's. It links against a newer libfmt than Debian ships, so that comes along too.
#
# We can remove this once SiLabs releases builds of `slt` and `conan` for ARM64 Linux.
FROM fedora:42 AS fex-builder
ARG TARGETARCH
RUN mkdir -p /fex/bin /fex/lib \
    && if [ "$TARGETARCH" = "arm64" ]; then set -eux \
        && dnf -y install --setopt=install_weak_deps=False fex-emu \
        && cp /usr/bin/FEX /usr/bin/FEXServer /fex/bin/ \
        && cp -aL /usr/lib64/libfmt.so.11 /fex/lib/ \
        && dnf clean all; \
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

# Install slt and all toolchain packages (depends on FEX for ARM64)
FROM debian:trixie-slim AS slt-toolchain
ARG TARGETARCH

# Copy FEX (only used on ARM64)
COPY --from=fex-builder /fex /tmp/fex

# Set up slt and conan
RUN set -e \
    && apt-get update && apt-get install -y --no-install-recommends \
        aria2 \
        ca-certificates \
        # Required by conan
        libarchive-tools \
        bzip2 \
        unzip \
        jq \
    && rm -rf /var/lib/apt/lists/* \
    # slt-cli is x64 only but runs fine with FEX
    && aria2c --checksum=sha-256=2b9941216a3549aea6c5cc76565e2bc91ebfd9f41bec1e026341ce47c3aca1d0 -o slt.zip \
        https://www.silabs.com/documents/public/software/slt-cli-1.1.2-linux-x64.zip \
    && bsdtar -xf slt.zip -C /usr/bin && rm slt.zip \
    && chmod +x /usr/bin/slt \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        cp /tmp/fex/bin/* /usr/bin/ \
        && cp /tmp/fex/lib/* /usr/lib/aarch64-linux-gnu/ \
        && dpkg --add-architecture amd64 \
        && apt-get update \
        && apt-get install -y --no-install-recommends libc6:amd64 zlib1g:amd64 \
        && rm -rf /var/lib/apt/lists/* \
        # slt needs to be emulated. FEX intercepts the execve() of conan_engine during
        # installation and re-runs it under FEX, so no wrapper is needed for it. Native
        # tools (tar, etc.) that slt spawns are passed straight through to the kernel.
        #
        # GODEBUG=asyncpreemptoff=1 is needed for correctness, not speed: FEX mishandles
        # the SIGURG Go uses to preempt goroutines and `slt` corrupts its own memory in
        # most runs without it. Exported so the conan_engine child inherits it.
        && mv /usr/bin/slt /usr/bin/slt-bin \
        && printf '#!/bin/sh\nexport GODEBUG=asyncpreemptoff=1\nexec /usr/bin/FEX /usr/bin/slt-bin "$@"\n' > /usr/bin/slt \
        && chmod +x /usr/bin/slt \
        # Install conan
        && slt --non-interactive install conan \
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
    fi \
    && rm -rf /tmp/fex

# Silicon Labs has suddenly stopped serving binary ARM64 packages for Windows and Linux.
# Rebuild them from the upstream recipes. They rely on OSS binary distributions and are
# binary-identical to the SiLabs builds.
RUN if [ "$TARGETARCH" = "arm64" ]; then set -eux \
    && export CONAN_HOME=/root/.silabs/slt/installs/conan \
    && CONAN=/root/.silabs/slt/engines/conan/conan/conan \
    && for spec in \
        "cmake:3.30.2" \
        "ninja:1.12.1" \
        "gcc-arm-none-eabi:14.2.rel1" \
        "llvm-arm-toolchain-for-embedded:21.1.1" \
       ; do \
        n="${spec%%:*}"; v="${spec#*:}"; u="https://conan.silabs.net/v2/conans/$n/$v/silabs/_" \
        && mkdir -p "/rebuild/$n" && cd "/rebuild/$n" \
        && aria2c -q -o latest.json "$u/latest" \
        && r=$(jq -r .revision latest.json) \
        && aria2c -q -o conanfile.py "$u/revisions/$r/files/conanfile.py" \
        # gcc-arm-none-eabi is the one recipe that pulls from SiLabs' internal
        # Artifactory (artifactory-local.silabs.net, not publicly resolvable).
        && if [ "$n" = "gcc-arm-none-eabi" ]; then \
             sed -i 's|linux_url = f"{artifactory_path}/gcc-arm-none-eabi-{package_version}-linux"|linux_url = f"https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/{package_version}/binrel"|' conanfile.py \
             && grep -q armkeil conanfile.py; \
           fi \
        # gcc-arm-none-eabi's recipe derives its version from `git describe` when none
        # is given, which would self-name the package 0.0.1-initial-build.
        && "$CONAN" export . --version="$v" --user=silabs \
        && "$CONAN" install --requires="$n/$v@silabs" -pr:h=default -pr:b=default --build="$n/*" \
       ; done \
    # cd out of /rebuild first: conan resolves a workspace folder by walking up from the
    # cwd, and dies with FileNotFoundError if the cwd has been deleted underneath it.
    && cd / \
    && rm -rf /rebuild \
    # Drop conan's retained build/source trees in the same layer that created them,
    # otherwise the ~1.1 GB stays in this layer no matter where it is deleted later.
    # This also leaves a single copy of the ARM toolchain, so the cc1/cc1plus/lto1 swap
    # in the final stage cannot patch a build-folder copy instead of the package.
    && "$CONAN" cache clean --source --build --temp; \
    fi

# Install toolchain via slt
RUN set -e \
    && slt --non-interactive install \
        cmake/3.30.2 \
        ninja/1.12.1 \
        commander/1.24.1 \
        slc-cli/6.0.22 \
        simplicity-sdk/2026.6.1 \
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
    # Install x86_64 libraries for FEX on ARM64
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
COPY --from=fex-builder /fex /tmp/fex
COPY --from=slt-toolchain /usr/bin/slt* /usr/bin/
COPY --from=slt-toolchain /root/.silabs /root/.silabs
COPY --from=zstd-gcc-builder /opt/zstd-gcc /tmp/zstd-gcc
RUN if [ "$TARGETARCH" = "arm64" ]; then set -eux \
        && cp /tmp/fex/bin/* /usr/bin/ \
        && cp /tmp/fex/lib/* /usr/lib/aarch64-linux-gnu/ \
        && d="$(dirname "$(find /root/.silabs -path '*/libexec/gcc/arm-none-eabi/*' -name lto1 | head -1)")" \
        && cp /tmp/zstd-gcc/cc1 /tmp/zstd-gcc/cc1plus /tmp/zstd-gcc/lto1 "$d/"; \
    fi \
    && rm -rf /tmp/fex /tmp/zstd-gcc

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV HOME=/root
ENV PATH="$PATH:/root/.silabs/slt/bin"

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
