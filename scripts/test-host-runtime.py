#!/usr/bin/env python3
"""Offline regressions for production setup/loop, calendar, pump limits and GPIO."""
from pathlib import Path
import subprocess
import sys

if __name__ == "__main__":
    raise SystemExit(subprocess.call([sys.executable,
        str(Path(__file__).with_name("test-host-startup.py")), "--runtime", *sys.argv[1:]]))
