#!/usr/bin/env bash
set -euo pipefail

info()  { printf '\033[1;34m[info]\033[0m  %s\n' "$*"; }

info "Installing system packages (macOS)..."

# ── Xcode Command Line Tools (provides Apple Clang) ─────────────────────────

if ! xcode-select -p &>/dev/null; then
    info "Installing Xcode Command Line Tools..."
    xcode-select --install
    info "Waiting for Xcode CLT installation to complete..."
    info "Re-run this script after the installation dialog finishes."
    exit 0
fi

# ── Homebrew ─────────────────────────────────────────────────────────────────

if ! command -v brew &>/dev/null; then
    info "Homebrew not found — installing..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

    if [ -f /opt/homebrew/bin/brew ]; then
        eval "$(/opt/homebrew/bin/brew shellenv)"
    elif [ -f /usr/local/bin/brew ]; then
        eval "$(/usr/local/bin/brew shellenv)"
    fi
fi

brew install \
    llvm \
    cmake \
    ninja \
    autoconf \
    autoconf-archive \
    automake \
    libtool \
    openssl \
    bison \
    flex \
    ccache \
    pkg-config \
    git \
    zip \
    unzip

info "Note: Homebrew llvm is keg-only. You may need to add it to your PATH:"
info "  export PATH=\"\$(brew --prefix llvm)/bin:\$PATH\""
