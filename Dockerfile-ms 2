FROM mcr.microsoft.com/devcontainers/python:3.10-bookworm

# ---------------------------------------------
# Base build tools
# ---------------------------------------------
RUN rm -f /etc/apt/sources.list.d/yarn.list && \
    apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build python3 curl xz-utils ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------
# Install ARM GNU Toolchain (host autodetect)
# Works with or without TARGETARCH/BuildKit
# ---------------------------------------------
ARG ARM_GCC_VERSION=13.3.rel1
ARG TARGETARCH  # may be unset under legacy builder

RUN set -e; \
    # Derive base arch: prefer TARGETARCH if provided, fallback to uname -m
    BASE_ARCH="${TARGETARCH:-}"; \
    if [ -z "$BASE_ARCH" ]; then BASE_ARCH="$(uname -m)"; fi; \
    case "$BASE_ARCH" in \
      amd64|x86_64)  HOSTARCH="x86_64" ;; \
      arm64|aarch64) HOSTARCH="aarch64" ;; \
      *)             HOSTARCH="x86_64" ;; \
    esac; \
    TARBALL="arm-gnu-toolchain-${ARM_GCC_VERSION}-${HOSTARCH}-arm-none-eabi.tar.xz"; \
    URL="https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/${ARM_GCC_VERSION}/binrel/${TARBALL}"; \
    echo "Downloading ${URL}"; \
    curl -fL -O "${URL}"; \
    tar -xf "${TARBALL}"; \
    rm "${TARBALL}"; \
    DIR="$(ls -d arm-gnu-toolchain-${ARM_GCC_VERSION}-*-arm-none-eabi)"; \
    mv "${DIR}" /opt/toolchain

# Toolchain in PATH
ENV PATH="/opt/toolchain/bin:${PATH}"

WORKDIR /src
