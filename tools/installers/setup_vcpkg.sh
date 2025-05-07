#!/bin/bash

VCPKG_ROOT="/opt/vcpkg"

if [ ! -d "$VCPKG_ROOT" ]; then
    echo "Cloning vcpkg repository..."
    sudo git clone https://github.com/Microsoft/vcpkg.git "$VCPKG_ROOT"
    sudo chown -R "$USER":"$USER" "$VCPKG_ROOT"
fi

cd "$VCPKG_ROOT" || exit

echo "Updating vcpkg..."
git pull origin master

echo "Bootstrapping vcpkg..."
./bootstrap-vcpkg.sh

# Install dependencies from Vcpkg
echo "Installing vcpkg packages..."
./vcpkg install \
    boost-system \
    boost-filesystem \
    boost-log \
    boost-program-options \
    grpc \
    jsoncpp \
    valijson \
    libpq \
    libuuid \
    gtest \
    libpqxx \
    drogon \
    abseil \
    libevent
