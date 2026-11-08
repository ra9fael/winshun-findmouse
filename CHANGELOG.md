# Changelog

[English](CHANGELOG.en.md)

All notable changes to this plugin. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/). The version in `plugin.json` is the
single source of truth; release tags are `v<version>`.

## [Unreleased]

## [0.4.5] - 2026-11-09

### Fixed

- 修复可执行文件图标，补齐打包中的 ICO 和资源脚本。

### Notes

- 本次仅生成本地标签与安装包。实际验证范围见 docs/icon-validation.md；未进行 GitHub 发布。更新后需重新启动插件进程。


## [0.4.4] - 2026-11-08

### Added
- Ping, Heartbeat and Orbit presets in the effect dropdown, using validated
  declarative layers with 1100/1400/1600 ms cycles. Diagnostics show the
  active document's layer count and cycle.
- Manifest/registry consistency checks and protocol regressions for preset
  startup, live switching and custom fallback across effects.

### Fixed
- Initial overlay settings could be dropped before the worker's window and
  message queue existed. Startup now waits for readiness; queued style
  payloads are released on failed delivery and shutdown.
- Invalid custom JSON after a preset could reuse that preset, while switching
  through a built-in lost the previous user fallback. The last valid custom
  document is now independent of the active preset; clearing JSON clears it.
- Fractional-speed sweeps no longer jump at cycle boundaries; negative-speed
  trails follow the rotation. Registry lookup accepts mixed-case ids.

### 中文说明
- 新增声呐、心跳、轨道三种可选预设；诊断显示有效图层和周期。
- 默认效果改为心跳（1400 ms）；升级保留用户已保存的效果选择。
- 修复初始设置投递过早、自定义样式回退被预设覆盖、扫描跨周期跳回和反向尾迹方向。
- 补齐 manifest 一致性、预设启动、动态切换和自定义回退测试。
- 默认晃动阈值为 400 ms 内 6 次换向、80 px 行程、1000 px/s 速度门槛；真人误触发率仍需实测。

### Changed
- Heartbeat is now the default effect (1400 ms cycle). Saved effect choices
  are preserved on upgrade; Ring remains the fallback for invalid effect ids
  or invalid custom JSON without a previously valid document.
- The shake thresholds default to the revised values: 6 reversals inside a
  400 ms window (was 4 in 300 ms), a 1000 px/s speed gate (was 500) and an
  80 px stroke (was 8). Synthetic trajectories exercise recognition and
  rejection; real-user gesture feel and false-positive rates are not measured.
  An earlier untagged draft used a 600 px stroke; the working default is 80 px.
- `shakeStrokePx` accepts up to 600 px (was 200), so a deliberately wide
  personal tuning still fits its own range.

### Known limitations
- Physical gesture feel and false-positive rates, mixed-DPI monitor transitions,
  exclusive fullscreen and elevated-window behavior were not revalidated this release.
- Comet, Spotlight and Pin remain design concepts, not selectable effects.
- 真人手势与误触发率、跨屏 DPI、独占全屏和提权窗口需进一步人工验证；彗尾、聚光灯和图钉尚未实现。

## [0.4.3] - 2026-11-07

### Fixed
- The beacon could never be dismissed when Win顺 ran as administrator: the
  host de-elevates the plugin to medium integrity, and Windows UIPI hides
  input aimed at elevated windows — with the focus on an elevated window
  neither the mouse hook nor `GetAsyncKeyState` can see anything, so shakes
  never triggered and clicks never dismissed. The plugin now probes the host
  chain's integrity once at startup: when elevated, the query results grow a
  notice row ("Win顺 提权中…", Enter for details) and the diagnostics report
  `integrity: high` with the host process name.

### Changed
- `followTimeoutMs` defaults to 3000 ms (was 0 = never). With click-to-dismiss
  unavailable on elevated windows, "never auto-dismiss" left the beacon stuck
  forever; a timeout backstop guarantees it always goes away. Set it back to 0
  for the old follow-until-click behavior.
- Diagnostics report the host process and its integrity level
  (`host process: …, integrity: medium/high/unknown`).

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

[Unreleased]: https://github.com/ra9fael/winshun-findmouse/compare/v0.4.4...HEAD
[0.4.4]: https://github.com/ra9fael/winshun-findmouse/compare/v0.4.3...v0.4.4
[0.4.3]: https://github.com/ra9fael/winshun-findmouse/compare/v0.4.2...v0.4.3
[0.4.2]: https://github.com/ra9fael/winshun-findmouse/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/ra9fael/winshun-findmouse/releases/tag/v0.4.1
