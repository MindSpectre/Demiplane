FROM fedora:latest AS toolchain

ENV VCPKG_ROOT=/opt/vcpkg
ENV DEBIAN_FRONTEND=noninteractive \
    VCPKG_DISABLE_METRICS=1 \
    VCPKG_FEATURE_FLAGS=manifests

# 1. Core build tooling
RUN dnf -y group install development-tools
RUN dnf -y install clang
# 2. vcpkg bootstrap (pinned commit for reproducibility)
RUN git clone https://github.com/microsoft/vcpkg ${VCPKG_ROOT} && \
    cd ${VCPKG_ROOT} && \
    ./bootstrap-vcpkg.sh

# 3. Copy manifest and restore libraries *before* app sources for cache reuse
WORKDIR /workspace
COPY . .
RUN ${VCPKG_ROOT}/vcpkg install --triplet x64-linux

# 4. Expose helpful environment variables for downstream build containers/VSCode
ENV PATH="$VCPKG_ROOT:$PATH" \
    VCPKG_DEFAULT_TRIPLET=x64-linux
