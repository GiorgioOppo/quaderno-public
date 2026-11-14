#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
set -- scripts/*.sh tools/writepad/*.sh

for script do
    sh -n "$script"
done

shellcheck --severity=warning "$@"
