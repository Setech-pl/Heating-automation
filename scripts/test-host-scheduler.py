#!/usr/bin/env python3
"""Deterministic offline target for the production central scheduler/controller."""
import argparse
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
    compiler = os.environ.get("CXX", "clang++")
    run([compiler, "--version"])
    output = ROOT / "build/host-scheduler" / ("sanitize" if args.sanitize else "normal")
    source = output / "source"
    source.mkdir(parents=True, exist_ok=True)
    for module in ("scheduler", "heating_config"):
        for suffix in (".cpp", ".h"):
            shutil.copy2(ROOT / "heating_server" / (module + suffix), source)
    shutil.copy2(ROOT / "heating_server/secrets.example.h", source / "secrets.h")
    flags = ["-std=c++11", "-g", "-O1", "-Wall", "-Wextra",
             "-fno-exceptions", "-fno-rtti",
             "-I" + str(ROOT / "tests/host-scheduler/adapters"),
             "-I" + str(ROOT / "tests/host-startup/adapters"),
             "-I" + str(source)]
    if args.sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                  "-fno-sanitize-recover=all"]
    # Scheduler warnings are errors. Keep existing unrelated config warnings visible.
    scheduler = output / "scheduler.o"
    run([compiler, *flags, "-Werror", "-c", str(source / "scheduler.cpp"),
         "-o", str(scheduler)])
    tests = output / "tests.o"
    run([compiler, *flags, "-Werror", "-c",
         str(ROOT / "tests/host-scheduler/scheduler-tests.cpp"), "-o", str(tests)])
    binary = output / "scheduler-tests"
    run([compiler, *flags, str(tests), str(scheduler),
         str(source / "heating_config.cpp"), "-o", str(binary)])
    environment = os.environ.copy()
    if args.sanitize:
        environment["ASAN_OPTIONS"] = "detect_leaks=0:halt_on_error=1"
        environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    run([str(binary)], env=environment, timeout=30)


if __name__ == "__main__":
    main()
