#!/bin/sh
set -eu

usage() {
    printf '%s\n' "Usage: $0 PAPER_PRO_EXECUTABLE OUTPUT_DIRECTORY [WRITEPAD_SOURCE_TARBALL]"
}

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

check_aarch64() {
    header=$(od -An -tx1 -N6 "$1" | tr -d '[:space:]')
    target=$(od -An -tx1 -j16 -N4 "$1" | tr -d '[:space:]')
    case "$header:$target" in
        7f454c460201:0200b700|7f454c460201:0300b700) ;;
        *) die "Expected an AArch64 ELF executable built with the matching Paper Pro SDK: $1" ;;
    esac
}

if [ "$#" -eq 1 ] && [ "$1" = --help ]; then
    usage
    exit 0
fi
if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    usage >&2
    exit 2
fi

executable=$1
writepad_source=${3:-}
repo=$(CDPATH='' cd -P -- "$(dirname -- "$0")/.." && pwd -P)
bundle=Quaderno-Paper-Pro-3.29.1.0
archive="$bundle.zip"
companion=Quaderno-WritePad-Source
source_archive="$companion.tar.gz"

for command in od zip unzip; do
    command -v "$command" >/dev/null || die "Missing command: $command"
done
if command -v sha256sum >/dev/null; then
    sha256() { sha256sum "$@"; }
elif command -v shasum >/dev/null; then
    sha256() { shasum -a 256 "$@"; }
else
    die 'Install sha256sum or shasum.'
fi

[ -f "$executable" ] || die "Executable not found: $executable"
if [ "$#" -eq 3 ]; then
    [ -f "$writepad_source" ] || die "WritePad source archive not found: $writepad_source"
fi
mkdir -p -- "$2"
output=$(CDPATH='' cd -P -- "$2" && pwd -P)
for file in "$archive" "$source_archive"; do
    if [ -e "$output/$file" ] || [ -L "$output/$file" ]; then
        die "Artifact already exists: $output/$file. Choose a fresh output directory."
    fi
done

stage=$(mktemp -d "$output/.quaderno-package.XXXXXX")
companion_linked=false
cleanup() {
    if [ "$companion_linked" = true ]; then
        rm -f "$output/$source_archive"
    fi
    rm -rf "$stage"
}
trap cleanup 0
trap 'exit 1' HUP INT TERM

package_dir="$stage/$bundle"
mkdir "$package_dir"
install -m 0755 -- "$executable" "$package_dir/quaderno"
check_aarch64 "$package_dir/quaderno"

install -m 0755 "$repo/scripts/install-paper-pro.sh" "$repo/scripts/try-on-device.sh" \
    "$repo/scripts/install-appload-quaderno.sh" "$repo/scripts/appload-quaderno-entry.sh" \
    "$repo/scripts/obsidian-sync-paper-pro.sh" "$repo/scripts/obsidian-sync-device.sh" \
    "$repo/scripts/cloud-auth-paper-pro.py" "$package_dir/"
install -m 0644 "$repo/scripts/quaderno-appload.png" "$package_dir/"
mkdir -p "$package_dir/docs/en"
install -m 0644 "$repo/docs/CLOUD_ACCOUNTS.md" "$repo/docs/DIAGRAMMI.md" "$repo/docs/IMPOSTAZIONI.md" \
    "$repo/docs/PROTOCOLLO_SYNC.md" "$repo/docs/DATI.md" "$repo/docs/NOTE_LIBRI.md" "$package_dir/docs/"
install -m 0644 "$repo/docs/en/CLOUD_ACCOUNTS.md" "$repo/docs/en/DIAGRAMS.md" "$repo/docs/en/SETTINGS.md" \
    "$repo/docs/en/SYNC_PROTOCOL.md" "$repo/docs/en/DATA.md" "$repo/docs/en/BOOK_NOTES.md" "$package_dir/docs/en/"
for file in LICENSE THIRD_PARTY_NOTICES.md; do
    if [ -f "$repo/$file" ]; then
        install -m 0644 "$repo/$file" "$package_dir/"
    fi
done

if [ -n "$writepad_source" ]; then
    source_dir="$stage/$companion"
    mkdir -p "$source_dir/tools/writepad"
    install -m 0644 "$repo/tools/writepad/handwriting.cpp" "$repo/tools/writepad/README.md" \
        "$repo/tools/writepad/build.sh" "$source_dir/tools/writepad/"
    install -m 0644 -- "$writepad_source" "$source_dir/writepad-source.tar.gz"
    sh "$source_dir/tools/writepad/build.sh" "$source_dir/writepad-source.tar.gz" "$package_dir"
    check_aarch64 "$package_dir/quaderno-handwriting"
    install -m 0644 "$package_dir/WritePad-LICENSE.txt" "$package_dir/WritePad-COPYRIGHT.txt" "$source_dir/"
    install -m 0644 "$package_dir/WritePad-LICENSE.txt" "$source_dir/tools/writepad/COPYING"
    COPYFILE_DISABLE=1 tar -C "$stage" -czf "$stage/$source_archive" "$companion"
    tar -tzf "$stage/$source_archive" >/dev/null
fi

(cd "$stage" && zip -X -q -r "$archive" "$bundle" && unzip -tq "$archive")
# Publish the completed archives without overwriting any existing file.
trap '' HUP INT TERM
if [ -n "$writepad_source" ]; then
    ln "$stage/$source_archive" "$output/"
    companion_linked=true
fi
ln "$stage/$archive" "$output/"
companion_linked=false
trap 'exit 1' HUP INT TERM

printf '%s\n' "Artifacts in $output"
cd "$output"
if [ -n "$writepad_source" ]; then
    sha256 "$archive" "$source_archive"
else
    sha256 "$archive"
fi
