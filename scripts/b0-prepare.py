#!/usr/bin/env python3
"""Prepare the pinned native build environment in .arduino on macOS ARM64.

This prepares compilation tools only. It does not install Rosetta, flash a
device, update global Arduino libraries, or establish hardware compatibility.
"""
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / ".arduino"
LOCK = json.loads((ROOT / "build-support/b0-lock.json").read_text())


def run(*args, cwd=ROOT, env=None):
    subprocess.run(args, check=True, cwd=cwd, env=env)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise SystemExit("This build manifest targets macOS ARM64 only.")
    if sys.version_info < (3, 9):
        raise SystemExit("Python 3.9 or newer is required.")
    for tool in ("gh", "git", "tar", "make", LOCK["ctags"]["cc"]):
        if not shutil.which(tool):
            raise SystemExit(f"Missing prerequisite: {tool}")
    (LOCAL / "prepared.json").unlink(missing_ok=True)
    downloads = LOCAL / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    for item in LOCK["artifacts"]:
        archive = downloads / item["archive"]
        if not archive.exists():
            run("gh", "release", "download", item["tag"], "--repo", item["repo"],
                "--pattern", item["archive"], "--dir", str(downloads))
        if sha256(archive) != item["sha256"]:
            raise SystemExit(f"Checksum mismatch: {archive.name}")
        destination = LOCAL / item["destination"]
        # ctags is a generated source build; remove stale objects/configuration.
        if item["destination"] == LOCK["ctags"]["source"] and destination.exists():
            shutil.rmtree(destination)
        destination.mkdir(parents=True, exist_ok=True)
        if item.get("file"):
            shutil.copy2(archive, destination / archive.name)
        elif archive.suffix == ".zip":
            with zipfile.ZipFile(archive) as source:
                for member in source.infolist():
                    parts = Path(member.filename).parts[item["strip"]:]
                    if not parts:
                        continue
                    target = destination.joinpath(*parts)
                    if not target.resolve().is_relative_to(destination.resolve()):
                        raise SystemExit("Archive path escapes destination")
                    if member.is_dir():
                        target.mkdir(parents=True, exist_ok=True)
                    else:
                        target.parent.mkdir(parents=True, exist_ok=True)
                        target.write_bytes(source.read(member))
                        mode = (member.external_attr >> 16) & 0o777
                        if mode:
                            target.chmod(mode)
        else:
            # BSD tar handles historical toolchain hard links while stripping
            # their top-level directory. Every archive is checksum-pinned.
            run("tar", "-xf", str(archive), "--strip-components",
                str(item["strip"]), "-C", str(destination))
        print(f"Verified and extracted: {item['archive']}", flush=True)

    # Empty Arduino indexes avoid opportunistic downloads/updates. The pinned
    # ESP8266 release index and installed packages provide offline compilation.
    data = LOCAL / "data"
    for name, content in (("package_index.json", {"packages": []}),
                          ("library_index.json", {"libraries": []})):
        (data / name).write_text(json.dumps(content) + "\n")
    for item in LOCK["libraries"]:
        path = LOCAL / item.get("checkout", f"user/libraries/{item['name']}")
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            run("git", "clone", "--depth", "1", "--branch", item["tag"],
                f"https://github.com/{item['repo']}.git", str(path))
        commit = subprocess.check_output(
            ["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()
        dirty = subprocess.check_output(
            ["git", "-C", str(path), "status", "--porcelain"], text=True)
        if commit != item["commit"] or dirty:
            raise SystemExit(f"Unexpected library revision/local changes: {item['name']}")
        metadata = (path / "library.properties").read_text().splitlines()
        if f"version={item['version']}" not in metadata:
            raise SystemExit(f"Library version mismatch: {item['name']}")
        print(f"Verified library: {item['name']} {item['version']} {commit}", flush=True)
        if item.get("omit_headers"):
            destination = LOCAL / "user/libraries" / item["name"]
            if destination.exists():
                shutil.rmtree(destination)
            shutil.copytree(path, destination,
                            ignore=shutil.ignore_patterns(".git", *item["omit_headers"]))
            for name, (old, new) in item.get("include_replacements", {}).items():
                header_source = destination / name
                content = header_source.read_text()
                if content.count(old) != 1:
                    raise SystemExit(f"Unexpected include in {item['name']}/{name}")
                header_source.write_text(content.replace(old, new))

    lcd = LOCK["vendored_lcd"]
    destination = LOCAL / "user/libraries/LiquidCrystal_I2C"
    destination.mkdir(parents=True, exist_ok=True)
    for name, digest in lcd["files"].items():
        source = ROOT / lcd["source"] / name
        if sha256(source) != digest:
            raise SystemExit(f"Vendored LCD changed: {name}; review lock first")
        shutil.copy2(source, destination / name)
    ctags = LOCK["ctags"]
    source = LOCAL / ctags["source"]
    # The old ctags attribute macro collides with current macOS SDK attributes.
    # Rename only the tool's macro/usages; its parser and attributes stay intact.
    for path in list(source.glob("*.c")) + list(source.glob("*.h")):
        path.write_bytes(re.sub(rb"\b__unused__\b", b"CTAGS_UNUSED", path.read_bytes()))
    env = dict(os.environ, CC=ctags["cc"], CFLAGS=ctags["cflags"])
    run("./configure", *ctags["configure"], cwd=source, env=env)
    run("make", "-j4", cwd=source, env=env)
    destination = LOCAL / ctags["destination"]
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source / "ctags", destination / "ctags")
    run(str(destination / "ctags"), "--version")

    # Core's python3 package is a launcher, not a Python version pin. Point it
    # at the existing interpreter running preparation; install nothing globally.
    python = LOCAL / "data/packages/esp8266/tools/python3/3.7.2-post1/python3"
    python.parent.mkdir(parents=True, exist_ok=True)
    if python.exists() or python.is_symlink():
        python.unlink()
    python.symlink_to(Path(sys.executable).resolve())
    compiler = LOCAL / next(item["destination"] for item in LOCK["artifacts"]
                            if item["repo"] == "esphome-libs/xtensa-lx106-elf-toolchain")
    run(str(compiler / "bin/xtensa-lx106-elf-g++"), "--version")
    (LOCAL / "prepared.json").write_text(json.dumps({
        "lock_sha256": sha256(ROOT / "build-support/b0-lock.json"),
        "python": str(Path(sys.executable).resolve()),
        "python_version": platform.python_version(),
    }, indent=2) + "\n")
    print("Native environment prepared; run python3 scripts/b0-build.py.")


if __name__ == "__main__":
    main()
