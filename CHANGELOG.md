# Changelog

All notable changes to this plugin. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/). The version in `plugin.json` is the
single source of truth; release tags are `v<version>`.

## [Unreleased]

## [0.4.2] - 2026-11-07

### Added
- `minWinShun: 0.22.0` and `homepage` in the manifest. `minWinShun` is required
  for a community native (exe) package: the plugin library lists it as
  `unreviewed-native`, and clients older than 0.22.0 have no install-risk
  confirmation for native packages, so they must not enable it.

### Changed
- The SDK header (`winshun.h`) is refreshed from WinShun 0.22's
  `src/plugins-sdk/winshun.h` (details support for #202, theme/style
  appearance, the plugin-host `serve(HANDLE, HANDLE)` variant). No code
  change was needed; the plugin does not use any of the new methods yet.
- The plugin library's community index now accepts native exe packages under
  the "unreviewed-native" category, so the README no longer claims the
  in-app library is script-only; the GitHub Releases stay the source of
  truth. No runtime behavior change.
- `README.md` is now the Chinese (default) documentation; `README.en.md` holds
  the full English text. Both link to each other. The English install steps now
  use Win顺's real English UI labels. No runtime change.

## [0.4.1] - 2026-11-05

First public release. Versions 0.1.0 to 0.4.0 were developed locally and never
published; they are listed below for context.

### Added
- `fadeOutMs` setting (0-3000 ms, default 400): the beacon fades out
  (quadratic decay of the window opacity) instead of disappearing at once.
  0 restores the instant hide.
- A trigger during the fade brings the beacon back at full opacity and restarts
  the auto-dismiss timer.

### Changed
- Plugin id is now `ra9fael.findmouse` (was `renfei.findmouse`) and the author
  is `ra9fael`. WinShun treats this as a new plugin, so settings from a local
  `renfei.findmouse` install do not carry over.

## [0.4.0] - 2026-11-05 (not published)

### Added
- `followTimeoutMs` auto-dismiss (0-60000 ms, default 0 = never).
- Custom effects: a declarative JSON style (`rippleEffect` = custom +
  `customStyle`) with ring / disc / sweep layers, easing curves and alpha modes.
- Units in every numeric setting title and hint.

### Removed
- The fixed 30 s follow cap (replaced by `followTimeoutMs`).

## [0.3.0] - 2026-11-05 (not published)

### Added
- Four built-in effects selectable with `rippleEffect`: ring, waves, halo, radar.

## [0.2.0] - 2026-11-05 (not published)

### Added
- `shakeMinSpeed` (px/s) gate and the existing stroke length as settings.
- The beacon follows the pointer until a left or right mouse button is pressed.

## [0.1.0] - 2026-11-05 (not published)

### Added
- Shake detection through a low-level mouse hook, a click-through layered
  overlay, and the `mouse` / `找鼠标` keyword (show beacon, toggle detection,
  copy diagnostics).

[Unreleased]: https://github.com/ra9fael/winshun-findmouse/compare/v0.4.2...HEAD
[0.4.2]: https://github.com/ra9fael/winshun-findmouse/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/ra9fael/winshun-findmouse/releases/tag/v0.4.1
