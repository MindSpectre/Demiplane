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
$VCPKG_ROOT/bootstrap-vcpkg.sh

export PATH="$VCPKG_ROOT:$PATH"
export VCPKG_DISABLE_METRICS=1
export VCPKG_DEFAULT_TRIPLET=x64-linux
# Install dependencies from Vcpkg
echo "Installing vcpkg packages..."

if [[ -f vcpkg.json ]]; then
  export VCPKG_FEATURE_FLAGS=manifests
  $VCPKG_ROOT/vcpkg install --triplet x64-linux
else
  echo "vcpkg.json not found. Install without manifest. Possibly requires
      additional installs to synchronise with manifest"
  $VCPKG_ROOT/vcpkg install \
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
      abseil \
      libevent
fi

