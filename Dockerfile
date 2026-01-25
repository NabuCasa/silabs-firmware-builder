# Build box64 for running x86_64 binaries on ARM64.
#
# The purpose of this is to emulate `slt` and `conan`, the new Silicon Labs tooling for
# setting up development tools. These tools are not available for ARM64 Linux but
# strangely all other packages are, meaning we can just emulate the downloading part
# and still have native performance for all compilation.
#
# box64 is significantly faster than QEMU for this workload (~4x). However, both QEMU and
# box64 are affected by a Go runtime issue (https://github.com/golang/go/issues/69255)
# where Go assumes AMD64's 48-bit sign-extended addresses, but ARM64 can allocate
# addresses up to the full 48-bit range. We patch box64 to force 47-bit address mode,
# constraining allocations to the canonical AMD64 low-half address space.
#
# We can remove this emulation layer once SiLabs releases ARM64 Linux builds of `slt`
# and `conan`.
FROM debian:trixie-slim AS box64-builder
ARG TARGETARCH

RUN set -e \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        apt-get update && apt-get install -y git cmake gcc g++ make python3 \
        && rm -rf /var/lib/apt/lists/* \
        && git clone https://github.com/ptitSeb/box64 /box64 && cd /box64 \
        && git checkout 0eb0f9decb0b7e560005b30c11e019f18805eec2; \
    fi

# Fix __pread64_chk/__xmknod/__xmknodat missing for static builds
# Force 47-bit address space to fix Go runtime issue (https://github.com/golang/go/issues/69255)
RUN <<EOF
if [ "$TARGETARCH" = "arm64" ]; then
    cd /box64 && git apply --unidiff-zero <<'PATCH'
diff --git a/src/custommem.c b/src/custommem.c
--- a/src/custommem.c
+++ b/src/custommem.c
@@ -2718,2 +2717,0 @@
-            if(s>0x7fff00000000LL)
-                have48bits = 1;
diff --git a/src/wrapped/wrappedlibc_private.h b/src/wrapped/wrappedlibc_private.h
--- a/src/wrapped/wrappedlibc_private.h
+++ b/src/wrapped/wrappedlibc_private.h
@@ -1529 +1528,0 @@
-#ifdef LA64
@@ -1531,3 +1529,0 @@
-#else
-GO(__pread64_chk, lFipLlL)
-#endif
@@ -2633,4 +2628,0 @@
-#ifdef STATICBUILD
-//GO(__xmknod, iFipup)
-//GO(__xmknodat, iFiipup)
-#else
@@ -2639 +2630,0 @@
-#endif
PATCH
fi
EOF

RUN set -e \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        mkdir -p /box64/build && cd /box64/build \
        && cmake .. -DARM64=1 -DSTATICBUILD=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        && make -j$(nproc); \
    else \
        mkdir -p /box64/build && touch /box64/build/box64; \
    fi

# The new `slt` tool does not provide Gecko SDK builds so we have to download it ourselves.
FROM debian:trixie-slim AS gecko-sdk-v4.5.0
RUN apt-get update && apt-get install -y --no-install-recommends aria2 ca-certificates libarchive-tools \
    && rm -rf /var/lib/apt/lists/* \
    && aria2c --checksum=sha-256=b5b2b2410eac0c9e2a72320f46605ecac0d376910cafded5daca9c1f78e966c8 -o /tmp/sdk.zip \
        https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.5.0/gecko-sdk.zip \
    && mkdir /out && bsdtar -xf /tmp/sdk.zip -C /out \
    && rm /tmp/sdk.zip

# Python virtual environment for the firmware builder script
FROM debian:trixie-slim AS python-venv
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /usr/bin/
COPY requirements.txt /tmp/
RUN UV_PYTHON_INSTALL_DIR=/opt/pythons uv venv -p 3.13 /opt/venv --no-cache \
    && uv pip install --python /opt/venv -r /tmp/requirements.txt

# Install slt and all toolchain packages (depends on box64 for ARM64)
FROM debian:trixie-slim AS slt-toolchain
ARG TARGETARCH

# Copy box64 for x86_64 emulation (only used on ARM64)
COPY --from=box64-builder /box64/build/box64 /usr/bin/box64

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
    # slt-cli is x64 only but runs fine with box64
    && aria2c --checksum=sha-256=8c2dd5091c15d5dd7b8fc978a512c49d9b9c5da83d4d0b820cfe983b38ef3612 -o /tmp/slt.zip \
        https://www.silabs.com/documents/public/software/slt-cli-1.1.0-linux-x64.zip \
    && bsdtar -xf /tmp/slt.zip -C /usr/bin && chmod +x /usr/bin/slt && rm /tmp/slt.zip \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        dpkg --add-architecture amd64 \
        && apt-get update \
        && apt-get install -y --no-install-recommends libc6:amd64 zlib1g:amd64 \
        && rm -rf /var/lib/apt/lists/* \
        # slt-cli and conan are x86_64 only, need box64 on ARM64
        && mv /usr/bin/slt /usr/bin/slt-bin \
        && printf '#!/bin/sh\nBOX64_LOG=0 BOX64_PROFILE=safer exec /usr/bin/box64 /usr/bin/slt-bin "$@"\n' > /usr/bin/slt \
        && chmod +x /usr/bin/slt \
        # Install conan
        && slt --non-interactive install conan \
        # Wrap conan_engine with box64
        && mv /root/.silabs/slt/engines/conan/conan_engine /root/.silabs/slt/engines/conan/conan_engine-bin \
        && printf '#!/bin/sh\nBOX64_LOG=0 BOX64_PROFILE=safer exec /usr/bin/box64 /root/.silabs/slt/engines/conan/conan_engine-bin "$@"\n' > /root/.silabs/slt/engines/conan/conan_engine \
        && chmod +x /root/.silabs/slt/engines/conan/conan_engine \
        # Patch slt to select ARM64 packages for subsequent installs
        && sed -i 's/amd6/arm6/g' /usr/bin/slt-bin \
        # Force conan to use the ARM64 profile for downloading packages
        && cp /root/.silabs/slt/installs/conan/profiles/linux_arm64 /root/.silabs/slt/installs/conan/profiles/default; \
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
    # Install x86_64 libraries for box64 on ARM64
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
COPY --from=slt-toolchain /usr/bin/slt* /usr/bin/
COPY --from=slt-toolchain /usr/bin/box64 /usr/bin/
COPY --from=slt-toolchain /root/.silabs /root/.silabs

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV HOME=/root
ENV PATH="$PATH:/root/.silabs/slt/bin"

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
