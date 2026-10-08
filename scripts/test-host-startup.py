#!/usr/bin/env python3
"""Offline regressions for the production MQTT formatter, NTP startup and LCD."""
import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def run(command, **kwargs):
    print(shlex.join(str(arg) for arg in command), flush=True)
    subprocess.run(command, check=True, cwd=ROOT, **kwargs)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    library = ROOT / ".arduino/user/libraries/NTPClient"
    pin = next(item for item in json.loads(
        (ROOT / "build-support/b0-lock.json").read_text())["libraries"]
               if item["name"] == "NTPClient")
    if not (library / "NTPClient.cpp").is_file():
        parser.error("Missing pinned NTPClient; run scripts/b0-prepare.py first.")
    commit = subprocess.check_output(
        ["git", "-C", str(library), "rev-parse", "HEAD"], text=True).strip()
    dirty = subprocess.check_output(
        ["git", "-C", str(library), "status", "--porcelain", "--untracked-files=all"],
        text=True)
    if (commit != pin["commit"] or dirty or
            f"version={pin['version']}" not in
            (library / "library.properties").read_text().splitlines()):
        parser.error("NTPClient revision/version/worktree differs from b0-lock.json")
    print(f"NTPClient {pin['version']} {commit}; clean", flush=True)
    compiler = os.environ.get("CXX", "clang++")
    run([compiler, "--version"])
    output = ROOT / "build/host-startup" / ("sanitize" if args.sanitize else "normal")
    source = output / "source"
    source.mkdir(parents=True, exist_ok=True)
    # Like the firmware build, stage current production sources so their local
    # secrets.h include resolves only to the example. Never read real secrets.
    modules = ("utils", "screen", "scheduler", "heating_config")
    for module in modules:
        for suffix in (".cpp", ".h"):
            shutil.copy2(ROOT / "heating_server" / (module + suffix), source)
    shutil.copy2(ROOT / "heating_server/secrets.example.h", source / "secrets.h")
    binary = output / "startup-tests"
    command = [compiler, "-std=c++11", "-g", "-O1", "-Wall", "-Wextra",
               "-Wno-deprecated-declarations", "-fno-exceptions", "-fno-rtti",
               "-I" + str(ROOT / "tests/host-startup/adapters"),
               "-I" + str(source), "-isystem", str(library),
               str(ROOT / "tests/host-startup/startup-tests.cpp"),
               *(str(source / (module + ".cpp")) for module in modules),
               str(library / "NTPClient.cpp"), "-o", str(binary)]
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
