# 10 — VR combat: swings, gestures, equipment, and reload rituals

Warband's engine is closed-source, so combat is split across two layers:

1. **The DLL senses and injects.** `MotionInput` reads OpenXR controller pose,
   velocity, triggers, grips, and sticks, then emits ordinary keyboard/mouse
   input through the input-sink layer.
2. **The module acts.** The companion Module System source validates the
   player agent and equipment state, then performs equipment or reload actions
   using Warband script operations.

This split is deliberate. The DLL does not read or write inventory memory; the
module does not try to sample VR hardware.

## Feature map

| Layer | Capability | Where | Status |
|---|---|---|---|
| 1 | Controller → keyboard/mouse emulation | DLL (`MotionInput`) | Implemented, headless-tested |
| 2 | Swing classification and held-attack steering | DLL (`MotionInput` + `MeleeCore`) | Implemented, headless-tested |
| 3 | Grip block, bow-draw trigger ownership, snap turn | DLL | Implemented, headless-tested |
| 4 | Body-zone grab gestures → synthetic F1–F6 keys | DLL (`GestureZones`) | Implemented, headless-tested |
| 5 | Equipment switching and crossbow reload ritual | Companion module | Implemented, statically checked |
| 6 | True 1:1 weapon-pose control or physical blade collision | Engine internals | Out of scope |

Hardware feel and real gameplay tuning remain pending; do not confuse the
passing fake-action tests with on-head validation.

## 1. Controller emulation

With `EmulateControllers=1` and an active OpenXR session:

- left stick → WASD movement;
- right stick → mouse look or snap turn;
- right trigger → LMB in pure emulation mode;
- right grip → RMB in pure emulation mode;
- right A/B-style face buttons → configurable Use/Jump keys.

The mapping is one action per control with both hand subaction paths. This is
the spec-correct OpenXR model and is exercised by `tests/test_motion_input.cpp`
against the fake action/space harness.

## 2. Motion melee

With `MotionMelee=1`, the right-hand velocity stream is ring-buffered. When a
swing starts above `SwingThreshold` and then slows, the peak sample in the
recent window is classified as:

- downward-dominant → overhead;
- left/right-dominant → corresponding slash;
- forward/back-dominant → thrust;
- upward-dominant → ignored, because Warband has no matching attack direction.

Injection follows Warband's native input behavior: a mouse-direction delta is
sent, then LMB is held. With `MotionSteer=1`, continued hand velocity produces
more mouse deltas while the attack is held. The hold ends when the hand slows,
tracking is lost, or `SteerMaxHoldMs` elapses.

The pure classification and steering policy lives in `src/MeleeCore.h` and is
unit-tested. The full action-to-input path is integration-tested with fake
OpenXR actions.

## 3. Grip block and bow draw

Right grip held beyond `BlockHoldTime` holds RMB for blocking. Grip taps and
gesture grabs are suppressed so reaching for equipment does not flick a block.

With `AttackHoldTrigger=1`, the right trigger owns LMB as a bow draw: hold to
draw, release to loose. LMB has a single owner. While the trigger owns it, the
swing classifier cannot inject an attack; while a swing owns it, the trigger
waits instead of double-pressing or releasing it.

## 4. Body-zone grab gestures

`GestureZones.h` classifies controller position relative to the current head
position. Defaults are intentionally simple boxes and may need per-player
tuning.

| Gesture | Default key | Companion meaning |
|---|---:|---|
| Left grip-press at left hip | F1 | equip `VR_SLOT_HIP_LEFT` |
| Right grip-press at right hip | F2 | equip `VR_SLOT_HIP_RIGHT` |
| Grip-press over shoulder | F3 | equip `VR_SLOT_SHOULDER` |
| Right grip-press at quiver | F4 | bolt/reload ritual step |
| Both grips held ~0.5 s | F5 | two-handed stance or crossbow spanning step |
| Grip released in a body zone | F6 | holster/stow hook |

All six keys are configurable in `[InputMap]`. Without the companion module,
you can bind those keys to Warband's native controls, but the precise
slot-validation behavior will not exist.

## 5. Companion module: what it actually does

The implemented source is `module_system/warbandvr_companion.py`; install
instructions are in `module_system/README.md`.

For F1, F2, F3, and non-reload F5, the companion:

1. gets the player agent;
2. checks that it is alive;
3. calls `agent_get_item_slot` to translate the configured equipment slot into
   an **item id**;
4. refuses empty slots;
5. calls `agent_equip_item` and `agent_set_wielded_item` with that item id;
6. optionally stores an attack-direction bias for the stock mouse attack path;
7. displays a short feedback message.

The important API detail is that `agent_equip_item` and
`agent_set_wielded_item` take **item ids**, not raw equipment slot numbers.
Any example that passes `2`, `3`, or another slot directly to those operations
is stale and wrong. `tools/check_companion.py` enforces the correct
`agent_get_item_slot` → item-id pattern.

## 6. Manual crossbow reload ritual

Warband reloads are timed animations, not separate mechanical engine states.
The companion therefore implements a permission ritual rather than pretending
to add physical bolt simulation:

1. A crossbow item trigger fires after a shot: ammo zeroed, reload marked
   needed, bolt/string flags cleared.
2. The quiver grab (F4) checks the configured ammo slot contains bolts and
   sets the bolt-seated flag (idempotent).
3. The both-grips gesture (F5) sets the string-drawn flag (idempotent).
4. **Either order, both required** (BannerlordVR lesson): when both flags
   are set and the wielded weapon is still a crossbow, one bolt is restored
   and the ritual resets. Steps 2 and 3 can happen in any order.

The ritual is self-healing: both gestures are idempotent, and a 1 s watchdog
resets the state if the wielded weapon stops being a crossbow mid-ritual
(weapon switched, dropped, or agent swapped) so it can never soft-lock.

On the DLL side, reaching into a grab zone is fast hand motion that would
otherwise classify as a melee swing; every zone grab/holster therefore starts
a `GrabRefractoryMs` (default 400 ms) swing-suppression window and cancels
any in-progress swing.

If no reload is in progress, F5 remains the two-handed/shoulder equipment
gesture. Reload animation timing still belongs to Warband; the ritual only
gates whether another bolt is available.

## 7. Optional melee direction assist

`VR_ENABLE_MELEE_DIRECTION_ASSIST` defaults to `0`. When enabled in the
companion source, a recent grab gesture can bias the next stock mouse attack
through `agent_set_attack_action`. This is a fallback for modules that want a
more scripted feel. The DLL's physical swing classifier remains the primary
path.

## 8. Testing path

Headless coverage already includes:

- action creation and subaction binding shape;
- stick/button/trigger transitions;
- snap-turn edge triggering;
- swing classification through the real motion-input path;
- held-attack steering and release;
- grip-block hold timing;
- zone-grab key emission;
- two-hand gesture timing;
- bow-trigger LMB ownership;
- shutdown release of all held inputs;
- companion source syntax and the item-id invariant.

On-hardware testing still needs:

1. stock boot with `EnableStereo=0`;
2. controller emulation in menus, map, movement, and combat;
3. swing-direction accuracy against dummies;
4. block behavior with shields;
5. bow draw/release timing;
6. grab gestures with the companion module installed;
7. the crossbow ritual from shot → quiver → spanning → next shot;
8. comfort tuning for swing threshold, steering scale, and snap turn.

## 9. Hard limits

- No true 6DOF weapon mesh control.
- No blade collision or physical parry simulation.
- No animation-state rewriting.
- No multiplayer use.
- No engine memory writes.

Those limits follow from Warband's closed engine and the project's safety
rules, not from missing tuning.
