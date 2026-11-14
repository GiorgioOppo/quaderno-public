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
            [str(binary), *arguments],
            cwd=profile,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=5,
        )
    except subprocess.TimeoutExpired:
        print(f"{mode} startup passed")
        return

    output = result.stdout.decode(errors="replace").rstrip()
    raise SystemExit(f"{mode} startup exited early ({result.returncode})\n{output}")


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
