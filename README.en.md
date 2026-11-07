# ra9fael.findmouse — Find My Mouse for Win顺

> **English** · [简体中文（默认）](README.md)
>
> Win顺 ships an English UI, but the default documentation is Chinese.
> `README.md` is the Chinese original; this file is its English counterpart.
> The two are kept in sync and link to each other.

Flash a locating beacon at the pointer when the mouse is **shaken quickly**.
The beacon **follows the pointer** until you press a mouse button, so a lost
cursor is impossible to miss. A `mouse` keyword covers the keyboard path:
trigger the beacon on demand, toggle shake detection, copy diagnostics.

## Purpose

Multi-monitor setups and high-DPI screens make the cursor easy to lose. Big
desktop utilities (PowerToys Find My Mouse) trigger on any click or require a
hotkey. This plugin uses a gesture you already make when hunting for the
cursor: a quick horizontal wiggle. Nothing appears until that gesture — no
click noise, no hotkey to remember, no always-visible HUD.

## Install

1. Download `ra9fael.findmouse-<version>.wsplugin` from the
   [latest release](https://github.com/ra9fael/winshun-findmouse/releases/latest).
   Optionally compare its SHA-256 with the `.sha256` file next to it
   (`Get-FileHash <file> -Algorithm SHA256`).
2. In Win顺 open **Settings → Plugins → Installed → Add plugin →
   Archive (.zip / .wsplugin)** and pick the file. In the Chinese UI the same
   path reads **设置 → 插件 → 已安装 → 添加插件 → 压缩包（.zip / .wsplugin）**.
3. A plugin installed from a local package is **not enabled automatically**.
   Turn it on in the list; enabling it is how you confirm its permissions
   (see [Permissions](#permissions-and-why)).

**Update:** install the newer `.wsplugin` the same way. Win顺 sees the same id,
asks before replacing the old version, and keeps the plugin's data folder.

**Uninstall:** remove it from **Settings → Plugins → Installed**
(**设置 → 插件 → 已安装** in the Chinese UI).

This plugin is a native exe package. Since Win顺 0.22.0 the plugin library
accepts community exe packages under the **unreviewed-native** category, so it
requires Win顺 0.22.0 or newer (declared as `minWinShun` in the manifest),
and installing it, and every update, ask for confirmation. The releases of
this repository remain the origin of the source and the package (see
[Release and provenance](#release-and-provenance)).

## Release and provenance

`v0.4.2` is the source tag for the released package. It is built from this
repository at that tag: MSVC (Visual Studio 2022, same as CI and the release
workflow) with a statically linked CRT — `findmouse.exe` needs no runtime
DLLs:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
python tools/package.py
```

`ra9fael.findmouse-<version>.wsplugin` is a deterministic zip (sorted names,
fixed timestamps) containing every git-tracked file (`plugin.json`,
`findmouse.exe`, the sources, the tests and this README), with a `.sha256`
and a `release-notes.md` (the matching CHANGELOG section) next to it.

This is a community native plugin (the package contains an exe): the plugin
library lists it as `unreviewed-native`. Installing and every update require
the user's confirmation; the public source, build command and SHA-256
document provenance but are not a code review.

## Build from source

Requirements: Windows 10/11 x64, CMake 3.21+, and either Visual Studio 2022 or
later (MSVC) or MinGW-w64 GCC 13+. Python 3.10+ for packaging and the
integration tests.

```powershell
cmake -S . -B build -A x64                 # MSVC; for MinGW use -G "MinGW Makefiles"
cmake --build build --config Release       # writes findmouse.exe next to plugin.json
ctest --test-dir build -C Release --output-on-failure
python tools/package.py                    # dist/ra9fael.findmouse-<version>.wsplugin
```

To run a local build inside Win顺 without packaging, and for tests and the
release process, see [CONTRIBUTING.md](CONTRIBUTING.md).

## Keywords and examples

| Input | Effect |
|---|---|
| `mouse` | Three rows: show beacon / toggle detection / copy diagnostics |
| `mouse 显示` | Filters to the rows whose text matches (host also matches) |
| `mouse` + **Enter** | Beacon at the pointer, then follows it until a click |
| `mouse` + **Ctrl+Enter** | Toggle shake detection (takes effect immediately, persists) |
| `mouse` + **Shift+Enter** | Copy diagnostics to the clipboard |

Actions are ordered by key as declared in `plugin.json` (`key` is display text
only; the order Enter / Ctrl+Enter / Shift+Enter is the binding). Everything
is reachable from the keyboard; no hidden keywords.

## The gesture and the beacon

**Detection** tracks **horizontal direction reversals** of the pointer with an
8 px hysteresis. A gesture is 4 reversals within 300 ms, each stroke reaching a
peak speed of at least 500 px/s (defaults); after a trigger the detector is
quiet for 1.5 s. Vertical wiggling, drags (any button held), and injected
input (remote-control tools, test harnesses) are ignored.

**The beacon** follows the pointer every frame and ends when you press the
**left or right mouse button** (polled per frame with `GetAsyncKeyState`; the
hook is not involved in ending it), or after the `followTimeoutMs` setting
(**0–60000 ms, default 0 = never auto-dismiss**). It does not vanish on the
spot: it **fades out** over `fadeOutMs` (**0–3000 ms, default 400 ms**;
quadratic decay of the whole-window opacity, still animating and following the
pointer; 0 = hide at once). Buttons are not polled during the fade. A new
trigger during follow mode just restarts the animation; a trigger during the
fade revives the beacon at full opacity. Every style keeps a constant center dot so
the exact pointer spot is never lost. Five switchable styles (the
`rippleEffect` setting):

| Style | Look |
|---|---|
| `ring` (default) | one ring expanding from 35% to full radius, fading out each cycle |
| `waves` | three staggered expanding ripples; one is always alive |
| `halo` | a breathing soft glow disc around the pointer, low alpha |
| `radar` | a rotating sonar sweep inside a static circle |
| `custom` | layers from your own JSON (see "Custom styles" below) |

Tunable parameters (all clamped into safe ranges at parse time; units in the
setting titles):

| Setting | Default | Meaning |
|---|---|---|
| `shakeStrokePx` | 8 px | min stroke distance — shorter retreats are hysteresis, not a reversal |
| `shakeMinSpeed` | 500 px/s | speed gate — strokes whose peak speed is lower are drags, not shakes; 0 disables |
| `shakeReversals` | 4 | direction changes needed |
| `shakeWindowMs` | 300 ms | time span the newest reversals must fit in |
| `shakeCooldownMs` | 1500 ms | quiet time after a trigger |
| `rippleEffect` | ring | beacon style: ring / waves / halo / radar / custom |
| `followTimeoutMs` | 0 (never) | 0–60000 ms; 0 = follow until a mouse button |
| `fadeOutMs` | 400 ms | 0–3000 ms; fade-out after a click or the timeout; 0 = instant |
| `rippleMs` | 700 ms | pulse cycle; also used by custom styles without `loopMs` |
| `rippleSize` | 64 px | radius at rest (96 dpi reference, scales with display scaling) |
| `rippleColor` | #2AA8FF | accent color; the custom JSON's `"accent"` resolves to it |
| `customStyle` | empty | custom style JSON, used when `rippleEffect` is `custom` |

Deliberate choice: the gate is **peak speed per stroke**, not acceleration.
Per-event acceleration is noise at 1000 Hz report rates; a speed floor
achieves the same intent (slow glides and drags never trigger) measurably.

## Custom styles (JSON)

Set `rippleEffect` to `custom` and paste a style document into the
`customStyle` setting. It is a **declarative** JSON: radii are structured
curve objects, never user expressions, so there is nothing to inject. Every
number is clamped and the whole document is rejected on any unknown shape,
mode or color — a parse failure keeps the previous style and shows
`custom style: parse error` in the diagnostics (Shift+Enter).

```json
{
  "loopMs": 900,
  "layers": [
    {"shape": "ring",  "from": 0.2, "to": 1.0, "ease": "outCubic",
     "width": 3.5, "offset": 0, "phase": 0,
     "alpha": {"mode": "linearFade", "value": 1.0}, "color": "accent"},
    {"shape": "ring",  "from": 0.2, "to": 1.0, "ease": "outCubic",
     "width": 1.6, "offset": 6.5, "phase": 0,
     "alpha": {"mode": "linearFade"}, "color": "#FFFFFF"},
    {"shape": "disc",  "radius": 0.05, "alpha": 1.0, "color": "accent"},
    {"shape": "sweep", "speed": 1.0, "trailDeg": 120, "fill": true,
     "alpha": {"mode": "constant", "value": 0.85}, "color": "accent"}
  ]
}
```

| Field | Meaning |
|---|---|
| `loopMs` | 200–5000 ms cycle length; missing → the `rippleMs` setting |
| `layers` | 1–12 layers, composited strongest-alpha-wins |
| `shape` | `ring` / `disc` / `sweep` |
| `from` / `to` + `ease` | animated radius over the cycle; `radius` for a static one |
| radius unit | fraction of the `rippleSize` setting (1.0 = full radius) |
| `width` / `offset` | px at 96 dpi: ring half-width / outward offset (outlines!) |
| `phase` | 0–1 shift of the layer's cycle (0, ⅓, ⅔ → continuous waves) |
| `alpha` | `{"mode": "constant"/"linearFade"/"holdFade"/"sin", "value": 0–1}` or a bare number (= constant) |
| `color` | `"accent"` (follows `rippleColor`) or `"#RRGGBB"` |
| `speed` / `trailDeg` / `fill` | sweep only: turns per loop (negative = counter-clockwise), trail angle 1–360, faint disc underneath |

Copy-paste starting points:

* **Ring**: the first two layers above (accent ring + white outline) — the
  built-in ring look.
* **Waves**: three ring layers, identical except `"phase": 0 / 0.333 / 0.667`.
* **Halo**: `{"shape": "disc", "from": 0.0, "to": 0.95, "ease": "sin", "alpha":
  {"mode": "sin", "value": 0.3}}` plus a static rim ring at `radius` 1.0.
* **Radar**: static ring at radius 1.0 + a `sweep` layer with `trailDeg` 120.

## Runtime dependencies

- Win顺 0.22.0 or newer (`minWinShun`): this is a community native package
  (`exe`), and the plugin library lists it as `unreviewed-native`; older
  clients have no install-risk confirmation for native packages.
- Windows 10 1809+ / Windows 11, x64 (the build refuses 32-bit).
- Ships as a statically linked `findmouse.exe`; no runtime libraries, no
  Python, no network.
- Resident plugin (`mode: resident`), `start: eager`, `idleExitSec: 0` — it
  must be alive while the launcher is closed, so it never idles out.
- Build: `cmake -B build && cmake --build build --config Release`. The exe is
  produced in this folder, where `plugin.json` names it.

## Permissions and why

- **`ui`** — the only permission. It lets the plugin observe Win顺's theme
  and language, draw a topmost window (the ripple), and register the global
  mouse hook. The hook itself needs no named permission: Win顺 has no
  `input:read` concept, and this is declared here instead of hidden.
- **No `elevated`** — the hook sees input on administrator windows only as
  far as UIPI allows; the ripple may not draw over an elevated fullscreen app.
  This is a Windows integrity-level fact, not a plugin bug.
- **No `network`, no `clipboard` (plugin-side), no `files`** — the plugin
  makes no network calls, reads no clipboard, and touches no user files.
  Clipboard *effects* (`copy`) are requested through the action protocol;
  the host performs them.
- **`sandbox: normal`, not `strict`** — a low-integrity process installing a
  global `WH_MOUSE_LL` hook is an unverified configuration (whether the hook
  receives events at low IL is undocumented). The plugin needs no strict-sandbox
  guarantees: it writes nothing outside its data dir, so `normal` is honest,
  not lazy.

## Data location

One file: `state.json` inside the plugin's data directory (`WS_DATA_DIR`,
provided by the host at initialize). It stores the last user toggle:

```json
{"shakeEnabled": true, "version": 1}
```

- Every explicit choice persists immediately: the Ctrl+Enter toggle writes it,
  and changing the **Enable shake detection** setting from Win顺's settings
  UI overwrites it too. Last explicit action wins; `state.json` (if present)
  seeds the value on plugin start.
- No logs, no caches, no secrets. Removing the file is safe.

## Failure handling

- Hook install failure (policy or resource): logged via `winshun/log`, the
  result rows say "hook not running", and the diagnostics still work.
- Overlay failure: the hook keeps running; the next trigger retries.
- Settings errors: every numeric setting is clamped; a bad color keeps the
  default accent. A broken `state.json` is ignored (falls back to defaults).
  A `customStyle` that fails to parse keeps the previous style (ring if there
  never was one) and says so in the diagnostics — the beacon never blanks.
- The detector never consumes input: the hook proc always ends in
  `CallNextHookEx`, and injected events pass through untouched.

## Testing

- `findmouse_tests` (ctest): pure gesture state machine (fast shake, slow
  movement, sub-stroke jitter, drift, cooldown, config swaps) and settings
  parsing/clamping/color.
- `tests/integration.py`: drives the real exe over its JSON-RPC stdio —
  initialize → query → actions → settingsChanged → shutdown — asserting
  protocol shape, side-effect rules (queries have none), and state persistence.

## License

MIT, see [LICENSE](LICENSE). Release history: [CHANGELOG.md](CHANGELOG.md).

## Honest notes

- The physical-gesture false-positive rate (trackpad palm sweeps, games,
  fast text selection) is **unverified** until used daily; the counters in
  the diagnostics (moves / reversals / slow strokes / triggers) exist exactly
  to tune the thresholds.
- Whether the beacon is visible over **exclusive fullscreen** or
  **administrator** windows depends on Windows integrity levels and is not
  guaranteed (see Permissions).
- The beacon's DPI snapshot is taken at the trigger point; moving to a monitor
  with different scaling mid-follow keeps the old size until the next trigger.
- v0.2 ends the beacon on any left/right click **anywhere** (the window is
  click-through, so the click lands on whatever is under the pointer).
- v0.1 deliberately skipped host hotkeys (`winshun/hotkeys`): the local host
  build does not expose `hostHotkeys` capability negotiation yet, so the
  keyboard path uses ordinary query actions instead.
---

[← 简体中文（默认）](README.md) · English
