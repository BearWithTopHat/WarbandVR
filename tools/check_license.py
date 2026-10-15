#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Ensure every repository source file carries the project SPDX tag."""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
SUFFIXES = {".h", ".cpp", ".inc", ".py", ".ps1", ".sh"}
SKIP_DIRS = {".git", "Release", "Debug", "WarbandVR", "bin", "obj"}
TAG = "SPDX-License-Identifier: GPL-3.0-only"

missing = []
for path in sorted(ROOT.rglob("*")):
    if not path.is_file() or path.suffix not in SUFFIXES:
        continue
    rel = path.relative_to(ROOT)
    if any(part in SKIP_DIRS for part in rel.parts):
        continue
    head = path.read_text(encoding="utf-8", errors="ignore")[:512]
    if TAG not in head:
        missing.append(str(rel))

if missing:
    print("FAIL: missing GPL SPDX tag:")
    for rel in missing:
        print(" ", rel)
    sys.exit(1)

print(f"license audit: {TAG} present in all checked source files")
