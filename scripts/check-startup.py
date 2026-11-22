#!/usr/bin/env python3

import argparse
import os
from pathlib import Path
import subprocess
import tempfile


def check_startup(binary, mode, arguments, profile):
    profile.mkdir(mode=0o700)
    runtime = profile / "runtime"
    runtime.mkdir(mode=0o700)
    environment = dict(
        os.environ,
        HOME=str(profile),
        CFFIXED_USER_HOME=str(profile),
        XDG_CACHE_HOME=str(profile / "cache"),
        XDG_CONFIG_HOME=str(profile / "config"),
        XDG_DATA_HOME=str(profile / "data"),
        XDG_RUNTIME_DIR=str(runtime),
        QT_QPA_PLATFORM="offscreen",
        QT_QUICK_BACKEND="software",
    )
    try:
        result = subprocess.run(
            [str(binary), *arguments, "--check-startup"],
            cwd=profile,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=12,
        )
    except subprocess.TimeoutExpired as error:
        output = (error.stdout or b"").decode(errors="replace").rstrip()
        raise SystemExit(f"{mode} startup timed out before readiness\n{output}")

    output = result.stdout.decode(errors="replace").rstrip()
    if result.returncode != 0 or "QUADERNO_READY" not in output.splitlines():
        raise SystemExit(f"{mode} startup did not render a ready UI ({result.returncode})\n{output}")
    if "failed to load component" in output.lower() or any(
        marker in output for marker in ("ReferenceError:", "TypeError:", "Binding loop detected", "Unable to assign", "Cannot assign")
    ):
        raise SystemExit(f"{mode} startup reported a QML error\n{output}")
    print(f"{mode} rendered startup passed")


def main():
    parser = argparse.ArgumentParser(description="Check that Quaderno starts in both layouts offscreen.")
    parser.add_argument("binary", type=Path, help="Path to the Quaderno executable")
    options = parser.parse_args()
    binary = options.binary.resolve()
    if not binary.is_file() or not os.access(binary, os.X_OK):
        parser.error(f"Quaderno executable not found or is not executable: {binary}")

    with tempfile.TemporaryDirectory(prefix="quaderno-startup-") as scratch:
        check_startup(binary, "Desktop", [], Path(scratch) / "desktop")
        check_startup(binary, "Tablet layout", ["--tablet"], Path(scratch) / "tablet")


if __name__ == "__main__":
    main()
