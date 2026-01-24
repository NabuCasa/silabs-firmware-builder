# Gecko SDK 4.5.0, this can be removed once we migrate away from Gecko SDK
FROM debian:trixie-slim AS gecko-sdk-v4.5.0
RUN apt-get update && apt-get install -y --no-install-recommends aria2 ca-certificates libarchive-tools \
    && rm -rf /var/lib/apt/lists/* \
    && aria2c --checksum=sha-256=b5b2b2410eac0c9e2a72320f46605ecac0d376910cafded5daca9c1f78e966c8 -o /tmp/sdk.zip \
        https://github.com/SiliconLabs/gecko_sdk/releases/download/v4.5.0/gecko-sdk.zip \
    && mkdir /out && bsdtar -xf /tmp/sdk.zip -C /out \
    && rm /tmp/sdk.zip

# Main slt bin
FROM debian:trixie-slim
ARG TARGETARCH

# Simplicity SDK includes unicode characters in folder names and fails to unzip
ENV LANG=C.UTF-8
ENV LC_ALL=C.UTF-8

# Set up slt and conan
RUN set -o pipefail \
    && apt-get update && apt-get install -y --no-install-recommends \
        aria2 \
        bzip2 \
        ca-certificates \
        curl \
        libarchive-tools \
        unzip \
    && rm -rf /var/lib/apt/lists/* \
    # Download slt-cli (x64 only, runs under QEMU on ARM64)
    && curl -sL -o /tmp/slt.zip "https://www.silabs.com/documents/public/software/slt-cli-1.1.0-linux-x64.zip" \
    && unzip -q /tmp/slt.zip -d /usr/bin && chmod +x /usr/bin/slt && rm /tmp/slt.zip \
    && if [ "$TARGETARCH" = "arm64" ]; then \
        # Install x86_64 libraries and QEMU for running slt under emulation
        dpkg --add-architecture amd64 \
        && apt-get update \
        && apt-get install -y --no-install-recommends libc6:amd64 zlib1g:amd64 \
        # Newer QEMU versions have Go runtime bugs, use 7.0.0 directly
        && curl -sL https://github.com/tonistiigi/binfmt/releases/download/deploy%2Fv7.0.0-28/qemu_v7.0.0_linux-arm64.tar.gz \
            | tar -xz -C /usr/bin qemu-x86_64 \
        && chmod +x /usr/bin/qemu-x86_64 \
        && rm -rf /var/lib/apt/lists/* \
        # Create wrapper script for transparent QEMU invocation of slt
        && mv /usr/bin/slt /usr/bin/slt-bin \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64 /usr/bin/slt-bin . "$@"\n' > /usr/bin/slt \
        && chmod +x /usr/bin/slt \
        # Install conan engine (x86_64) before patching slt for ARM64
        && slt install conan \
        # Wrap conan binaries to run under QEMU
        && CONAN_PATH="$(slt where conan)" \
        && mv "$CONAN_PATH/conan" "$CONAN_PATH/conan-dir" \
        && printf '#!/bin/sh\nexec /usr/bin/qemu-x86_64 %s/conan-dir/conan . "$@"\n' "$CONAN_PATH" > "$CONAN_PATH/conan" \
        && chmod +x "$CONAN_PATH/conan" \
        && mv "$CONAN_PATH/conan_engine" "$CONAN_PATH/conan_engine-bin" \
        && printf '#!/bin/sh\nexport PATH="%s:$PATH"\nexec /usr/bin/qemu-x86_64 %s/conan_engine-bin . "$@"\n' "$CONAN_PATH" "$CONAN_PATH" > "$CONAN_PATH/conan_engine" \
        && chmod +x "$CONAN_PATH/conan_engine" \
        # Now patch slt to select ARM64 packages
        && sed -i 's/amd6/arm6/g' /usr/bin/slt-bin \
        # Force conan to use ARM64 profile (detect_arch returns x86_64 under QEMU)
        && cp /root/.silabs/slt/installs/conan/profiles/linux_arm64 /root/.silabs/slt/installs/conan/profiles/default; \
    else \
        slt install conan; \
    fi

# Install toolchain via slt
RUN set -o pipefail \
    && slt install \
        simplicity-sdk/2025.6.2 \
        commander/1.22.0 \
        slc-cli/6.0.15 \
        zap/2025.12.02 \
    # Clean up download caches to reduce image size
    && rm -rf /root/.silabs/slt/installs/archive/*.zip \
              /root/.silabs/slt/installs/archive/*.tar.* \
              /root/.silabs/slt/installs/conan/p/*/d/ \
    # Create stable symlinks/wrappers for PATH using slt where
    && mkdir -p /root/.silabs/slt/bin \
    && ln -s "$(slt where java21)/jre/bin/java" /root/.silabs/slt/bin/java \
    && ln -s "$(slt where commander)/commander" /root/.silabs/slt/bin/commander \
    # slc needs a wrapper script because it uses $(dirname "$0") to find slc.jar
    && printf '#!/bin/sh\nexec "%s/slc" "$@"\n' "$(slt where slc-cli)" > /root/.silabs/slt/bin/slc \
    && chmod +x /root/.silabs/slt/bin/slc

# Python virtual environment for the firmware builder script
COPY requirements.txt /tmp/
RUN set -o pipefail \
    && apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates curl \
    && curl -LsSf https://astral.sh/uv/install.sh | env UV_INSTALL_DIR="/usr/bin" sh \
    && UV_PYTHON_INSTALL_DIR=/opt/pythons uv venv -p 3.13 /opt/venv --no-cache \
    && uv pip install --python /opt/venv -r /tmp/requirements.txt

# Install runtime packages
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       cmake \
       ninja-build \
       git \
       libstdc++6 \
       libgl1 \
       libpng16-16t64 \
       libpcre2-16-0 \
       libglib2.0-0 \
    && rm -rf /var/lib/apt/lists/*

# Copy all downloaded artifacts from parallel stages
COPY --from=gecko-sdk-v4.5.0 /out /gecko_sdk_4.5.0

# Signal to the firmware builder script that we are running within Docker
ENV SILABS_FIRMWARE_BUILD_CONTAINER=1
ENV HOME=/root
ENV PATH="$PATH:/root/.silabs/slt/bin"

WORKDIR /repo

ENTRYPOINT ["/opt/venv/bin/python3", "tools/build_project.py"]
