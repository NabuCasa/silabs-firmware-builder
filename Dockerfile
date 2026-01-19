FROM alpine:3.23 AS base-downloader
ARG TARGETARCH

# Simplicity SDK includes unicode characters in folder names and fails to unzip
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

RUN apk add --no-cache bzip2 ca-certificates curl libarchive-tools xz

# ============ Parallel download stages ============

# Simplicity SDK 2025.6.2
FROM base-downloader AS simplicity-sdk-v2025.6.2
RUN mkdir /out \
    && curl -L https://github.com/SiliconLabs/simplicity_sdk/releases/download/v2025.6.2/simplicity-sdk.zip \
       | bsdtar -xf - -C /out

# Gecko SDK 4.5.0
FROM base-downloader AS gecko-sdk-v4.5.0
RUN mkdir /out \
    && curl -L https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.5.0/gecko-sdk.zip \
       | bsdtar -xf - -C /out

# ZCL Advanced Platform (ZAP) v2025.12.02
FROM base-downloader AS zap
ARG TARGETARCH
RUN \
    apk add --no-cache jq \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        ARCH="arm64"; \
    else \
        ARCH="x64"; \
    fi \
    && mkdir /out \
    && curl -L "https://github.com/project-chip/zap/releases/download/v2025.12.02/zap-linux-${ARCH}.zip" \
       | bsdtar -xf - -C /out \
    && chmod +x /out/zap /out/zap-cli \
    # Patch ZAP apack.json to add missing linux.aarch64 executable definitions
    # Remove once https://github.com/project-chip/zap/pull/1677 is merged
    && jq '.executable["zap:linux.aarch64"]     = {"exe": "zap",     "optional": true} \
         | .executable["zap-cli:linux.aarch64"] = {"exe": "zap-cli", "optional": true}' \
        /out/apack.json > /tmp/apack.json && mv /tmp/apack.json /out/apack.json

# GCC Embedded Toolchain 12.2.rel1
FROM base-downloader AS gcc-embedded-toolchain
ARG TARGETARCH
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        ARCH="aarch64"; \
    else \
        ARCH="x86_64"; \
    fi \
    && mkdir /out \
    && curl -L "https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-${ARCH}-arm-none-eabi.tar.xz" \
       | tar -C /out -xJf -

# Simplicity Commander CLI
FROM base-downloader AS commander
ARG TARGETARCH
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        ARCH="aarch64"; \
    else \
        ARCH="x86_64"; \
    fi \
    && curl -L --compressed -H 'User-Agent: Firefox/143' -H 'Accept-Language: *' https://www.silabs.com/documents/public/software/SimplicityCommander-Linux.zip \
       | bsdtar -xf - \
    && mkdir /out \
    && tar -C /out -xjf SimplicityCommander-Linux/Commander-cli_linux_${ARCH}_*.tar.bz \
    && rm -r SimplicityCommander-Linux \
    && ln -s commander-cli /out/commander-cli/commander

# Silicon Labs Configurator (slc)
FROM base-downloader AS slc-cli
RUN mkdir /out \
    && curl -L --compressed -H 'User-Agent: Firefox/143' -H 'Accept-Language: *' https://www.silabs.com/documents/public/software/slc_cli_linux.zip \
       | bsdtar -xf - -C /out

# slc-cli hardcodes architectures internally and does not properly support ARM64 despite
# actually being fully compatible with it. It requires Python via JEP just for Jinja2
# template generation so we can install a standalone Python 3.10 and use that for JEP.
# For consistency across architectures, we compile and replace on x86-64 even though it
# is not necessary.
FROM debian:trixie-slim AS slc-python
RUN \
    apt-get update \
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
RUN \
    apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl \
    && curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR="/usr/bin" sh \
    && UV_PYTHON_INSTALL_DIR=/opt/pythons uv venv -p 3.13 /opt/venv --no-cache \
    && uv pip install --python /opt/venv -r /tmp/requirements.txt

# ============ Final image ============

FROM debian:trixie-slim

# Copy all downloaded artifacts from parallel stages
COPY --from=simplicity-sdk-v2025.6.2 /out /simplicity_sdk_2025.6.2
COPY --from=gecko-sdk-v4.5.0 /out /gecko_sdk_4.5.0
COPY --from=zap /out /opt/zap
COPY --from=gcc-embedded-toolchain /out /opt
COPY --from=commander /out /opt
COPY --from=slc-cli /out /opt
COPY --from=slc-python /out /opt/slc_cli/bin/slc-cli/developer/adapter_packs/python
COPY --from=builder-venv /opt/venv /opt/venv
COPY --from=builder-venv /opt/pythons /opt/pythons

# Install runtime packages
RUN \
    apt-get update \
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

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV PATH="$PATH:/opt/commander-cli:/opt/slc_cli"
ENV STUDIO_ADAPTER_PACK_PATH="/opt/zap"

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

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
