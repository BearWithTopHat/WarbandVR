# WarbandVR companion module

`warbandvr_companion.py` is the optional script-side half of the VR gesture
system. The DLL only injects keys; this module turns those keys into real
Warband equipment actions.

## Install into a module-system source tree

1. Copy `warbandvr_companion.py` next to `module_mission_templates.py`.
2. In `module_mission_templates.py`, import it:

   ```python
   from warbandvr_companion import warbandvr_mission_triggers
   ```

3. Extend every single-player battle template trigger list:

   ```python
   triggers = existing_triggers + warbandvr_mission_triggers
   ```

4. For the manual crossbow ritual, add `warbandvr_crossbow_item_triggers` to
   each crossbow item's optional trigger field in `module_items.py`.
5. Rebuild the module with the normal Module System build script.

The default slot mapping is:

| Gesture key | Body zone | Equipment slot |
| --- | --- | --- |
| F1 | left hip | `ek_item_0` |
| F2 | right hip | `ek_item_1` |
| F3 | shoulder / two-hander | `ek_item_2` |
| F4 | quiver | `ek_item_3` (reload ritual step) |
| F5 | both grips | two-handed stance, or crossbow spanning during a reload |

Change the `VR_SLOT_*` constants near the top of the file if the host module
uses a different loadout layout.

## Scope notes

- Single-player missions only.
- `agent_equip_item` and `agent_set_wielded_item` receive the **item id**
  returned by `agent_get_item_slot`; passing a raw slot number is wrong.
- The DLL's motion swing classifier is the primary melee-direction path. The
  companion's `VR_ENABLE_MELEE_DIRECTION_ASSIST` option only biases the next
  stock mouse attack after a grab gesture for modules that want that fallback.
- Manual crossbow reload is a permission ritual: after a shot, ammo is held at
  zero until BOTH the quiver grab (bolt) and the both-grips spanning gesture
  (string) complete — in either order. Switching weapons mid-ritual resets it
  within ~1 s. The reload animation timing remains Warband's own.

The source is syntax- and invariant-checked by `tools/check_companion.py` in
the repository test gate.
