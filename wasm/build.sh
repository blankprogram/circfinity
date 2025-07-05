#!/usr/bin/env bash
set -euxo pipefail

rm -rf build && mkdir build && cd build

# Correctly combine vcpkg and emscripten toolchains
emcmake cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake \
    -DVCPKG_TARGET_TRIPLET=wasm32-emscripten \
    ..

cmake --build . --target wasm_main
