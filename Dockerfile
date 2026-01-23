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

# Silicon Labs Configurator (slc)
FROM slt-downloader AS slc-cli
RUN aria2c --checksum=sha-256=923107bb6aa477324efe8bc698a769be0ca5a26e2b7341f6177f5d3586453158 -o /tmp/slc.zip \
        "https://www.silabs.com/documents/public/software/slc_cli_linux.zip" \
    && mkdir /out && bsdtar -xf /tmp/slc.zip -C /out \
    && rm /tmp/slc.zip

# slc-cli hardcodes architectures internally and does not properly support ARM64 despite
# actually being fully compatible with it. It requires Python via JEP just for Jinja2
# template generation so we can install a standalone Python 3.10 and use that for JEP.
# For consistency across architectures, we compile and replace on x86-64 even though it
# is not necessary.
FROM debian:trixie-slim AS slc-python
RUN set -o pipefail \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       curl \
       clang \
       default-jdk-headless \
    && curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR="/usr/bin" sh \
    && uv python install 3.10 --no-cache \
    && mkdir -p /out/bin /out/lib \
    # Copy Python binary and stdlib
    && cp /root/.local/share/uv/python/cpython-3.10.*/bin/python3.10 /out/bin/python3 \
    && cp -R /root/.local/share/uv/python/cpython-3.10.*/lib/libpython3.10.so.1.0 /out/lib/ \
    && cp -R /root/.local/share/uv/python/cpython-3.10.*/lib/python3.10 /out/lib/ \
    # Python binary needs libpython in the same directory
    && cp /out/lib/libpython3.10.so.1.0 /out/bin/ \
    # Install JEP and dependencies (setuptools<69 required for JEP build)
    && echo "setuptools<69" > /tmp/uv_constraints.txt \
    && JAVA_HOME=/usr/lib/jvm/default-java \
       LIBRARY_PATH=/out/lib \
       uv pip install \
       --python /out/bin/python3 \
       --break-system-packages \
       --build-constraint /tmp/uv_constraints.txt \
       --no-cache \
       jep==4.1.1 jinja2==3.1.6 pyyaml==6.0.3 \
    # Mirror expected slc-cli folder structure
    && mkdir -p /out/jep \
    && cp /out/lib/python3.10/site-packages/jep/*.py /out/jep/ \
    && cp /out/lib/python3.10/site-packages/jep/jep.cpython-310-*-linux-gnu.so /out/jep/jep.so

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
       default-jre-headless \
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
COPY --from=slc-python /out /opt/slc_cli/bin/slc-cli/developer/adapter_packs/python
COPY --from=builder-venv /opt/venv /opt/venv
COPY --from=builder-venv /opt/pythons /opt/pythons

# We can run slc-cli without the native wrapper. For consistency across architectures,
# we create the same wrapper script on both.
RUN cat <<'EOF' > /usr/local/bin/slc && chmod +x /usr/local/bin/slc
#!/bin/sh
exec java \
    -jar /opt/slc_cli/bin/slc-cli/plugins/org.eclipse.equinox.launcher_*.jar \
    -install /opt/slc_cli/bin/slc-cli \
    -consoleLog \
    "$@"
EOF

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV PATH="$PATH:/opt/commander-cli:/opt/slc_cli"
ENV STUDIO_ADAPTER_PACK_PATH="/opt/zap"

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
