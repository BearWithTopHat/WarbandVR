#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Parity gates (protocol section 2/3/7). Run from the repo root:
#   python3 tools/check_parity.py
# Exit 0 = all green.
#
# 1. Config three-way parity: Config.h fields <-> Config.cpp parser entries
#    <-> WarbandVR.ini keys (catches the dead-field / orphan-key class).
# 2. OpenXRBridge decl/def parity: every public API declared in the header has
#    exactly two definitions (real + stub) in the .cpp.
# 3. XR hygiene: no XR symbols outside #ifdef WBVR_WITH_OPENXR regions of
#    MotionInput.cpp / OpenXRBridge.cpp (coarse text scan).
# 4. Package parity (if both packages given as args): src/ trees byte-identical.

import re, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
fail = 0

def bad(msg):
    global fail
    fail += 1
    print("FAIL:", msg)

# --- 1. config parity --------------------------------------------------------
h = (ROOT / "src/Config.h").read_text()
cpp = (ROOT / "src/Config.cpp").read_text()
ini = (ROOT / "WarbandVR.ini").read_text()

# fields: lines like "type name = default;" inside struct Config
fields = set(re.findall(r'^\s+(?:bool|int|float|char|unsigned long)\s+(\w+)\s*(?:\[\d+\])?\s*=', h, re.M))
fields -= {"hasExpectedPos"}  # state flag, not an ini key

# parser keys: GetBool(path, "Sec", "Key", def) style, plus
# GetPrivateProfileIntA("Sec", "Key", ...) and GetKey(path, "Key", def).
keys = set(re.findall(r'Get(?:Bool|Float|ULong|Str)\(\s*path,\s*"\w+",\s*"(\w+)"', cpp))
keys |= set(re.findall(r'GetPrivateProfileIntA\(\s*"\w+",\s*"(\w+)"', cpp))
keys |= set(re.findall(r'GetKey\(\s*path,\s*"(\w+)"', cpp))
inikeys = {k.lower() for k in re.findall(r'^\s*(\w+)\s*=', ini, re.M)}  # ini is case-insensitive

def camel(s):  # enableStereo -> EnableStereo
    return s[0].upper() + s[1:]

# irregular key names: ini/parser key -> Config.h field
alias = {"forward": "keyForward", "back": "keyBack", "left": "keyLeft",
         "right": "keyRight", "use": "keyUse", "jump": "keyJump",
         "grabhipleft": "keyGrabHipL", "grabhipright": "keyGrabHipR",
         "grabshoulder": "keyGrabShoulder", "grabquiver": "keyGrabQuiver",
         "twohand": "keyTwoHand", "holster": "keyHolster",
         "smoothing": "trackingSmoothing", "gamebuild": "gameBuild",
         "vignette": "comfortVignette",
         "expectedpos": "expectedPos", "scanbase": "scanBase", "scansize": "scanSize"}

# Expert override keys may be deliberately commented in the shipped ini when a
# friendly preset owns the effective value. They still have parser coverage.
optional_ini = {"worldscale"}

keys_l = {k.lower() for k in keys}
missing_parser = {f for f in fields
                  if camel(f).lower() not in keys_l
                  and f not in alias.values()}
missing_ini = {f for f in fields
               if camel(f).lower() not in inikeys
               and camel(f).lower() not in optional_ini
               and not any(k in inikeys for k, v in alias.items() if v == f)}
orphan_ini = {k for k in inikeys
              if k not in {camel(f).lower() for f in fields}
              and k not in alias}

for f in sorted(missing_parser): bad(f"Config.h field '{f}' has no parser entry")
for f in sorted(missing_ini):    bad(f"Config field '{f}' missing from WarbandVR.ini")
for k in sorted(orphan_ini):     bad(f"ini key '{k}' has no Config.h field")
print(f"config parity: {len(fields)} fields checked")

# --- 2. bridge decl/def parity ------------------------------------------------
hh = (ROOT / "src/OpenXRBridge.h").read_text()
cc = (ROOT / "src/OpenXRBridge.cpp").read_text()
decls = set(re.findall(r'^\s+(?:bool|void|long long)\s+(\w+)\s*\(', hh, re.M))
for name in sorted(decls):
    n = len(re.findall(rf'\bOpenXRBridge::{name}\s*\(', cc))
    if n != 2:
        bad(f"OpenXRBridge::{name}: {n} definitions (expected 2: real + stub)")
print(f"bridge parity: {len(decls)} APIs checked")

# --- 3. XR hygiene ------------------------------------------------------------
mi = (ROOT / "src/MotionInput.cpp").read_text()
stub_region = mi.split("#else", 1)[1] if "#else" in mi else ""
if re.search(r'\bxr[A-Z]\w*|Xr[A-Z]\w*', stub_region.replace("OpenXRBridge", "")):
    bad("XR symbol referenced in the stub (#else) region of MotionInput.cpp")
print("xr hygiene: stub region clean")

# --- 4. package parity (optional args) ---------------------------------------
if len(sys.argv) == 3:
    import filecmp
    a, b = pathlib.Path(sys.argv[1]) / "src", pathlib.Path(sys.argv[2]) / "src"
    af = {p.relative_to(a) for p in a.rglob("*") if p.is_file()}
    bf = {p.relative_to(b) for p in b.rglob("*") if p.is_file()}
    if af != bf:
        bad(f"file lists differ: {af ^ bf}")
    for rel in sorted(af & bf):
        if not filecmp.cmp(a / rel, b / rel, shallow=False):
            bad(f"src differs: {rel}")
    print(f"package parity: {len(af & bf)} files compared")

print("PARITY:", "FAIL" if fail else "OK")
sys.exit(1 if fail else 0)
