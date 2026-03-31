#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
VCPKG_DIR="$PROJECT_ROOT/vcpkg"

if [ -d "$VCPKG_DIR" ]; then
    echo "vcpkg already exists at $VCPKG_DIR"
    echo "To update, run: git -C $VCPKG_DIR pull && $VCPKG_DIR/bootstrap-vcpkg.sh"
else
    echo "Cloning vcpkg into $VCPKG_DIR..."
    git clone https://github.com/microsoft/vcpkg "$VCPKG_DIR"
    echo "Bootstrapping vcpkg..."
    "$VCPKG_DIR/bootstrap-vcpkg.sh"
fi

echo ""
echo "Setup complete. Build with:"
echo ""
echo "  cmake --preset=debug"
echo "  cmake --build build/debug"
