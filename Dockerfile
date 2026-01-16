FROM debian:trixie AS base-downloader
ARG TARGETARCH

# Simplicity SDK includes unicode characters in folder names and fails to unzip
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

RUN apt-get update && apt-get install -y --no-install-recommends \
    bzip2 ca-certificates curl unzip xz-utils

# ============ Parallel download stages ============

# Simplicity SDK 2025.6.2
FROM base-downloader AS simplicity-sdk
RUN curl -o sdk.zip -L https://github.com/SiliconLabs/simplicity_sdk/releases/download/v2025.6.2/simplicity-sdk.zip \
    && unzip -q -d /out sdk.zip

# Gecko SDK 4.5.0
FROM base-downloader AS gecko-sdk
RUN curl -o sdk.zip -L https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.5.0/gecko-sdk.zip \
    && unzip -q -d /out sdk.zip

# ZCL Advanced Platform (ZAP) v2025.12.02
FROM base-downloader AS zap
ARG TARGETARCH
RUN \
    apt-get install -y --no-install-recommends jq \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        ARCH="arm64"; \
    else \
        ARCH="x64"; \
    fi \
    && curl -o zap.zip -L "https://github.com/project-chip/zap/releases/download/v2025.12.02/zap-linux-${ARCH}.zip" \
    && unzip -q -d /out zap.zip \
    # Patch ZAP apack.json to add missing linux.aarch64 executable definitions
    # Remove once https://github.com/project-chip/zap/pull/1677 is merged
    && jq '.executable["zap:linux.aarch64"]     = {"exe": "zap",     "optional": true} \
         | .executable["zap-cli:linux.aarch64"] = {"exe": "zap-cli", "optional": true}' \
        /out/apack.json > /tmp/apack.json && mv /tmp/apack.json /out/apack.json

# GCC Embedded Toolchain 12.2.rel1
FROM base-downloader AS toolchain
ARG TARGETARCH
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        ARCH="aarch64"; \
    else \
        ARCH="x86_64"; \
    fi \
    && curl -O "https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-${ARCH}-arm-none-eabi.tar.xz" \
    && mkdir /out \
    && tar -C /out -xf arm-gnu-toolchain-12.2.rel1-${ARCH}-arm-none-eabi.tar.xz \
    && ln -s /out/arm-gnu-toolchain-12.2.rel1-${ARCH}-arm-none-eabi /out/arm-gnu-toolchain-12.2.rel1-arm-none-eabi

# Simplicity Commander CLI
FROM base-downloader AS commander
ARG TARGETARCH
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        ARCH="aarch64"; \
    else \
        ARCH="x86_64"; \
    fi \
    && curl -L -O --compressed -H 'User-Agent: Firefox/143' -H 'Accept-Language: *' https://www.silabs.com/documents/public/software/SimplicityCommander-Linux.zip \
    && unzip -q SimplicityCommander-Linux.zip \
    && mkdir /out \
    && tar -C /out -xjf SimplicityCommander-Linux/Commander-cli_linux_${ARCH}_*.tar.bz \
    && ln -s /out/commander-cli/commander-cli /out/commander-cli/commander

# Silicon Labs Configurator (slc)
FROM base-downloader AS slc-cli
RUN curl -L -O --compressed -H 'User-Agent: Firefox/143' -H 'Accept-Language: *' https://www.silabs.com/documents/public/software/slc_cli_linux.zip \
    && unzip -q -d /out slc_cli_linux.zip

# slc-cli hardcodes architectures internally and does not properly support ARM64 despite
# actually being fully compatible with it. It requires Python via JEP just for Jinja2
# template generation so we can install a standalone Python 3.10 and use that for JEP.
# For consistency across architectures, we compile and symlink on x86-64 even though it
# is not necessary.
FROM debian:trixie AS slc-python
RUN \
    apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       curl \
       clang \
       default-jdk-headless \
    && curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR="/usr/bin" sh \
    && uv python install 3.10 --no-cache \
    && cp -R /root/.local/share/uv/python/cpython-3.10.* /out \
    && echo "setuptools<69" > /out/uv_constraints.txt \
    && JAVA_HOME=/usr/lib/jvm/default-java \
       LIBRARY_PATH=/out/lib \
       uv pip install \
       --python /out \
       --break-system-packages \
       --build-constraint /out/uv_constraints.txt \
       --no-cache \
       jep==4.1.1 jinja2==3.1.6 pyyaml==6.0.3 \
    && mkdir -p /out/jep \
    # Create symlinks for JEP shared library and Python shared library
    && ln -sf /out/lib/libpython3.10.so.1.0 /out/bin/libpython3.10.so.1.0 \
    && ln -sf /out/lib/python3.10/site-packages/jep/jep.cpython-310-*-linux-gnu.so /out/jep/jep.so

# ============ Final image ============

FROM debian:trixie

ARG TARGETARCH
ARG DEBIAN_FRONTEND=noninteractive
ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Simplicity SDK includes unicode characters in folder names
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Copy all downloaded artifacts from parallel stages
COPY --from=simplicity-sdk /out /simplicity_sdk_2025.6.2
COPY --from=gecko-sdk /out /gecko_sdk_4.5.0
COPY --from=zap /out /opt/zap
COPY --from=toolchain /out /opt
COPY --from=commander /out /opt
COPY --from=slc-cli /out /opt
COPY --from=slc-python /out /opt/slc_python

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
    && curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR="/usr/bin" sh \
    && rm -rf /var/lib/apt/lists/*

# Remove bundled Python from slc-cli (we provide our own via STUDIO_PYTHON3_PATH)
RUN rm -rf /opt/slc_cli/bin/slc-cli/developer/adapter_packs/python

# Set up Python virtual environment for firmware builder script
COPY requirements.txt /tmp/

# uv will try to install into /root/, which is not accessible from the builder user
ENV UV_PYTHON_INSTALL_DIR=/opt/pythons

RUN \
    uv venv -p 3.13 /opt/venv --no-cache \
    && uv pip install --python /opt/venv -r /tmp/requirements.txt \
    && rm /tmp/requirements.txt

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV PATH="$PATH:/opt/commander-cli:/opt/slc_cli"
ENV STUDIO_ADAPTER_PACK_PATH="/opt/zap"
ENV STUDIO_PYTHON3_PATH="/opt/slc_python"

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

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

USER $USERNAME
WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
