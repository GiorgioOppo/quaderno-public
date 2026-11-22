#!/usr/bin/env python3

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


REQUIRED_SUITES = {
    "regression", "reader", "lasso", "chartrecognition", "diagramrecognition", "diagramcontext",
    "booknotes", "booknotesintegration", "diagramsintegration", "workspaceintegration", "ui",
    "settingsintegration", "tabletpreferences", "screenorientation", "markerbattery", "librarymaintenance",
    "sync", "blesync", "wirelesssync", "syncrendezvous", "paperprosshaccess", "cloud", "cloud_helper", "smtp",
}


def isolated_environment(profile):
    runtime = profile / "runtime"
    runtime.mkdir(mode=0o700)
    return dict(
        os.environ,
        CFFIXED_USER_HOME=str(profile),
        XDG_CACHE_HOME=str(profile / "cache"),
        XDG_CONFIG_HOME=str(profile / "config"),
        XDG_DATA_HOME=str(profile / "data"),
        XDG_RUNTIME_DIR=str(runtime),
    )


def run_qa(build, binary, ctest, parallel):
    with tempfile.TemporaryDirectory(prefix="quaderno-qa-") as scratch:
        environment = isolated_environment(Path(scratch))
        command = [ctest, "--test-dir", str(build)]
        discovery = subprocess.run(
            [*command, "--show-only=json-v1"],
            env=environment,
            check=True,
            capture_output=True,
            text=True,
        )
        registered = {test["name"] for test in json.loads(discovery.stdout)["tests"]}
        missing = REQUIRED_SUITES - registered
        if missing:
            raise ValueError(f"Missing CTest suites: {', '.join(sorted(missing))}")

        report = build / "qa-results.xml"
        print(f"Running {len(registered)} CTest suites. JUnit report: {report}", flush=True)
        tests = subprocess.run(
            [*command, "--parallel", str(parallel), "--output-on-failure", "--output-junit", str(report),
             "--no-tests=error"],
            env=environment,
        )
        startup = subprocess.run(
            [sys.executable, str(Path(__file__).with_name("check-startup.py")), str(binary)],
            env=environment,
        )
        return tests.returncode or startup.returncode


def main():
    parser = argparse.ArgumentParser(description="Run all Quaderno CTest suites and both rendered startup checks.")
    parser.add_argument("build", type=Path, help="Configured and built CMake directory with BUILD_TESTING=ON")
    parser.add_argument("--binary", type=Path, help="Executable to use for the startup checks")
    parser.add_argument("--ctest", default="ctest", help="Path to the CTest executable")
    parser.add_argument("--parallel", type=int, default=2, metavar="JOBS", help="Number of concurrent CTest suites")
    options = parser.parse_args()
    build = options.build.resolve()
    if options.parallel < 1:
        parser.error("--parallel must be greater than zero")
    if not (build / "CTestTestfile.cmake").is_file():
        parser.error(f"CTest registration not found in {build}")

    default_binary = "quaderno.app/Contents/MacOS/quaderno" if sys.platform == "darwin" else "quaderno"
    binary = (options.binary or build / default_binary).resolve()
    if not binary.is_file() or not os.access(binary, os.X_OK):
        parser.error(f"Quaderno executable not found or is not executable: {binary}")

    try:
        return run_qa(build, binary, options.ctest, options.parallel)
    except subprocess.CalledProcessError as error:
        parser.exit(1, f"Could not list CTest suites: {error.stderr.strip()}\n")
    except (OSError, KeyError, ValueError) as error:
        parser.exit(1, f"Could not run QA: {error}\n")


if __name__ == "__main__":
    sys.exit(main())
