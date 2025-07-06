#!/bin/bash
# Fedora installation script for project dependencies

set -e  # Exit immediately if a command exits with a non-zero status

echo "Installation starts for Fedora."

# Update the package list
sudo dnf update -y && sudo dnf upgrade -y || true
# Install necessary packages
sudo dnf install -y \
    @development-tools \
      clang \
      perl-IPC-Cmd \
      kernel-headers \
      curl \
      openssl-devel \
      bison \
      flex \
      make \
      cmake \
      autoconf \
      automake \
      git \
      perl-CPAN

sudo cpan IPC::Cmd

echo "LLVM and Clang installation completed. Current version" && clang --version
echo "Base packages installation completed."
script_path="$(pwd)/setup_vcpkg.sh"
chmod +x "$script_path"  # ensure it's executable
sudo "$script_path"
echo "Installation complete for Fedora."
