#!/usr/bin/env python3

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description="Run the synthetic handwriting cases against a native helper.")
    parser.add_argument("helper", type=Path, help="Path to the packaged quaderno-handwriting executable")
    parser.add_argument("--engine", choices=("macos", "writepad"),
                        default="macos" if sys.platform == "darwin" else "writepad")
    options = parser.parse_args()
    helper = options.helper.resolve()
    if not helper.is_file() or not os.access(helper, os.X_OK):
        parser.error(f"Helper is not executable: {helper}")

    fixtures = Path(__file__).resolve().parents[1] / "tests" / "fixtures" / "handwriting.json"
    cases = json.loads(fixtures.read_text(encoding="utf-8"))
    failures = 0
    for case in cases:
        if case.get("engine", options.engine) != options.engine:
            continue
        env = os.environ.copy()
        if case["prose"]:
            env["QUADERNO_HANDWRITING_PROSE"] = "1"
        else:
            env.pop("QUADERNO_HANDWRITING_PROSE", None)
        try:
            with tempfile.TemporaryFile(mode="w+", encoding="utf-8") as ink:
                json.dump(case["strokes"], ink)
                ink.seek(0)
                result = subprocess.run(
                    [str(helper), "--language", case["language"]], stdin=ink, capture_output=True,
                    encoding="utf-8", check=True, timeout=60, env=env)
            response = json.loads(result.stdout)
            if not isinstance(response, dict) or not isinstance(response.get("text", ""), str):
                raise ValueError("invalid response from the handwriting helper")
            text = "\n".join(line.strip() for line in response.get("text", "").strip().splitlines())
            ignore_case = case["prose"] and not case.get("caseSensitive", False)
            matches = text.casefold() == case["expected"].casefold() if ignore_case else text == case["expected"]
            if not matches:
                raise ValueError(f"expected {case['expected']!r}, received {text or response.get('error')!r}")
            print(f"PASS {case['name']}: {text!r}")
        except (OSError, subprocess.SubprocessError, ValueError) as error:
            print(f"FAIL {case['name']}: {error}", file=sys.stderr)
            failures += 1
    return bool(failures)


if __name__ == "__main__":
    sys.exit(main())
