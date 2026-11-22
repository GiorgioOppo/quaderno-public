#!/bin/sh
set -eu

unit=quaderno-demo.service

die() {
    printf '%s\n' "$*" >&2
    exit 1
}

case "${1:-}" in
    -h|--help)
        cat <<'EOF'
On the reMarkable Paper Pro, copy this script and the Linux AArch64
quaderno executable into the same directory under /home/root.

  sh try-on-device.sh          Start the demo for up to ten minutes.
  sh try-on-device.sh --check  Check the executable and tablet runtime.
  sh try-on-device.sh --stop   End the demo and return to reMarkable.
  sh try-on-device.sh --persistent  Run from AppLoad until the user exits.

You can also exit from the demo on the tablet. If USB SSH disconnects,
the demo keeps running. After reconnecting you can check the log with
journalctl -u quaderno-demo.service.
EOF
        exit 0
        ;;
    ''|--check|--stop|--persistent) ;;
    *) die "Use --help for usage." ;;
esac
[ "$#" -le 1 ] || die "Use --help for usage."

[ "$(id -u)" = 0 ] || die "Run this script as root on the tablet."
command -v systemctl >/dev/null || die "systemctl is required on the tablet."

if [ "${1:-}" = --stop ]; then
    [ "$(systemctl show --property=Transient --value "$unit")" = yes ] ||
        die "No Quaderno demo is running."
    exec systemctl stop "$unit"
fi

[ "$(uname -m)" = aarch64 ] || die "This demo requires a reMarkable Paper Pro."
model=$(cat /sys/devices/soc0/machine 2>/dev/null || true)
case "$model" in
    *Ferrari*|*ferrari*) ;;
    *)
        if [ -r /proc/device-tree/model ]; then
            model=$(tr -d '\000' < /proc/device-tree/model)
        fi
        case "$model" in
            *Ferrari*|*ferrari*|'reMarkable Paper Pro') ;;
            *) die "The reMarkable Paper Pro (Ferrari) was not detected." ;;
        esac
        ;;
esac

app_dir=$(CDPATH='' cd -P "$(dirname "$0")" && pwd) ||
    die "Cannot locate the demo directory."
