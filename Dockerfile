FROM debian:trixie

ARG TARGETARCH
ARG DEBIAN_FRONTEND=noninteractive
ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Simplicity SDK includes unicode characters in folder names and fails to unzip
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Install minimal packages needed for downloading
RUN \
    apt-get update \
    && apt-get install -y --no-install-recommends \
       bzip2 \
       ca-certificates \
       curl \
       unzip \
       xz-utils

# Simplicity SDK 2025.6.2
RUN \
    curl -o simplicity_sdk_2025.6.2.zip -L https://github.com/SiliconLabs/simplicity_sdk/releases/download/v2025.6.2/simplicity-sdk.zip \
    && unzip -q -d simplicity_sdk_2025.6.2 simplicity_sdk_2025.6.2.zip \
    && rm simplicity_sdk_2025.6.2.zip

# Gecko SDK 4.5.0
RUN \
    curl -o gecko_sdk_4.5.0.zip -L https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.5.0/gecko-sdk.zip \
    && unzip -q -d gecko_sdk_4.5.0 gecko_sdk_4.5.0.zip \
    && rm gecko_sdk_4.5.0.zip

# ZCL Advanced Platform (ZAP) v2025.12.02 (architecture-specific)
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        ZAP_ARCH="arm64"; \
    else \
        ZAP_ARCH="x64"; \
    fi \
    && curl -o zap.zip -L "https://github.com/project-chip/zap/releases/download/v2025.12.02/zap-linux-${ZAP_ARCH}.zip" \
    && unzip -q -d /opt/zap zap.zip \
    && rm zap.zip

# GCC Embedded Toolchain 12.2.rel1
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        TOOLCHAIN_ARCH="aarch64"; \
    else \
        TOOLCHAIN_ARCH="x86_64"; \
    fi \
    && curl -O "https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-${TOOLCHAIN_ARCH}-arm-none-eabi.tar.xz" \
    && tar -C /opt -xf arm-gnu-toolchain-12.2.rel1-${TOOLCHAIN_ARCH}-arm-none-eabi.tar.xz \
    && rm arm-gnu-toolchain-12.2.rel1-${TOOLCHAIN_ARCH}-arm-none-eabi.tar.xz \
    && ln -s /opt/arm-gnu-toolchain-12.2.rel1-${TOOLCHAIN_ARCH}-arm-none-eabi /opt/arm-gnu-toolchain-12.2.rel1-arm-none-eabi

# Install Simplicity Commander CLI
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        COMMANDER_ARCH="aarch64"; \
    else \
        COMMANDER_ARCH="x86_64"; \
    fi \
    && curl -L -O --compressed -H 'User-Agent: Firefox/143' -H 'Accept-Language: *' https://www.silabs.com/documents/public/software/SimplicityCommander-Linux.zip \
    && unzip -q SimplicityCommander-Linux.zip \
    && tar -C /opt -xjf SimplicityCommander-Linux/Commander-cli_linux_${COMMANDER_ARCH}_*.tar.bz \
    && chmod -R a+rX /opt/commander-cli \
    && ln -s /opt/commander-cli/commander-cli /opt/commander-cli/commander \
    && rm -r SimplicityCommander-Linux \
    && rm SimplicityCommander-Linux.zip

# Install Silicon Labs Configurator (slc)
RUN \
    curl -L -O --compressed -H 'User-Agent: Firefox/143' -H 'Accept-Language: *' https://www.silabs.com/documents/public/software/slc_cli_linux.zip \
    && unzip -q -d /opt slc_cli_linux.zip \
    && rm slc_cli_linux.zip

# Now install remaining packages
RUN \
    apt-get install -y --no-install-recommends \
       git \
       git-lfs \
       jq \
       yq \
       libgl1 \
       libglib2.0-0 \
       libpcre2-16-0 \
       make \
       default-jre-headless \
       patch \
       python3 \
       python3-pip \
       python3-virtualenv \
    && rm -rf /var/lib/apt/lists/*

# Patch ZAP apack.json to add missing linux.aarch64 executable definitions
RUN jq '.executable["zap:linux.aarch64"] = {"exe": "zap", "optional": true} | .executable["zap-cli:linux.aarch64"] = {"exe": "zap-cli", "optional": true}' \
    /opt/zap/apack.json > /tmp/apack.json && mv /tmp/apack.json /opt/zap/apack.json

# slc-cli hardcodes architectures internally and does not properly support ARM64 despite
# actually being fully compatible with it. It requires Python via JEP just for Jinja2
# template generation so we can install a standalone Python 3.10 and use that for JEP.
# For consistency across architectures, we compile and symlink on x86-64 even though it
# is not necessary.
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        PYTHON_ARCH="aarch64"; \
    else \
        PYTHON_ARCH="x86_64"; \
    fi \
    && curl -L -o /tmp/python3.10.tar.gz "https://github.com/astral-sh/python-build-standalone/releases/download/20251217/cpython-3.10.19%2B20251217-${PYTHON_ARCH}-unknown-linux-gnu-install_only.tar.gz" \
    && mkdir -p /opt/slc_python \
    && tar -xzf /tmp/python3.10.tar.gz -C /opt/slc_python --strip-components=1 \
    && rm /tmp/python3.10.tar.gz \
    && apt-get update \
    && apt-get install -y --no-install-recommends clang default-jdk-headless \
    # JEP also does not build with setuptools>=69
    && /opt/slc_python/bin/python3.10 -m pip install --no-cache-dir setuptools==68.2.2 wheel==0.45.1 \
    && JAVA_HOME=/usr/lib/jvm/default-java LIBRARY_PATH=/opt/slc_python/lib /opt/slc_python/bin/python3.10 -m pip install --no-cache-dir --no-build-isolation jep==4.1.1 jinja2==3.1.6 pyyaml==6.0.3 \
    && mkdir -p /opt/slc_python/jep \
    # Create symlinks for JEP shared library and Python shared library
    && ln -sf /opt/slc_python/lib/python3.10/site-packages/jep/jep.cpython-310-${PYTHON_ARCH}-linux-gnu.so /opt/slc_python/jep/jep.so \
    && ln -sf /opt/slc_python/lib/libpython3.10.so.1.0 /opt/slc_python/bin/libpython3.10.so.1.0 \
    && apt-get purge -y clang default-jdk-headless \
    && apt-get autoremove -y \
    && rm -rf /var/lib/apt/lists/* \
    # Remove bundled Python from slc-cli (we provide our own via STUDIO_PYTHON3_PATH)
    && rm -rf /opt/slc_cli/bin/slc-cli/developer/adapter_packs/python

# Set up Python virtual environment for firmware builder script
COPY requirements.txt /tmp/

RUN \
    virtualenv /opt/venv \
    && /opt/venv/bin/pip install -r /tmp/requirements.txt \
    && rm /tmp/requirements.txt

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV PATH="$PATH:/opt/commander-cli:/opt/slc_cli"
ENV STUDIO_ADAPTER_PACK_PATH="/opt/zap"
ENV STUDIO_PYTHON3_PATH="/opt/slc_python"

# We can run slc-cli without the native wrapper. For consistency across architectures,
# we create the same wrapper script on both.
RUN printf '#!/bin/sh\nexec java -Dorg.slf4j.simpleLogger.defaultLogLevel=off -jar /opt/slc_cli/bin/slc-cli/plugins/org.eclipse.equinox.launcher_*.jar -consoleLog "$@"\n' > /usr/local/bin/slc \
    && chmod +x /usr/local/bin/slc

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

USER $USERNAME
WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
