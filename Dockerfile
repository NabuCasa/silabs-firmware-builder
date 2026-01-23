FROM debian:trixie-slim AS slt-downloader
ARG TARGETARCH

# Simplicity SDK includes unicode characters in folder names and fails to unzip
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
        aria2 \
        bzip2 \
        ca-certificates \
        curl \
        libarchive-tools \
        unzip \
        xz-utils \
    && rm -rf /var/lib/apt/lists/* \
    # Download slt-cli (x64 only, runs under QEMU on ARM64)
    && curl -sL -o /tmp/slt.zip "https://www.silabs.com/documents/public/software/slt-cli-1.1.0-linux-x64.zip" \
    && unzip -q /tmp/slt.zip -d /usr/bin && chmod +x /usr/bin/slt && rm /tmp/slt.zip \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        # Patch slt to select ARM64 packages instead of x86_64
        sed -i 's/amd6/arm6/g' /usr/bin/slt \
        # Install x86_64 libraries and QEMU for running slt under emulation
        && dpkg --add-architecture amd64 \
        && apt-get update \
        && apt-get install -y --no-install-recommends libc6:amd64 zlib1g:amd64 \
        # Newer QEMU versions have Go runtime bugs, use 7.0.0 directly
        && curl -sL https://github.com/tonistiigi/binfmt/releases/download/deploy%2Fv7.0.0-28/qemu_v7.0.0_linux-arm64.tar.gz \
            | tar -xz -C /usr/bin qemu-x86_64 \
        && chmod +x /usr/bin/qemu-x86_64 \
        && rm -rf /var/lib/apt/lists/* \
        # Create wrapper script for transparent QEMU invocation
        && mv /usr/bin/slt /usr/bin/slt-bin \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64 /usr/bin/slt-bin "$@"\n' > /usr/bin/slt \
        && chmod +x /usr/bin/slt; \
    fi

# ============ Parallel download stages ============

# Simplicity SDK 2025.6.2
FROM slt-downloader AS simplicity-sdk-v2025.6.2
RUN aria2c --checksum=sha-256=463021f42ab1b4eeb1ca69660d3e8fccda4db76bd95b9cccec2c2bcba87550de -o /tmp/sdk.zip \
        https://github.com/SiliconLabs/simplicity_sdk/releases/download/v2025.6.2/simplicity-sdk.zip \
    && mkdir /out && bsdtar -xf /tmp/sdk.zip -C /out \
    && rm /tmp/sdk.zip

# Gecko SDK 4.5.0
FROM slt-downloader AS gecko-sdk-v4.5.0
RUN aria2c --checksum=sha-256=b5b2b2410eac0c9e2a72320f46605ecac0d376910cafded5daca9c1f78e966c8 -o /tmp/sdk.zip \
        https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.5.0/gecko-sdk.zip \
    && mkdir /out && bsdtar -xf /tmp/sdk.zip -C /out \
    && rm /tmp/sdk.zip

# ZCL Advanced Platform (ZAP) v2025.12.02
FROM slt-downloader AS zap
ARG TARGETARCH
RUN apt-get update && apt-get install -y --no-install-recommends jq && rm -rf /var/lib/apt/lists/* \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        ARCH="arm64"; \
        CHECKSUM="b9e64d4c3bd1796205bd2729ed8d6900f60b675c2d3fd94b6339713f8a1df1e6"; \
    else \
        ARCH="x64"; \
        CHECKSUM="0f6d66a1cecfb053b02de5951911ea4b596c417007cf48a903590e31823d44fa"; \
    fi \
    && aria2c --checksum=sha-256=$CHECKSUM -o /tmp/zap.zip \
        "https://github.com/project-chip/zap/releases/download/v2025.12.02/zap-linux-${ARCH}.zip" \
    && mkdir /out && bsdtar -xf /tmp/zap.zip -C /out && rm /tmp/zap.zip \
    && chmod +x /out/zap /out/zap-cli \
    # Patch ZAP apack.json to add missing linux.aarch64 executable definitions
    # Remove once https://github.com/project-chip/zap/pull/1677 is merged
    && jq '.executable["zap:linux.aarch64"]     = {"exe": "zap",     "optional": true} \
         | .executable["zap-cli:linux.aarch64"] = {"exe": "zap-cli", "optional": true}' \
        /out/apack.json > /tmp/apack.json && mv /tmp/apack.json /out/apack.json

# GCC Embedded Toolchain 12.2.rel1
FROM slt-downloader AS gcc-embedded-toolchain
ARG TARGETARCH
RUN if [ "$TARGETARCH" = "arm64" ]; then \
        ARCH="aarch64"; \
        CHECKSUM="7ee332f7558a984e239e768a13aed86c6c3ac85c90b91d27f4ed38d7ec6b3e8c"; \
    else \
        ARCH="x86_64"; \
        CHECKSUM="84be93d0f9e96a15addd490b6e237f588c641c8afdf90e7610a628007fc96867"; \
    fi \
    && aria2c --checksum=sha-256=$CHECKSUM -o /tmp/toolchain.tar.xz \
        "https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-${ARCH}-arm-none-eabi.tar.xz" \
    && mkdir /out && tar -C /out -xJf /tmp/toolchain.tar.xz \
    && rm /tmp/toolchain.tar.xz

# Simplicity Commander CLI
FROM slt-downloader AS commander
ARG TARGETARCH
RUN if [ "$TARGETARCH" = "arm64" ]; then \
        ARCH="aarch64"; \
    else \
        ARCH="x86_64"; \
    fi \
    && aria2c --checksum=sha-256=13a46f16edce4a9854df13a6226a978b43f958abfeb23bb5871580e6481ed775 -o /tmp/commander.zip \
        "https://www.silabs.com/documents/public/software/SimplicityCommander-Linux.zip" \
    && bsdtar -xf /tmp/commander.zip && rm /tmp/commander.zip \
    && mkdir /out \
    && tar -C /out -xjf SimplicityCommander-Linux/Commander-cli_linux_${ARCH}_*.tar.bz \
    && rm -r SimplicityCommander-Linux \
    && ln -s commander-cli /out/commander-cli/commander

# Silicon Labs Configurator (slc) via slt
FROM slt-downloader AS slc-cli
RUN slt help install slc-cli \
    && mkdir -p /out \
    && cp -r "$(slt help where slc-cli)" /out/slc_cli \
    && cp -r "$(slt help where python)" /out/python \
    && cp -r "$(slt help where java21)" /out/java21

# Python virtual environment for the firmware builder script
FROM debian:trixie-slim AS builder-venv
COPY requirements.txt /tmp/
RUN set -o pipefail \
    && apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl \
    && curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR="/usr/bin" sh \
    && UV_PYTHON_INSTALL_DIR=/opt/pythons uv venv -p 3.13 /opt/venv --no-cache \
    && uv pip install --python /opt/venv -r /tmp/requirements.txt

# ============ Final image ============

FROM debian:trixie-slim

# Install runtime packages
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       # For Simplicity Commander
       libglib2.0-0 \
       libpcre2-16-0 \
       # For SLC
       cmake \
       ninja-build \
       # For build script
       git \
    && rm -rf /var/lib/apt/lists/*

# Copy all downloaded artifacts from parallel stages
COPY --from=gcc-embedded-toolchain /out /opt
COPY --from=commander /out /opt
COPY --from=simplicity-sdk-v2025.6.2 /out /simplicity_sdk_2025.6.2
COPY --from=gecko-sdk-v4.5.0 /out /gecko_sdk_4.5.0
COPY --from=zap /out /opt/zap
COPY --from=slc-cli /out /opt
COPY --from=builder-venv /opt/venv /opt/venv
COPY --from=builder-venv /opt/pythons /opt/pythons

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV PATH="$PATH:/opt/commander-cli:/opt/slc_cli:/opt/java21/jre/bin"
ENV STUDIO_ADAPTER_PACK_PATH="/opt/zap"

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
