#!/usr/bin/env python3
"""Prepare the pinned historical B0 environment in .arduino on macOS ARM64.

This prepares compilation tools only. It does not install Rosetta, flash a
device, update global Arduino libraries, or establish hardware compatibility.
"""
import hashlib
import json
import platform
import shutil
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / ".arduino"
LOCK = json.loads((ROOT / "build-support/b0-lock.json").read_text())


def run(*args):
    subprocess.run(args, check=True, cwd=ROOT)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise SystemExit("This B0 manifest targets macOS ARM64 only.")
    for tool in ("gh", "git", "tar"):
        if not shutil.which(tool):
            raise SystemExit(f"Missing prerequisite: {tool}")
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
            run("tar", "-xzf", str(archive), "--strip-components",
                str(item["strip"]), "-C", str(destination))
        print(f"Verified and extracted: {item['archive']}", flush=True)

    # Empty Arduino indexes avoid opportunistic downloads/updates. The pinned
    # ESP8266 release index and installed packages provide offline compilation.
    data = LOCAL / "data"
    for name, content in (("package_index.json", {"packages": []}),
                          ("library_index.json", {"libraries": []})):
        (data / name).write_text(json.dumps(content) + "\n")
    for item in LOCK["libraries"]:
        path = LOCAL / "user/libraries" / item["name"]
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

    lcd = LOCK["vendored_lcd"]
    destination = LOCAL / "user/libraries/LiquidCrystal_I2C"
    destination.mkdir(parents=True, exist_ok=True)
    for name, digest in lcd["files"].items():
        source = ROOT / lcd["source"] / name
        if sha256(source) != digest:
            raise SystemExit(f"Vendored LCD changed: {name}; review lock first")
        shutil.copy2(source, destination / name)
    (LOCAL / "data/packages/builtin/tools/ctags/5.8-arduino11/ctags").chmod(0o755)
    print("Environment prepared; compiler/ctags host compatibility still requires a build.")


if __name__ == "__main__":
    main()
