#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Static checks for the GitHub Actions build workflow."""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parent.parent
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"

if not WORKFLOW.is_file():
    print("FAIL: .github/workflows/build.yml is missing")
    sys.exit(1)

text = WORKFLOW.read_text(encoding="utf-8")
required = {
    "headless regression job": "headless-regression:",
    "full headless gate": "sh tests/run_all.sh",
    "MSVC setup": "microsoft/setup-msbuild@v2",
    "Release job": "msvc-release:",
    "Release configuration": "/p:Configuration=Release",
    "OpenXR job": "msvc-openxr:",
    "OpenXR configuration": "/p:Configuration=Release-OpenXR",
    "Win32 platform": "/p:Platform=Win32",
    "warnings fatal": "/p:TreatWarningAsError=true",
    "pinned OpenXR SDK": "OPENXR_TAG: release-1.1.63",
    "x86 loader architecture": "-A Win32",
    "x86 PE check": "0x14C",
    "artifact upload": "actions/upload-artifact@v4",
}

missing = [name for name, needle in required.items() if needle not in text]
if missing:
    for name in missing:
        print(f"FAIL: workflow missing {name}")
    sys.exit(1)

print("workflow check: GitHub Actions MSVC and headless gates present")
