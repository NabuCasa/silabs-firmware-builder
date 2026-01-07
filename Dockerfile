FROM debian:trixie

ARG DEBIAN_FRONTEND=noninteractive
ARG TARGETARCH

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
    && rm -r SimplicityCommander-Linux \
    && rm SimplicityCommander-Linux.zip

# Install Silicon Labs Configurator (slc)
RUN \
    curl -L -O --compressed -H 'User-Agent: Firefox/143' -H 'Accept-Language: *' https://www.silabs.com/documents/public/software/slc_cli_linux.zip \
    && unzip -q -d /opt slc_cli_linux.zip \
    && rm slc_cli_linux.zip

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

# ZCL Advanced Platform (ZAP) v2024.09.27 (architecture-specific)
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        ZAP_ARCH="arm64"; \
    else \
        ZAP_ARCH="x64"; \
    fi \
    && curl -o zap.zip -L "https://github.com/project-chip/zap/releases/download/v2024.09.27/zap-linux-${ZAP_ARCH}.zip" \
    && unzip -q -d /opt/zap zap.zip \
    && rm zap.zip

# Now install remaining packages
RUN \
    apt-get install -y --no-install-recommends \
       git \
       git-lfs \
       jq \
       yq \
       libgl1 \
       libglib2.0-0 \
       make \
       default-jre-headless \
       default-jdk-headless \
       patch \
       python3 \
       python3-pip \
       python3-virtualenv \
       python3-numpy \
       python3-scipy \
       python3-jinja2 \
       python3-yaml \
    && rm -rf /var/lib/apt/lists/*

# slc-cli requires a tiny bit of native code for Jinja2 template generation.
# Unfortunately, slc-cli does not support ARM64. We can fix this.
RUN \
    if [ "$TARGETARCH" = "arm64" ]; then \
        # Set up Python 3.10
        curl -L -o /tmp/python3.10.tar.gz "https://github.com/astral-sh/python-build-standalone/releases/download/20251217/cpython-3.10.19+20251217-aarch64-unknown-linux-gnu-install_only.tar.gz" \
        && mkdir -p /opt/slc_python \
        && tar -xzf /tmp/python3.10.tar.gz -C /opt/slc_python --strip-components=1 \
        && rm /tmp/python3.10.tar.gz \
        # Build JEP
        && apt-get update \
        && apt-get install -y --no-install-recommends clang \
        && /opt/slc_python/bin/python3.10 -m pip install --no-cache-dir "setuptools<69" wheel \
        && JAVA_HOME=/usr/lib/jvm/default-java LIBRARY_PATH=/opt/slc_python/lib /opt/slc_python/bin/python3.10 -m pip install --no-cache-dir --no-build-isolation jep==4.1.1 numpy scipy jinja2 pyyaml \
        && mkdir -p /opt/slc_python/jep \
        # Symlink our local JEP to replace the bundled x86_64 one
        && ln -sf /opt/slc_python/lib/python3.10/site-packages/jep/jep.cpython-310-aarch64-linux-gnu.so /opt/slc_python/jep/jep.so \
        && ln -sf /opt/slc_python/lib/libpython3.10.so.1.0 /opt/slc_python/bin/libpython3.10.so.1.0 \
        # Clean up apt
        && apt-get purge -y clang && apt-get autoremove -y && rm -rf /var/lib/apt/lists/*; \
    fi

COPY requirements.txt /tmp/

RUN \
    virtualenv /opt/venv \
    && /opt/venv/bin/pip install -r /tmp/requirements.txt \
    && rm /tmp/requirements.txt

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV PATH="$PATH:/opt/commander:/opt/slc_cli"
ENV STUDIO_ADAPTER_PACK_PATH="/opt/zap"

# slc-cli Python path for ARM64.
# It will fall back to the bundled Python if this does not exist.
ENV STUDIO_PYTHON3_PATH="/opt/slc_python"

# We can run slc-cli without the native wrapper
RUN printf '#!/bin/sh\nexec java -Dorg.slf4j.simpleLogger.defaultLogLevel=off -jar /opt/slc_cli/bin/slc-cli/plugins/org.eclipse.equinox.launcher_*.jar -consoleLog "$@"\n' > /usr/local/bin/slc \
    && chmod +x /usr/local/bin/slc

ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

USER $USERNAME
WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
