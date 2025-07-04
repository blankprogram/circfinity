#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$HERE/build-test"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_TOOLCHAIN_FILE="$HERE/../vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_MANIFEST_MODE=ON \
    -DVCPKG_TARGET_TRIPLET=x64-linux

cmake --build . --parallel

ctest --output-on-failure
