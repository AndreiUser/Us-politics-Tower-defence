#!/usr/bin/env bash
# Installs the build tools (Debian, Ubuntu, Raspberry Pi OS), builds the game and starts it.
set -e
cd "$(dirname "$0")"
if command -v apt-get >/dev/null; then
  sudo apt-get update || echo "apt-get update had errors; trying to install anyway"
  sudo apt-get install -y build-essential cmake git \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev
fi
cmake -B build -DOPENGL_VERSION="2.1"
cmake --build build -j"$(nproc)"
./build/capitol_defense
