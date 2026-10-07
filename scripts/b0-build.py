#!/usr/bin/env python3
"""Compile the central controller using a staged dummy-only sketch."""
import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / ".arduino"
LOCK = json.loads((ROOT / "build-support/b0-lock.json").read_text())


def main():
    cli = LOCAL / "bin/arduino-cli"
    if not cli.is_file():
        raise SystemExit("Run python3 scripts/b0-prepare.py first.")
    prepared = LOCAL / "prepared.json"
    digest = hashlib.sha256((ROOT / "build-support/b0-lock.json").read_bytes()).hexdigest()
    if not prepared.is_file() or json.loads(prepared.read_text())["lock_sha256"] != digest:
        raise SystemExit("Build manifest changed/not prepared; run python3 scripts/b0-prepare.py.")
    work = ROOT / "build/b0"
    sketch = work / "sketch/heating_server"
    if sketch.exists():
        shutil.rmtree(sketch)
    sketch.mkdir(parents=True)
    # Enumerate tracked production files; do not read any local secrets.h.
    tracked = subprocess.check_output(
        ["git", "ls-files", "-z", "--", "heating_server"], cwd=ROOT)
    for raw in tracked.split(b"\0"):
        if not raw:
            continue
        relative = Path(os.fsdecode(raw))
        if (relative.parent == Path("heating_server")
                and relative.suffix in (".ino", ".cpp", ".h")
                and not relative.name.startswith("secrets")):
            shutil.copy2(ROOT / relative, sketch / relative.name)
    shutil.copy2(ROOT / "heating_server/secrets.example.h", sketch / "secrets.h")

    env = os.environ.copy()
    env.update(ARDUINO_DIRECTORIES_DATA=str(LOCAL / "data"),
               ARDUINO_DIRECTORIES_DOWNLOADS=str(LOCAL / "downloads"),
               ARDUINO_DIRECTORIES_USER=str(LOCAL / "user"),
               ARDUINO_UPDATER_ENABLE_NOTIFICATION="false")
    env.pop("ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS", None)
    env.pop("ARDUINO_CONFIG_FILE", None)
    config = LOCAL / "b0-cli.yaml"
    config.write_text("board_manager:\n  additional_urls: []\n")
    command = [str(cli), "--config-file", str(config), "compile",
               "--fqbn", LOCK["fqbn"], "--warnings", "all", "--clean",
               "--verbose", "--build-path", str(work / "compiled"),
               "--output-dir", str(work / "output"), str(sketch)]
    print("Compiling central controller with dummy configuration only.", flush=True)
    (work / "command.json").write_text(json.dumps(command, indent=2) + "\n")
    with (work / "compile.log").open("w") as log:
        result = subprocess.run(command, env=env, cwd=ROOT, stdout=log,
                                stderr=subprocess.STDOUT)
    print(f"Compile exit code: {result.returncode}; log: {work / 'compile.log'}")
    raise SystemExit(result.returncode)


if __name__ == "__main__":
    main()
