#!/usr/bin/env python3
"""Offline host target for the production heating_server UDP module."""
import argparse
import json
import os
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    print(shlex.join(str(arg) for arg in command), flush=True)
    subprocess.run(command, check=True, cwd=ROOT, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--arduino-json", type=Path,
                        default=ROOT / ".arduino/user/libraries/ArduinoJson")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    library = args.arduino_json.resolve()
    pin = next(item for item in json.loads(
        (ROOT / "build-support/b0-lock.json").read_text())["libraries"]
               if item["name"] == "ArduinoJson")
    if not (library / "src/ArduinoJson.h").is_file():
        parser.error("Missing pinned ArduinoJson sources; use --arduino-json PATH "
                     "to an existing verified checkout (see tests/host-udp/README.md).")
    commit = subprocess.check_output(
        ["git", "-C", str(library), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(
        ["git", "-C", str(library), "status", "--porcelain", "--untracked-files=all"],
        text=True)
    if (commit != pin["commit"] or dirty or
            f"version={pin['version']}" not in
            (library / "library.properties").read_text().splitlines()):
        parser.error("ArduinoJson revision/version/worktree differs from b0-lock.json")
    print(f"ArduinoJson {pin['version']} {commit}; clean", flush=True)
    compiler = os.environ.get("CXX", "clang++")
    run([compiler, "--version"])
    output = ROOT / "build/host-udp" / ("sanitize" if args.sanitize else "normal")
    output.mkdir(parents=True, exist_ok=True)
    binary = output / "udp-tests"
    command = [compiler, "-std=c++11", "-g", "-O1", "-Wall", "-Wextra",
               "-Werror", "-Wno-pragma-once-outside-header",
               "-Wno-deprecated-declarations", "-fno-exceptions",
               "-fno-rtti", "-DARDUINOJSON_USE_DOUBLE=0",
               "-DARDUINOJSON_USE_LONG_LONG=0",
               "-DARDUINOJSON_DEFAULT_NESTING_LIMIT=10",
               "-I" + str(ROOT / "tests/host-udp/adapters"),
               "-isystem", str(library / "src"),
               "-I" + str(ROOT / "heating_server"),
               str(ROOT / "tests/host-udp/udp-tests.cpp"),
               str(ROOT / "heating_server/udpmessengerservice.cpp"),
               "-o", str(binary)]
    if args.sanitize:
        command[1:1] = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-fno-sanitize-recover=all"]
    run(command)
    environment = os.environ.copy()
    if args.sanitize:
        environment["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
        environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    run([str(binary)], env=environment, timeout=30)


if __name__ == "__main__":
    main()
