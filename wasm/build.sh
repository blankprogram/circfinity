#!/usr/bin/env bash
set -euxo pipefail

rm -rf build && mkdir build && cd build

emcmake cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DVCPKG_MANIFEST_MODE=ON \
    -DVCPKG_TARGET_TRIPLET=wasm32-emscripten \
    ..

cmake --build .
