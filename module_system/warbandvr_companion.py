# SPDX-License-Identifier: GPL-3.0-only
# WarbandVR companion mission-template triggers.
#
# Drop-in source for a Warband Module System build. This file is intentionally
# plain Python 2/3-compatible module-system code: the DLL injects F1-F6, and
# these triggers translate those synthetic gesture keys into real equipment
# state using Warband script operations.
#
# Integration:
#   1. Copy this file beside your module system's module_mission_templates.py.
#   2. In module_mission_templates.py:
#        from warbandvr_companion import warbandvr_mission_triggers
#      then extend every single-player battle template's trigger list with
#      `+ warbandvr_mission_triggers`.
#   3. For the crossbow ritual, also extend crossbow item definitions with
#      `warbandvr_crossbow_item_triggers` (field 9 / triggers).
#
# Slot layout is configurable. Defaults use the four hard-coded weapon slots:
#   ek_item_0 = left hip, ek_item_1 = right hip,
#   ek_item_2 = over-shoulder/two-hander, ek_item_3 = quiver/bolts.
# Adjust these four constants if your module arranges equipment differently.

from header_operations import *
from header_common import *
from header_triggers import *
from header_game_keys import *
from header_items import *

VR_SLOT_HIP_LEFT   = 0
VR_SLOT_HIP_RIGHT  = 1
VR_SLOT_SHOULDER   = 2
VR_SLOT_QUIVER     = 3

# Custom agent slots. Keep clear of slots used by the host module.
VR_AGENT_SLOT_ATTACK_DIR  = 100
VR_AGENT_SLOT_RELOAD_STEP = 101
VR_AGENT_SLOT_BOLT        = 102  # crossbow ritual: bolt seated (0/1)
VR_AGENT_SLOT_SPANNED     = 103  # crossbow ritual: string drawn (0/1)

# Warband engine attack directions used by agent_set_attack_action.
VR_ATTACK_THRUST     = 0
VR_ATTACK_SLASHRIGHT = 1
VR_ATTACK_SLASHLEFT  = 2
VR_ATTACK_OVERSWING  = 3

# The DLL already classifies physical swings and steers held attacks. This
# optional assist is for modules that want the last grab gesture to bias the
# next stock mouse attack too. Leave 0 for the native-feel default.
VR_ENABLE_MELEE_DIRECTION_ASSIST = 0


def _vr_player_agent():
    return (get_player_agent_no, ":warbandvr_agent")


def _vr_equip_trigger(key_id, equipment_slot, attack_dir, message):
    """Build one grab-gesture -> equipment switch trigger."""
    return (0, 0, 0.25,
        [
            (key_is_down, key_id),
        ],
        [
            _vr_player_agent(),
            (agent_is_alive, ":warbandvr_agent"),
            (agent_get_item_slot, ":warbandvr_item", ":warbandvr_agent", equipment_slot),
            (gt, ":warbandvr_item", -1),
            # IMPORTANT: agent_equip_item takes an item id, not an equipment
            # slot number. agent_get_item_slot performs the slot -> item hop.
            (agent_equip_item, ":warbandvr_agent", ":warbandvr_item"),
            (agent_set_wielded_item, ":warbandvr_agent", ":warbandvr_item"),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_ATTACK_DIR, attack_dir),
            (display_message, message),
        ])


warbandvr_equip_triggers = [
    _vr_equip_trigger(key_f1, VR_SLOT_HIP_LEFT,  VR_ATTACK_SLASHLEFT,
                      "@VR: left-hip weapon"),
    _vr_equip_trigger(key_f2, VR_SLOT_HIP_RIGHT, VR_ATTACK_SLASHRIGHT,
                      "@VR: right-hip weapon"),
    _vr_equip_trigger(key_f3, VR_SLOT_SHOULDER,  VR_ATTACK_OVERSWING,
                      "@VR: shoulder weapon"),
    # F5 doubles as "two-handed stance" when no crossbow reload is in progress.
    (0, 0, 0.25,
        [
            (key_is_down, key_f5),
            _vr_player_agent(),
            (agent_slot_eq, ":warbandvr_agent", VR_AGENT_SLOT_RELOAD_STEP, 0),
        ],
        [
            (agent_get_item_slot, ":warbandvr_item", ":warbandvr_agent", VR_SLOT_SHOULDER),
            (gt, ":warbandvr_item", -1),
            (agent_equip_item, ":warbandvr_agent", ":warbandvr_item"),
            (agent_set_wielded_item, ":warbandvr_agent", ":warbandvr_item"),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_ATTACK_DIR, VR_ATTACK_OVERSWING),
            (display_message, "@VR: two-handed stance"),
        ]),
]

# Optional stock-attack direction assist. When enabled, pressing attack with a
# remembered grab direction ready-and-holds that attack direction once. Physical
# swing classification in the DLL remains the primary/native-feeling path.
warbandvr_melee_assist_triggers = [
    (0.05, 0, 0,
        [
            (game_key_is_down, gk_attack),
            _vr_player_agent(),
            (agent_is_alive, ":warbandvr_agent"),
            (agent_slot_ge, ":warbandvr_agent", VR_AGENT_SLOT_ATTACK_DIR, 0),
            (agent_get_attack_action, ":warbandvr_action", ":warbandvr_agent"),
            (eq, ":warbandvr_action", 0),  # free
        ],
        [
            (agent_get_slot, ":warbandvr_dir", ":warbandvr_agent", VR_AGENT_SLOT_ATTACK_DIR),
            (agent_set_attack_action, ":warbandvr_agent", ":warbandvr_dir", 1),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_ATTACK_DIR, -1),
        ]),
] if VR_ENABLE_MELEE_DIRECTION_ASSIST else []

