#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a clean WarbandVR source package.

The package intentionally contains source, docs, tests, module-system support,
and build tooling only. Local build products, generated test binaries, Python
caches, SDK drop-ins, and log/capture artifacts are excluded.
"""

from __future__ import annotations

import argparse
import os
import shutil
import tempfile
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

TOP_LEVEL = [
    ".gitignore",
    ".github",
    "AGENTS.md",
    "LICENSE",
    "NOTICE",
    "README.md",
    "full-from-baseline.patch",
    "WarbandVR.def",
    "WarbandVR.ini",
    "WarbandVR.sln",
    "WarbandVR.vcxproj",
    "docs",
    "module_system",
    "src",
    "tests",
    "tools",
]

EXCLUDED_DIRS = {".git", "__pycache__", "Release", "Debug", "WarbandVR", "obj"}
EXCLUDED_SUFFIXES = {".pyc", ".o", ".obj", ".exe", ".dll", ".lib", ".exp", ".pdb", ".bmp"}
EXCLUDED_TEST_BINARIES = {
    "run_tests",
    "run_integration",
    "run_integration_asan",
    "run_fuzz",
    "run_motion_input",
}

NATIVE_HEADER = """; ============================================================================
; WarbandVR native Steam Frame profile (Phase 2 experiment).
;
; This file is generated from the shared PCVR configuration. The source tree
; is byte-identical to the PCVR package; only this annotated profile differs.
; Native execution depends on Proton + FEX/ARM64 plus a working Wine OpenXR
; path and remains experimental. Keep EnableStereo=0 for the first boot.
; ============================================================================

"""


def should_skip(path: Path) -> bool:
    rel = path.relative_to(ROOT)
    if any(part in EXCLUDED_DIRS for part in rel.parts):
        return True
    if path.suffix in EXCLUDED_SUFFIXES:
        return True
    if rel.parts and rel.parts[0] == "tests" and path.name in EXCLUDED_TEST_BINARIES:
        return True
    return False


def copy_tree(destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    destination.mkdir(parents=True)
    for name in TOP_LEVEL:
        src = ROOT / name
        if not src.exists():
            raise FileNotFoundError(f"required release input missing: {name}")
        dst = destination / name
        if src.is_dir():
            def ignore(_directory: str, names: list[str]) -> set[str]:
                ignored = set()
                for name_part in names:
                    p = Path(_directory) / name_part
                    if p.is_dir() and name_part in EXCLUDED_DIRS:
                        ignored.add(name_part)
                    elif p.is_file() and should_skip(p):
                        ignored.add(name_part)
                return ignored
            shutil.copytree(src, dst, ignore=ignore)
        else:
            shutil.copy2(src, dst)


def apply_native_profile(destination: Path) -> None:
    ini = destination / "WarbandVR.ini"
    body = ini.read_text(encoding="utf-8")
    marker = body.find("[Stereo]")
    if marker < 0:
        raise RuntimeError("WarbandVR.ini lacks [Stereo]")
    ini.write_text(NATIVE_HEADER + body[marker:], encoding="utf-8")
    (destination / "README_NATIVE.md").write_text(
        "# WarbandVR native Steam Frame profile\n\n"
        "This package keeps the same source tree as the PCVR package. Only the "
        "annotated `WarbandVR.ini` profile differs. Phase 2 native execution is "
        "experimental; start with `EnableStereo=0`, then follow "
        "`docs/13_Native_SteamFrame.md` and the manual checklist.\n",
        encoding="utf-8",
    )


def zip_directory(source: Path, zip_path: Path) -> None:
    """Write an archive and retry once if the mounted filesystem raced us."""
    for attempt in range(2):
        if zip_path.exists():
            zip_path.unlink()
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
            for path in sorted(source.rglob("*")):
                if path.is_file():
                    zf.write(path, path.relative_to(source.parent))
        os.sync()
        expected = {p.relative_to(source).as_posix() for p in source.rglob("*") if p.is_file()}
        with zipfile.ZipFile(zip_path) as zf:
            actual = {Path(n).relative_to(source.name).as_posix() for n in zf.namelist()}
            if zf.testzip() is not None:
                raise RuntimeError(f"zip CRC failed: {zip_path}")
        if actual == expected:
            return
        if attempt == 1:
            missing = sorted(expected - actual)
            extra = sorted(actual - expected)
            raise RuntimeError(f"zip content mismatch: missing={missing} extra={extra}")
        os.sync()


def write_build_info(package: Path, name: str, native: bool) -> None:
    """BUILD-INFO.json travels inside the package (BannerlordVR lesson):
    provenance must survive the zip being copied around, not live only in an
    external report. Carries the package identity and a manifest hash of
    every shipped file; the regression-gate output lives in the completion
    report, not here (never record results this script did not run)."""
    import datetime
    import hashlib
    import json

    entries = []
    for p in sorted(package.rglob("*")):
        if p.is_file() and p.name != "BUILD-INFO.json":
            digest = hashlib.sha256(p.read_bytes()).hexdigest()
            entries.append(f"{p.relative_to(package).as_posix()}:{digest}")
    manifest = hashlib.sha256("\n".join(entries).encode("utf-8")).hexdigest()
    patch = package / "full-from-baseline.patch"
    info = {
        "package": name,
        "profile": "native" if native else "pcvr",
        "built_at_utc": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "file_count": len(entries),
        "manifest_sha256": manifest,
        "baseline_patch_sha256": hashlib.sha256(patch.read_bytes()).hexdigest() if patch.exists() else None,
    }
    (package / "BUILD-INFO.json").write_text(json.dumps(info, indent=2) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, help="package output directory")
    parser.add_argument("--name", default="WarbandVR-PCVR", help="package directory/zip base name")
    parser.add_argument("--native", action="store_true", help="annotate the ini as the native Phase 2 profile")
    parser.add_argument("--no-zip", action="store_true", help="leave only the package directory")
    args = parser.parse_args()

    out_root = Path(args.output).resolve()
    out_root.mkdir(parents=True, exist_ok=True)
    package_dir = out_root / args.name

    # Build on the ordinary local filesystem first. The user-visible output
    # mount can briefly lag directory-copy visibility, which is hostile to
    # immediately zipping a freshly copied tree.
    staging_root = Path(tempfile.mkdtemp(prefix=f"warbandvr-{args.name}-"))
    try:
        staging_dir = staging_root / args.name
        copy_tree(staging_dir)
        if args.native:
            apply_native_profile(staging_dir)
        write_build_info(staging_dir, args.name, args.native)
        os.sync()
        if not args.no_zip:
            zip_directory(staging_dir, out_root / f"{args.name}.zip")
        if package_dir.exists():
            shutil.rmtree(package_dir)
        shutil.copytree(staging_dir, package_dir)
        os.sync()
    finally:
        shutil.rmtree(staging_root, ignore_errors=True)

    print(f"packaged {args.name}: {package_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
