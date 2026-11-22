#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

if [ "$#" -ne 2 ]; then
    printf '%s\n' "Usage: $0 WRITEPAD_SOURCE_TARBALL OUTPUT_DIRECTORY" >&2
    exit 2
fi

tool_dir=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
archive=$1
output=$2
revision=d6cd5b032d679e739e427a6d03d03a6219b0dcaf
expected=f95a59c5900aa3c60f02a94b8096a279d66f109da6d114c15ec1cf78214e2ae2
actual=$(sha256sum < "$archive")
if [ "${actual%% *}" != "$expected" ]; then
    printf '%s\n' "The WritePad source archive does not match commit $revision." >&2
    exit 1
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
source_prefix="WritePad-Handwriting-Recognition-Engine-$revision"
tar -xzf "$archive" -C "$work" --strip-components=1 \
    "$source_prefix/Linux/makefile" "$source_prefix/UniversalRecognizer/V300/include" \
    "$source_prefix/UniversalRecognizer/V300/InkData" "$source_prefix/UniversalRecognizer/V300/RecognizerWrapper" \
    "$source_prefix/UniversalRecognizer/V300/reco" "$source_prefix/Dictionaries/Italian.dct" \
    "$source_prefix/Dictionaries/English.dct" "$source_prefix/LICENSE" "$source_prefix/COPYRIGHT"

# The reMarkable SDK supplies these environment variables. CXX and STRIP may contain flags.
: "${CXX:=c++}" "${STRIP:=strip}"
mkdir -p "$work/Linux/Lib" "$work/Linux/Release"
make -C "$work/Linux" -j "${QUADERNO_BUILD_JOBS:-2}" CPP="$CXX" \
    CFLAGS='-std=gnu++11 -O2 -DNDEBUG -ffunction-sections -fdata-sections' \
    DEFINES='-D_OS_LINUX -DHWR_SYSTEM=HWR_LINUX -D_EMBEDDED_DEVICE -DRECODICT -DLANG_ITALIAN -DLANG_ENGLISH'

# shellcheck disable=SC2086
$CXX -std=c++17 -O2 -Wall -Wextra -Wpedantic -D_OS_LINUX -ffunction-sections -fdata-sections \
    -I "$work/UniversalRecognizer/V300/include" "$tool_dir/handwriting.cpp" \
    "$work/Linux/Lib/libWritePadLib.a" -Wl,--gc-sections -ljson-c -o "$work/quaderno-handwriting"
# shellcheck disable=SC2086
$STRIP --strip-unneeded "$work/quaderno-handwriting"

mkdir -p "$output"
install -m 0755 "$work/quaderno-handwriting" "$output/quaderno-handwriting"
install -m 0644 "$work/Dictionaries/Italian.dct" "$output/quaderno-handwriting-it.dct"
install -m 0644 "$work/Dictionaries/English.dct" "$output/quaderno-handwriting-en.dct"
install -m 0644 "$work/LICENSE" "$output/WritePad-LICENSE.txt"
install -m 0644 "$work/COPYRIGHT" "$output/WritePad-COPYRIGHT.txt"
