# Completing the effect presets

[简体中文](presets-design.md)

Date: 2026-11-08. Scope: continue the existing Ping, Heart, Orbit and gesture-threshold work.

## 1. Verdict

The three presets share the declarative renderer. Keep the last valid user document separate
from the active preset so changing effects never changes the custom-error fallback.

## 2. Grounded facts

Code inspection found existing uncommitted enum values, manifest options and preset documents.
Overlay start returned before its message queue existed, allowing initial settings to be lost.
The -0.6-speed Orbit sweep wrapped its input progress every cycle, causing a discontinuity.
These are code findings; measured validation is recorded separately.

## 3. Architecture

Retain the protocol, mouse-hook and overlay threads. Wait for the overlay window and queue
before returning from start; return failure if window creation fails. Pass validated immutable
preset documents through existing style messages. Release their memory on failed posts and shutdown.
Keep existing settings keys and the four built-in effects, with no new dependencies.

## 4. Core logic

Retain the draft defaults of 80 px, six reversals, 400 ms and 1000 px/s.
Synthetic gesture tests cannot establish real-user false-positive rates.
Radius and opacity use periodic progress; sweep angles use elapsed cycles, with reverse trails
following behind the direction of motion.

## 5. Interaction

The dropdown has eight choices: ring, waves, halo, radar, ping, heart, orbit and custom.
Ping/Heart/Orbit use 1100/1400/1600 ms cycles. Heartbeat is the default; saved choices survive upgrades.
Existing command and action ids remain supported.
Diagnostics show the selected preset, active layer count and cycle, and distinguish keeping a
user document from falling back to Ring. Comet, Spotlight, Pin and other preview concepts are
not available plugin effects.

## 6. Permissions, data and privacy

Continue using the ui permission. Presets are compiled data with no network or additional file access.
Tests use isolated data, close their processes and remove temporary state. The live plugin and
user configuration are not modified.

## 7. Failure matrix

| Condition | Handling |
|---|---|
| Invalid custom JSON | Keep the last valid user document, otherwise Ring |
| Empty custom JSON | Clear the user document and use Ring |
| Invalid custom JSON after switching through another effect | Still use the user fallback, never a preset |
| Invalid preset document | Log a warning and render Ring |
| Unknown or planned effect id | Select Ring |
| Overlay window creation failure | Return failure; preserve the manual retry path |
| Failed or unprocessed style message at shutdown | Release its heap payload |

## 8. Deliverables

Implementation, regressions, bilingual READMEs, v0.4.4 notes, this design, native screenshots and validation evidence.
The user approved Heartbeat as the default and publication of v0.4.4. Push the tag after main CI passes;
the release workflow builds and publishes the package.

## 9. Layered tests

| Level | Coverage |
|---|---|
| L0 | Manifest choices equal implemented effects, bilingual labels, defaults and package completeness |
| L1 | Registry lookup, preset parsing, simulated gestures, fractional sweep continuity and reverse trails |
| L2 | JSON-RPC startup, switching, custom fallback, pure queries, real overlay visibility and timeout |
| L3 | Actual wsbench + WinShunPluginHost queries, diagnostics and showNow for every preset |
| L4 | Physical gestures, multiple monitors, mixed DPI and elevated windows require manual retesting |
| L5 | Build from a clean commit and verify remote assets, checksums and tag; no real-user calibration claims |

## 10. Honest notes

Code inference does not replace desktop measurement; synthetic trajectories do not measure everyday
false positives. The design preview is not a capture of the native renderer.

## 11. Delivery report

Development evidence lives at workspace `docs/validation/findmouse-presets-20261108/README.en.md`.
See [v0.4.4 validation](release-validation-v0.4.4.en.md) for the default-Heartbeat release checks.

[简体中文](presets-design.md)
