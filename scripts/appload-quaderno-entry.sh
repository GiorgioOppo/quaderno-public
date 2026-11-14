#!/bin/sh
set -eu

# AppLoad launches this process inside the xochitl service. Use the system
# manager to dispatch the runner so it survives the display handoff.
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
unset LD_LIBRARY_PATH LD_PRELOAD

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

base=/home/root/quaderno
[ -L "$base/current" ] || die "Quaderno installation link is missing."
release=$(CDPATH='' cd -P "$base/current" && pwd -P) ||
    die "Cannot locate the active Quaderno installation."
case "$release" in
    "$base"/installation-*)
        version=${release#"$base"/installation-}
        case "$version" in
            ''|*[!a-zA-Z0-9]*) die "Invalid Quaderno installation path." ;;
        esac
        ;;
    *) die "Invalid Quaderno installation path." ;;
esac

runner="$release/try-on-device.sh"
[ -f "$runner" ] && [ ! -L "$runner" ] && [ -r "$runner" ] ||
    die "The active Quaderno installation is incomplete."
command -v systemd-run >/dev/null || die "systemd-run is required on the tablet."

# The runner checks the executable and the stock service before taking over.
# A fixed transient unit name prevents overlapping dispatches.
exec env -i PATH="$PATH" systemd-run --system --no-block --collect \
    --unit=quaderno-launch.service --service-type=exec \
    --description='Launch Quaderno from AppLoad' \
    --property=RuntimeMaxSec=15min /bin/sh "$runner"
