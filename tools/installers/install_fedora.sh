#!/bin/bash
# Fedora installation script for project dependencies

set -e  # Exit immediately if a command exits with a non-zero status

echo "Installation starts for Fedora."

# Update the package list
sudo dnf update -y && sudo dnf upgrade -y
# Install necessary packages
sudo dnf install -y \
    @development-tools \
    pkgconf-pkg-config \
    curl \
    haskell-platform \
    libicu-devel \
    libuuid-devel \
    mesa-libGL-devel \
    mesa-libGLU-devel \
    xkeyboard-config \
    openssl-devel \
    zlib-devel \
    zip \
    unzip \
    git \
    perl-IPC-Cmd \
    autoconf \
    automake \
    libtool \
    kernel-headers



sudo dnf install -y perl-CPAN
sudo cpan IPC::Cmd

echo "LLVM and Clang installation completed. Current version" && clang --version
echo "Base packages installation completed."
script_path="$(pwd)/setup_vcpkg.sh"
chmod +x "$script_path"  # ensure it's executable
sudo "$script_path"
echo "Installation complete for Fedora."
