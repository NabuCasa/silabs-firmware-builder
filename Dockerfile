FROM debian:trixie

ARG DEBIAN_FRONTEND=noninteractive

# Simplicity SDK includes unicode characters in folder names and fails to unzip
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

RUN \
    apt-get update \
    && apt-get install -y --no-install-recommends \
       bzip2 \
       curl \
       git \
       git-lfs \
       jq \
       yq \
       libgl1 \
       libglib2.0-0 \
       make \
       default-jre-headless \
       patch \
       python3 \
       python3-pip \
       python3-virtualenv \
       unzip \
       xz-utils

COPY requirements.txt /tmp/

RUN \
    virtualenv /opt/venv \
    && /opt/venv/bin/pip install -r /tmp/requirements.txt \
    && rm /tmp/requirements.txt

# Install Simplicity Commander (unfortunately no stable URL available, this
# is known to be working with Commander_linux_x86_64_1v15p0b1306.tar.bz).
RUN \
    curl -L -O --compressed -H 'User-Agent: Firefox/143' -H 'Accept-Language: *' https://www.silabs.com/documents/public/software/SimplicityCommander-Linux.zip \
    && unzip -q SimplicityCommander-Linux.zip \
    && tar -C /opt -xjf SimplicityCommander-Linux/Commander_linux_x86_64_*.tar.bz \
    && rm -r SimplicityCommander-Linux \
    && rm SimplicityCommander-Linux.zip

ENV PATH="$PATH:/opt/commander"

# Install Silicon Labs Configurator (slc)
RUN \
    curl -L -O --compressed -H 'User-Agent: Firefox/143' -H 'Accept-Language: *' https://www.silabs.com/documents/public/software/slc_cli_linux.zip \
    && unzip -q -d /opt slc_cli_linux.zip \
    && rm slc_cli_linux.zip

ENV PATH="$PATH:/opt/slc_cli"

# GCC Embedded Toolchain 12.2.rel1 (for Gecko SDK 4.4.0+)
RUN \
    curl -O https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz \
    && tar -C /opt -xf arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz \
    && rm arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz

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

# ZCL Advanced Platform (ZAP) v2024.09.27
RUN \
    curl -o zap_2024.09.27.zip -L https://github.com/project-chip/zap/releases/download/v2024.09.27/zap-linux-x64.zip \
    && unzip -q -d /opt/zap zap_2024.09.27.zip \
    && rm zap_2024.09.27.zip

ENV STUDIO_ADAPTER_PACK_PATH="/opt/zap"

ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

USER $USERNAME
WORKDIR /build
