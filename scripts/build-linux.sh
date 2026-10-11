#!/usr/bin/env bash
# Development build on Linux (Ubuntu 24.04): builds the VST3, runs every test under a virtual display.
# Deps: cmake ninja g++ pkg-config libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev
#       libxinerama-dev libxcursor-dev libxext-dev libgl1-mesa-dev xvfb python3
set -euo pipefail
cd "$(dirname "$0")/.."
JUCE_COMMIT=91ad83ae34a81e0833b1a2b0866f54846370ae53
if [ ! -f third_party/JUCE/CMakeLists.txt ]; then
  git clone --branch 8.0.15 --depth 1 https://github.com/juce-framework/JUCE.git third_party/JUCE
fi
[ "$(git -C third_party/JUCE rev-parse HEAD)" = "$JUCE_COMMIT" ] || { echo "Unexpected JUCE commit"; exit 1; }
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DPRYSM_ENABLE_LTO=OFF
cmake --build build-linux
xvfb-run -a ctest --test-dir build-linux --output-on-failure