case "$app_dir" in
    /home/root|/home/root/*) ;;
    *) die "Copy the demo to a directory under /home/root." ;;
esac
app="$app_dir/quaderno"
[ -f "$app" ] && [ -x "$app" ] ||
    die "An executable named quaderno is required next to this script."
# Check the ELF class, byte order, and e_machine using BusyBox-compatible flags.
header=$(dd if="$app" bs=1 count=6 2>/dev/null | od -b | awk 'NR == 1 {for (i = 2; i <= NF; i++) printf "%s", $i}')
machine=$(dd if="$app" bs=1 skip=18 count=2 2>/dev/null | od -b | awk 'NR == 1 {for (i = 2; i <= NF; i++) printf "%s", $i}')
[ "$header" = 177105114106002001 ] && [ "$machine" = 267000 ] ||
    die "quaderno must be a Linux AArch64 build."

loader=/usr/lib/ld-linux-aarch64.so.1
[ -x "$loader" ] || loader=/lib/ld-linux-aarch64.so.1
[ -x "$loader" ] || die "Cannot find the tablet's AArch64 dynamic loader."
if ! dependencies=$("$loader" --list "$app" 2>&1); then
    printf '%s\n' "$dependencies" >&2
    die "The installed libraries cannot load Quaderno."
fi
[ -r /usr/lib/plugins/platforms/libepaper.so ] &&
    [ -r /usr/lib/plugins/scenegraph/libqsgepaper.so ] ||
    die "Cannot find the tablet's Qt e-paper plugins."

if [ "${1:-}" = --check ]; then
    handwriting="$app_dir/quaderno-handwriting"
    dictionary="$app_dir/quaderno-handwriting-it.dct"
    if [ -e "$handwriting" ] || [ -e "$dictionary" ]; then
        if [ ! -x "$handwriting" ] || [ ! -s "$dictionary" ] || ! timeout 10 "$handwriting" --check; then
            die "Cannot start the offline handwriting engine or read its Italian dictionary."
        fi
        english_dictionary="$app_dir/quaderno-handwriting-en.dct"
        if [ -e "$english_dictionary" ]; then
            [ -s "$english_dictionary" ] && timeout 10 "$handwriting" --check --language en ||
                die "Cannot load the offline English handwriting dictionary."
            printf '%s\n' 'Offline English handwriting engine detected.'
        fi
        printf '%s\n' 'Offline Italian handwriting engine detected.'
    fi
    printf '%s\n' 'Paper Pro executable, libraries and e-paper plugins detected.'
    exit 0
fi

command -v systemd-run >/dev/null || die "systemd-run is required on the tablet."
[ "$(systemctl show --property=LoadState --value xochitl.service)" = loaded ] ||
    die "Cannot find the reMarkable interface service (xochitl)."
systemctl is-active --quiet xochitl.service ||
    die "The reMarkable interface must be active before starting the demo."

# PID 1 owns both hooks, so restoring the interface does not depend on SSH.
# Record the obligation before requesting the stop, scoped to this invocation.
# The ':' prefix lets the hook's shell expand its own environment variables.
# shellcheck disable=SC2016
prepare_display='set -eu
systemctl is-active --quiet xochitl.service
marker="/home/root/.quaderno-restore-${INVOCATION_ID:?}"
(umask 077; set -C; : > "$marker")
# Keep stock autosleep from taking ownership of the display. The application
# provides its own idle screen and keeps the current document on wake.
if [ -r /sys/power/autosleep ] && [ -w /sys/power/autosleep ]; then
    previous=$(cat /sys/power/autosleep)
    case "$previous" in off|mem|standby) ;; *) exit 1 ;; esac
    printf "%s\n" "$previous" > "$marker.autosleep"
    echo off > /sys/power/autosleep
elif [ -w /sys/power/wake_lock ] && [ -w /sys/power/wake_unlock ]; then
    echo wake-lock > "$marker"
    echo quaderno-demo-display > /sys/power/wake_lock
else
    exit 1
fi
systemctl stop xochitl.service
[ "$(systemctl show --property=ActiveState --value xochitl.service)" = inactive ]'

# shellcheck disable=SC2016
restore_display='set -eu
marker="/home/root/.quaderno-restore-${INVOCATION_ID:?}"
if [ -f "$marker" ]; then
    status=0
    if [ "$(systemctl is-system-running 2>/dev/null)" != stopping ]; then
        # Each intentional return counts as a service start on stock firmware.
        systemctl reset-failed xochitl.service || true
        systemctl start xochitl.service || status=$?
    fi
    if [ -s "$marker" ] && [ -w /sys/power/wake_unlock ]; then
        echo quaderno-demo-display > /sys/power/wake_unlock || status=$?
    fi
    if [ -f "$marker.autosleep" ] && [ -w /sys/power/autosleep ]; then
        previous=$(cat "$marker.autosleep")
        case "$previous" in
            off|mem|standby) echo "$previous" > /sys/power/autosleep || status=$? ;;
            *) status=1 ;;
        esac
        rm -f "$marker.autosleep"
    fi
    rm -f "$marker"
    exit "$status"
fi'

printf 'Starting the demo. To exit via SSH, run systemctl stop %s\n' "$unit"
runtime=10min
if [ "${1:-}" = --persistent ]; then runtime=infinity; fi
# The fixed unit name reserves the display for one demo invocation at a time.
if ! systemd-run --unit="$unit" --description='Quaderno Paper Pro demo' \
    --wait --collect --service-type=exec \
    --setenv=HOME=/home/root --setenv=QT_QUICK_BACKEND=epaper \
    --property="RuntimeMaxSec=$runtime" \
    --property=TimeoutStartSec=60s --property=TimeoutStopSec=60s \
    --property="ExecStartPre=:/bin/sh -c '$prepare_display'" \
    --property="ExecStopPost=:/bin/sh -c '$restore_display'" \
    "$app" --tablet -platform epaper; then
    die "The demo ended with an error. On the tablet, run journalctl -u $unit."
fi
