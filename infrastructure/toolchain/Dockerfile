FROM fedora:latest AS base

ENV VCPKG_ROOT=/opt/vcpkg
ENV DEBIAN_FRONTEND=noninteractive \
    VCPKG_DISABLE_METRICS=1 \
    VCPKG_FEATURE_FLAGS=manifests

# 1. Core build tooling
RUN dnf -y group install development-tools
RUN dnf -y install  \
    clang  \
    perl-IPC-Cmd  \
    kernel-headers \
    curl  \
    openssl-devel \
    bison  \
    flex  \
    make  \
    cmake \
    autoconf \
    automake  \
    ninja

RUN dnf install -y perl-CPAN
RUN cpan "IPC::Cmd"

# 2. vcpkg bootstrap
RUN git clone https://github.com/microsoft/vcpkg ${VCPKG_ROOT} && \
    cd ${VCPKG_ROOT} && \
    ./bootstrap-vcpkg.sh

# Intermediate stage for building dependencies
FROM base AS dependencies

# Copy manifest files to a temporary location
COPY vcpkg*.json /tmp/


# Install dependencies using the manifest
WORKDIR /tmp
RUN ${VCPKG_ROOT}/vcpkg install --triplet x64-linux

# Final toolchain stage
FROM base AS toolchain

# Copy the installed packages from the dependencies stage
COPY --from=dependencies /tmp/vcpkg_installed ${VCPKG_ROOT}/installed

# Set up workspace
WORKDIR /workspace
COPY . .
# Expose environment variables
ENV PATH="$VCPKG_ROOT:$PATH" \
    VCPKG_DEFAULT_TRIPLET=x64-linux