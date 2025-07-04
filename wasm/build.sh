#!/usr/bin/env bash
rm -rf build && mkdir build && cd build

emcmake cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../../vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DVCPKG_MANIFEST_MODE=ON \
    -DVCPKG_TARGET_TRIPLET=wasm32-emscripten \
    ..

cmake --build .
