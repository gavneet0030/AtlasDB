#!/usr/bin/env bash
set -euo pipefail

echo "Installing AtlasDB Linux build dependencies..."

sudo apt-get update
sudo apt-get install -y \
    build-essential \
    g++ \
    clang \
    clang-format \
    clang-tidy \
    cmake \
    ninja-build \
    git \
    gdb \
    lldb \
    python3 \
    python3-pip \
    python3-venv \
    python3-dev \
    pkg-config \
    ccache \
    curl \
    wget \
    unzip \
    zip \
    tar \
    valgrind \
    linux-tools-common \
    linux-tools-generic

echo ""
echo "Tool versions:"
cmake --version
ninja --version
g++ --version
clang++ --version
git --version
python3 --version

echo ""
echo "Linux dependencies installed."
echo "Run the CMake configure/build/test commands from the repository root."