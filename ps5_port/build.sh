#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export PS5_ROOT="${PS5_ROOT:-$HOME/PS5}"
export PS5_PAYLOAD_SDK="${PS5_PAYLOAD_SDK:-$PS5_ROOT/ps5-native-app-boilerplate/.deps/native/ps5-payload-sdk}"
BUILD="${BUILD_DIR:-$HERE/../build/ps5}"
if [ ! -f "$BUILD/build.ninja" ]; then
    cmake -S "$HERE/../pc_port" -B "$BUILD" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$HERE/toolchain.cmake" \
        -DSH_STATIC_MAPS=ON -DSH_FMV_FFMPEG=OFF \
        -DOPENAL_INCLUDE_DIR="$HERE/stubs/include" \
        -DOPENAL_LIBRARY="$PS5_ROOT/stubs/libopenal.a" \
        -DSDL2_DIR="$PS5_ROOT/ps5-opengl/build/native-build/sdk/lib/cmake/SDL2"
fi
exec ninja -C "$BUILD" "$@"
