FROM debian:bookworm

ARG DEBIAN_FRONTEND=noninteractive

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
       make \
       default-jre-headless \
       patch \
       python3 \
       python3-pip \
       python3-virtualenv \
       unzip \
       xz-utils

COPY requirements.txt /tmp/
COPY tools/build_firmware.sh /usr/bin/build_firmware.sh

RUN \
    virtualenv /opt/venv \
    && /opt/venv/bin/pip install -r /tmp/requirements.txt \
    && rm /tmp/requirements.txt

# Install Simplicity Commander (unfortunately no stable URL available, this
# is known to be working with Commander_linux_x86_64_1v15p0b1306.tar.bz).
ARG SIMPLICITY_COMMANDER_SHA256SUM=ce7b9138c54f4fa0a24c48c8347e55e3e5f8b402d7f32615771bd0403c2d8962
RUN \
    curl -O https://www.silabs.com/documents/login/software/SimplicityCommander-Linux.zip \
    && echo "${SIMPLICITY_COMMANDER_SHA256SUM}  SimplicityCommander-Linux.zip" | sha256sum -c \
    && unzip -q SimplicityCommander-Linux.zip \
    && tar -C /opt -xjf SimplicityCommander-Linux/Commander_linux_x86_64_*.tar.bz \
    && rm -r SimplicityCommander-Linux \
    && rm SimplicityCommander-Linux.zip

ENV PATH="$PATH:/opt/commander"

# Install Silicon Labs Configurator (slc)
ARG SLC_CLI_SHA256SUM=da4faa09ef4cbe385da71e5b95a4e444666cf4aaca6066b1095ca13bf5ebf233
RUN \
    curl -O https://www.silabs.com/documents/login/software/slc_cli_linux.zip \
    && echo "${SLC_CLI_SHA256SUM}  slc_cli_linux.zip" | sha256sum -c \
    && unzip -q -d /opt slc_cli_linux.zip \
    && rm slc_cli_linux.zip

ENV PATH="$PATH:/opt/slc_cli"

# GCC Embedded Toolchain 12.2.rel1 (for Gecko SDK 4.4.0+)
ARG GCC_TOOLCHAIN_SHA256SUM=84be93d0f9e96a15addd490b6e237f588c641c8afdf90e7610a628007fc96867
RUN \
    curl -O https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/12.2.rel1/binrel/arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz \
    && echo "${GCC_TOOLCHAIN_SHA256SUM}  arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz" | sha256sum -c \
    && tar -C /opt -xf arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz \
    && rm arm-gnu-toolchain-12.2.rel1-x86_64-arm-none-eabi.tar.xz

# Simplicity SDK 2024.6.2
ARG SIMPLICITY_SDK_SHA256SUM=7e4337c7cc68262dd3a83c8528095774634a0478d40b1c1fd2b462e86236af8a
RUN \
    curl -o simplicity_sdk_2024.6.2.zip -L https://github.com/SiliconLabs/simplicity_sdk/releases/download/v2024.6.2/gecko-sdk.zip \
    && echo "${SIMPLICITY_SDK_SHA256SUM}  simplicity_sdk_2024.6.2.zip" | sha256sum -c \
    && unzip -q -d simplicity_sdk_2024.6.2 simplicity_sdk_2024.6.2.zip \
    && rm simplicity_sdk_2024.6.2.zip

# Gecko SDK 4.4.4
ARG GECKO_SDK_SHA256SUM=831ec7c564df4392b18a8cc8ceb228c114dc3bec604be75807961a4289ee9b20
RUN \
    curl -o gecko_sdk_4.4.4.zip -L https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.4.4/gecko-sdk.zip \
    && echo "${GECKO_SDK_SHA256SUM}  gecko_sdk_4.4.4.zip" | sha256sum -c \
    && unzip -q -d gecko_sdk_4.4.4 gecko_sdk_4.4.4.zip \
    && rm gecko_sdk_4.4.4.zip

# ZCL Advanced Platform (ZAP) v2024.09.27
ARG ZAP_SHA256SUM=22beeae3cf33b04792be379261d68695b5c96986d3b80700c22b1348f4c0421e
RUN \
    curl -o zap_2024.09.27.zip -L https://github.com/project-chip/zap/releases/download/v2024.09.27/zap-linux-x64.zip \
    && echo "${ZAP_SHA256SUM}  zap_2024.09.27.zip" | sha256sum -c \
    && unzip -q -d /opt/zap zap_2024.09.27.zip \
    && rm zap_2024.09.27.zip

ENV STUDIO_ADAPTER_PACK_PATH="/opt/zap"

ARG USERNAME=builder
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

RUN mkdir -p /build_dir /outputs
RUN chown $USERNAME:$USERNAME \
    /gecko_sdk_* \
    /simplicity_sdk_* \
    /build_dir \
    /outputs

USER $USERNAME
WORKDIR /build