# Add these to every crossbow item's trigger list (module_items field 9). The
# trigger fires after a bolt is released; the ritual then gates the next shot.
warbandvr_crossbow_item_triggers = [
    (ti_on_weapon_attack,
        [
            (store_trigger_param_1, ":warbandvr_agent"),
            (agent_get_wielded_item, ":warbandvr_weapon", ":warbandvr_agent", 0),
            (item_get_type, ":warbandvr_kind", ":warbandvr_weapon"),
            (eq, ":warbandvr_kind", itp_type_crossbow),
        ],
        [
            (agent_set_ammo, ":warbandvr_agent", ":warbandvr_weapon", 0),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_RELOAD_STEP, 1),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_BOLT, 0),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_SPANNED, 0),
            (display_message, "@VR: crossbow spent - reach to quiver"),
        ]),
]

warbandvr_reload_triggers = [
    # Bolt nock: quiver grab (F4) takes a bolt from the configured quiver
    # slot. Independent of the spanning step - the BannerlordVR lesson is
    # that seating the bolt and spanning the string can happen in EITHER
    # order; both are required to fire. BOLT==0 keeps it idempotent.
    (0, 0, 0.25,
        [
            (key_is_down, key_f4),
            _vr_player_agent(),
            (agent_is_alive, ":warbandvr_agent"),
            (agent_slot_ge, ":warbandvr_agent", VR_AGENT_SLOT_RELOAD_STEP, 1),
            (agent_slot_eq, ":warbandvr_agent", VR_AGENT_SLOT_BOLT, 0),
        ],
        [
            (agent_get_item_slot, ":warbandvr_ammo", ":warbandvr_agent", VR_SLOT_QUIVER),
            (gt, ":warbandvr_ammo", -1),
            (item_get_type, ":warbandvr_ammo_kind", ":warbandvr_ammo"),
            (eq, ":warbandvr_ammo_kind", itp_type_bolts),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_BOLT, 1),
            (display_message, "@VR: bolt nocked"),
        ]),
    # String span: both-grips gesture (F5) draws the string, independent of
    # the bolt step. SPANNED==0 keeps it idempotent.
    (0, 0, 0.25,
        [
            (key_is_down, key_f5),
            _vr_player_agent(),
            (agent_is_alive, ":warbandvr_agent"),
            (agent_slot_ge, ":warbandvr_agent", VR_AGENT_SLOT_RELOAD_STEP, 1),
            (agent_slot_eq, ":warbandvr_agent", VR_AGENT_SLOT_SPANNED, 0),
        ],
        [
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_SPANNED, 1),
            (display_message, "@VR: string spanned"),
        ]),
    # Completion: bolt AND string (either order) seat one bolt and reset.
    (0, 0, 0.25,
        [
            _vr_player_agent(),
            (agent_is_alive, ":warbandvr_agent"),
            (agent_slot_ge, ":warbandvr_agent", VR_AGENT_SLOT_RELOAD_STEP, 1),
            (agent_slot_eq, ":warbandvr_agent", VR_AGENT_SLOT_BOLT, 1),
            (agent_slot_eq, ":warbandvr_agent", VR_AGENT_SLOT_SPANNED, 1),
            (agent_get_wielded_item, ":warbandvr_weapon", ":warbandvr_agent", 0),
            (item_get_type, ":warbandvr_kind", ":warbandvr_weapon"),
            (eq, ":warbandvr_kind", itp_type_crossbow),
        ],
        [
            (agent_set_ammo, ":warbandvr_agent", ":warbandvr_weapon", 1),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_RELOAD_STEP, 0),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_BOLT, 0),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_SPANNED, 0),
            (display_message, "@VR: crossbow spanned"),
        ]),
    # Watchdog: if the ritual is mid-flight but the wielded weapon is no
    # longer a crossbow (weapon switched, dropped, agent swapped), reset the
    # state machine so it can never soft-lock.
    (1.0, 0, 0,
        [
            _vr_player_agent(),
            (agent_is_alive, ":warbandvr_agent"),
            (agent_slot_ge, ":warbandvr_agent", VR_AGENT_SLOT_RELOAD_STEP, 1),
            (agent_get_wielded_item, ":warbandvr_weapon", ":warbandvr_agent", 0),
            (item_get_type, ":warbandvr_kind", ":warbandvr_weapon"),
            (neq, ":warbandvr_kind", itp_type_crossbow),
        ],
        [
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_RELOAD_STEP, 0),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_BOLT, 0),
            (agent_set_slot, ":warbandvr_agent", VR_AGENT_SLOT_SPANNED, 0),
            (display_message, "@VR: reload cancelled"),
        ]),
]

# Extend single-player battle mission templates with this list.
warbandvr_mission_triggers = (warbandvr_equip_triggers +
                              warbandvr_reload_triggers +
                              warbandvr_melee_assist_triggers)
