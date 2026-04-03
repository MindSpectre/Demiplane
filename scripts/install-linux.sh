#!/usr/bin/env bash
set -euo pipefail

info()  { printf '\033[1;34m[info]\033[0m  %s\n' "$*"; }
error() { printf '\033[1;31m[error]\033[0m %s\n' "$*" >&2; exit 1; }

# ── dnf (Fedora/RHEL) ───────────────────────────────────────────────────────

install_dnf() {
    info "Installing system packages via dnf..."

    sudo dnf -y group install development-tools

    sudo dnf -y install \
        clang \
        clang-tools-extra \
        perl-IPC-Cmd \
        kernel-headers \
        curl \
        openssl-devel \
        bison \
        flex \
        make \
        cmake \
        autoconf \
        autoconf-archive \
        automake \
        libtool \
        ninja-build \
        ccache \
        mold

    sudo dnf -y install perl-CPAN
    cpan "IPC::Cmd"
}

# ── apt (Debian/Ubuntu) ─────────────────────────────────────────────────────

install_apt() {
    info "Installing system packages via apt..."

    sudo apt-get update

    sudo apt-get install -y \
        build-essential \
        clang \
        clang-tools \
        libperl-dev \
        linux-headers-generic \
        curl \
        libssl-dev \
        bison \
        flex \
        make \
        cmake \
        autoconf \
        autoconf-archive \
        automake \
        libtool \
        ninja-build \
        ccache \
        mold \
        pkg-config \
        git \
        zip \
        unzip \
        tar

    sudo apt-get install -y libipc-run-perl
}

# ── pacman (Arch/Manjaro) ───────────────────────────────────────────────────

install_pacman() {
    info "Installing system packages via pacman..."

    sudo pacman -Syu --noconfirm

    sudo pacman -S --needed --noconfirm \
        base-devel \
        clang \
        linux-headers \
        curl \
        openssl \
        bison \
        flex \
        make \
        cmake \
        autoconf \
        autoconf-archive \
        automake \
        libtool \
        ninja \
        ccache \
        mold \
        pkgconf \
        git \
        zip \
        unzip \
        perl
}

# ── Dispatch ─────────────────────────────────────────────────────────────────

if command -v dnf &>/dev/null; then
    install_dnf
elif command -v apt-get &>/dev/null; then
    install_apt
elif command -v pacman &>/dev/null; then
    install_pacman
else
    error "Unsupported package manager. Supported: dnf (Fedora/RHEL), apt (Debian/Ubuntu), pacman (Arch)."
fi
