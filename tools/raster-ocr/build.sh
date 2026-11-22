#!/bin/sh
# SPDX-License-Identifier: MIT
set -eu

if [ "$#" -ne 7 ]; then
    printf '%s\n' "Usage: $0 ONNXRUNTIME_NUPKG MODEL_ONNX DICTIONARY_TXT PADDLE_LICENSE CONVERSION_NOTICE BUILD_DIR OUTPUT_DIR" >&2
    exit 2
fi

: "${OECORE_TARGET_SYSROOT:?Source the reMarkable SDK environment first}"
: "${OECORE_NATIVE_SYSROOT:?Source the reMarkable SDK environment first}"
: "${CMAKE_TOOLCHAIN_FILE:?Source the reMarkable SDK environment first}"
: "${STRIP:=strip}"

tool_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$6" "$7"
build=$(CDPATH='' cd -- "$6" && pwd)
output=$(CDPATH='' cd -- "$7" && pwd)
runtime="$build/runtime"
mkdir -p "$runtime/include" "$runtime/lib"

verify_file() {
    actual=$(sha256sum < "$1")
    if [ "${actual%% *}" != "$2" ]; then
        printf '%s\n' "The $3 file does not match the pinned SHA-256." >&2
        exit 1
    fi
}

verify_file "$1" a9150bc2992cbbb6722070da9910e1ffa989a8bfc67bdd5bb8e745ca755641db "ONNX Runtime"
verify_file "$2" 497dbed20b7fd86334c9deb5082c2958982a316bb16e3589ebc6502cd85cae79 "PP-OCRv5 model"
verify_file "$3" ccbcc45730b3fbbd9050c5bc74db6a99067141ef1035e3d14889a84a6b9b1aff "character dictionary"
verify_file "$4" 3840c5c0c61c294264d2dd77b8777be6ddd90121ef4e0e64abcd22edea581d6e "PaddleOCR license"
verify_file "$5" 48f304e5e27ab3baf3aacb9603226b4f264a8b0ed8725e6b69e4603f859ecae4 "PaddleOcrNet conversion notice"

unzip -jo "$1" 'build/native/include/*.h' -d "$runtime/include" >/dev/null
unzip -p "$1" runtimes/linux-arm64/native/libonnxruntime.so > "$runtime/lib/libonnxruntime.so"
unzip -p "$1" LICENSE > "$runtime/ONNXRuntime-LICENSE.txt"
unzip -p "$1" ThirdPartyNotices.txt > "$runtime/ONNXRuntime-ThirdPartyNotices.txt"
{
    cat "$4"
    printf '\n\nPaddleOcrNet ONNX model attribution\n'
    printf '%s\n\n' 'https://github.com/FarhanLodi/PaddleOcrNet/blob/da71ab03ac1552a77ee66fbea2fae9b6739c5829/NOTICE'
    sed '/^PDFium native libraries/,$d' "$5"
} > "$runtime/PaddleOCR-LICENSE.txt"

cmake -S "$tool_dir" -B "$build/helper" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE" \
    -DQt6_DIR="$OECORE_TARGET_SYSROOT/usr/lib/cmake/Qt6" \
    -DQT_HOST_PATH="$OECORE_NATIVE_SYSROOT/usr" \
    -DONNXRUNTIME_ROOT="$runtime"
cmake --build "$build/helper" --parallel "${QUADERNO_BUILD_JOBS:-2}"

# The official ARM library has the SONAME libonnxruntime.so.1. Stage it as a
# regular file because the Paper Pro package does not carry symlinks.
# shellcheck disable=SC2086
$STRIP --strip-unneeded -o "$build/quaderno-raster-ocr" "$build/helper/quaderno-raster-ocr"
install -m 0755 "$build/quaderno-raster-ocr" "$output/quaderno-raster-ocr"
install -m 0644 "$runtime/lib/libonnxruntime.so" "$output/libonnxruntime.so.1"
install -m 0644 "$2" "$output/quaderno-raster-ocr.onnx"
install -m 0644 "$3" "$output/quaderno-raster-ocr.txt"
install -m 0644 "$runtime/PaddleOCR-LICENSE.txt" "$output"
install -m 0644 "$runtime/ONNXRuntime-LICENSE.txt" "$runtime/ONNXRuntime-ThirdPartyNotices.txt" "$output"
