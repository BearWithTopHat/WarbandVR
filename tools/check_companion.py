#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Static checks for the WarbandVR companion module-system source."""
import pathlib
import py_compile
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "module_system" / "warbandvr_companion.py"
source = SRC.read_text(encoding="utf-8")
py_compile.compile(str(SRC), doraise=True)

required = [
    "agent_equip_item",
    "agent_get_item_slot",
    "agent_set_wielded_item",
    "agent_set_attack_action",
    "agent_set_ammo",
    "ti_on_weapon_attack",
    "warbandvr_mission_triggers",
    "warbandvr_crossbow_item_triggers",
]
missing = [name for name in required if name not in source]
if missing:
    print("companion check: missing", ", ".join(missing))
    sys.exit(1)

# Regression guard: agent_equip_item takes an item id. The slot must be
# resolved through agent_get_item_slot first; never pass an integer slot.
if re.search(r"\(agent_equip_item\s*,\s*[^,]+,\s*\d+\s*\)", source):
    print("companion check: agent_equip_item called with an integer slot")
    sys.exit(1)
for call in re.finditer(r"\(agent_equip_item\s*,", source):
    before = source[:call.start()]
    if "agent_get_item_slot" not in before:
        print("companion check: agent_equip_item lacks prior slot->item lookup")
        sys.exit(1)

# Gesture-key surface: three direct equips, one stance equip, two reload steps.
if len(re.findall(r"_vr_equip_trigger\(", source)) < 4:  # def + three calls
    print("companion check: expected three _vr_equip_trigger call sites")
    sys.exit(1)
for key in ("key_f1", "key_f2", "key_f3", "key_f4", "key_f5"):
    if key not in source:
        print("companion check: missing", key)
        sys.exit(1)

# Ritual order in the emitted trigger source: spend -> quiver -> span.
order = [source.index("crossbow spent"), source.index("bolt nocked"),
         source.index("crossbow spanned")]
if order != sorted(order):
    print("companion check: crossbow ritual stages are out of order")
    sys.exit(1)

print("companion check: OK")
