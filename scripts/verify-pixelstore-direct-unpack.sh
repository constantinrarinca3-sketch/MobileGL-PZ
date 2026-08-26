#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_022="$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)"
SOURCE_021="${1:-$(CDPATH= cd -- "$SOURCE_022/../../mobilegl_021_renderer_pack/source" && pwd)}"
BUILD_DIR="${2:-$SOURCE_022/../build-pixelstore-selftest}"
TEST_SOURCE="$SCRIPT_DIR/tests/pixelstore-direct-unpack-selftest.cpp"

test -f "$SOURCE_021/MobileGL/MG_Util/Texture/PixelStoreProcessor.cpp"
test -f "$SOURCE_022/MobileGL/MG_Util/Texture/PixelStoreProcessor.cpp"
mkdir -p "$BUILD_DIR"

compile_test() {
    local source_root="$1"
    local direct="$2"
    local output="$3"
    g++ -std=gnu++23 -O1 -g -fno-omit-frame-pointer \
        -fsanitize=address,undefined \
        -DMOBILEGL_LOG_ACTIVE_LEVEL=MOBILEGL_LOG_LEVEL_FATAL \
        -DSELFTEST_DIRECT="$direct" \
        -I"$SCRIPT_DIR/tests/stubs" \
        -I"$source_root/include" \
        -I"$source_root/MobileGL" \
        -I"$source_root/3rdparty/glslang" \
        -I"$source_root/3rdparty/glslang/External" \
        -I"$source_root/3rdparty/glslang/External/spirv-tools/include" \
        -I"$source_root/3rdparty/glslang/External/spirv-tools/external/spirv-headers/include" \
        -I"$source_root/3rdparty/SPIRV-Cross" \
        -I"$source_root/3rdparty/xxHash" \
        -I"$source_root/3rdparty/Vulkan-Headers/include" \
        "$TEST_SOURCE" \
        "$source_root/MobileGL/MG_Util/Texture/PixelStoreProcessor.cpp" \
        "$source_root/MobileGL/MG_Util/Metrics/TextureMetrics.cpp" \
        -o "$output"
}

compile_test "$SOURCE_021" 0 "$BUILD_DIR/pixelstore-021-oracle"
compile_test "$SOURCE_022" 1 "$BUILD_DIR/pixelstore-022-direct"

ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$BUILD_DIR/pixelstore-021-oracle" > "$BUILD_DIR/021.out"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "$BUILD_DIR/pixelstore-022-direct" > "$BUILD_DIR/022.out"

diff -u "$BUILD_DIR/021.out" "$BUILD_DIR/022.out"
printf 'PIXELSTORE_021_022_EQUIVALENCE=PASS\n'
printf 'PIXELSTORE_ASAN_UBSAN=PASS\n'
printf 'PIXELSTORE_CASES=8\n'
