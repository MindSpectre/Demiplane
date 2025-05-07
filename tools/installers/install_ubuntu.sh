#!/bin/bash
# Ubuntu installation script for project dependencies

set -e  # Exit immediately if a command exits with a non-zero status

echo "Installation starts for Ubuntu."

# Update the package list, but don't exit if it fails
sudo apt-get update || true

# Install necessary packages
sudo apt-get install -y \
    build-essential \
    pkg-config \
    curl \
    libicu-dev \
    uuid-dev \
    mesa-common-dev \
    libglu1-mesa-dev \
    libxkbcommon-x11-0 \
    libssl-dev \
    openssl \
    zlib1g-dev \
    zip \
    unzip \
    ninja-build

echo "Base packages installation completed."

# Install LLVM and Clang
echo "Installing LLVM and Clang..."
wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    sudo ./llvm.sh 19 && \
    sudo apt-get install -y \
        clang-19 \
        clang++-19 && \
    sudo update-alternatives --install /usr/bin/clang clang /usr/bin/clang-19 101 && \
    sudo update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-19 101 && \
    sudo update-alternatives --set clang /usr/bin/clang-19 && \
    sudo update-alternatives --set clang++ /usr/bin/clang++-19 && \
    rm -f llvm.sh

echo "LLVM and Clang installation completed. Current version" && clang --version
script_path="$(pwd)/setup_vcpkg.sh"
chmod +x "$script_path"  # ensure it's executable
sudo "$script_path"


echo "Installation complete for Ubuntu."
