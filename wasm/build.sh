#!/usr/bin/env bash
set -euxo pipefail

rm -rf build && mkdir build && cd build

EXTRA_FLAGS=""
if [[ -z "${CI:-}" ]]; then
    EXTRA_FLAGS="-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$HOME/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
fi

emcmake cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_MANIFEST_MODE=ON \
    -DVCPKG_TARGET_TRIPLET=wasm32-emscripten \
    $EXTRA_FLAGS \
    ..

cmake --build .
