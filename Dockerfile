FROM debian:bullseye

ARG DEBIAN_FRONTEND=noninteractive

RUN \
    apt-get update \
    && apt-get install -y --no-install-recommends \
       bzip2 \
       curl \
       git \
       jq \
       libgl1 \
       make \
       openjdk-11-jre-headless \
       python3 \
       unzip

# Install Simplicity Commander
RUN \
    curl -O https://www.silabs.com/documents/login/software/SimplicityCommander-Linux.zip \
    && unzip SimplicityCommander-Linux.zip \
    && tar -C /opt -xjf SimplicityCommander-Linux/Commander_linux_x86_64_*.tar.bz \
    && rm -r SimplicityCommander-Linux \
    && rm SimplicityCommander-Linux.zip

ENV PATH="$PATH:/opt/commander"

# Install Silicon Labs Configurator (slc)
RUN \
    curl -O https://www.silabs.com/documents/login/software/slc_cli_linux.zip \
    && unzip -d /opt slc_cli_linux.zip \
    && rm slc_cli_linux.zip

ENV PATH="$PATH:/opt/slc_cli"

ARG GCC_ARM_VERSION="10.3-2021.10"

# Install ARM GCC embedded toolchain
RUN \
    curl -O https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu-rm/${GCC_ARM_VERSION}/gcc-arm-none-eabi-${GCC_ARM_VERSION}-x86_64-linux.tar.bz2 \
    && tar -C /opt -xjf gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2 \
    && rm gcc-arm-none-eabi-${GCC_ARM_VERSION}-x86_64-linux.tar.bz2

ENV PATH="$PATH:/opt/gcc-arm-none-eabi-${GCC_ARM_VERSION}/bin"

ARG GECKO_SDK_VERSION="v4.1.4"

RUN \
    git clone --depth 1 -b ${GECKO_SDK_VERSION} \
       https://github.com/SiliconLabs/gecko_sdk.git

RUN \
    slc configuration \
           --sdk="/gecko_sdk/" \
    && slc signature trust --sdk "/gecko_sdk/" \
    && slc configuration \
           --gcc-toolchain="/opt/gcc-arm-none-eabi-${GCC_ARM_VERSION}/"

WORKDIR /build
