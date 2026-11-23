#!/usr/bin/env bash
# Configure, build, and test beDBug.
# Usage: ./build.sh [Debug|Release]
set -euo pipefail

cd "$(dirname "$0")"

BUILD_TYPE="${1:-Debug}"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build build --parallel
ctest --test-dir build --output-on-failure

echo "beDBug ($BUILD_TYPE) built: ./build/src/app/bedbug"
