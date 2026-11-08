# v0.4.4 release validation

[简体中文](release-validation-v0.4.4.md)

Date: 2026-11-08. The user requested Heartbeat as the default, commits, a tag and a public release.

## Build regression

Windows x64 build passed with Visual Studio 18 and Windows SDK 10.0.26100.0.
CTest passed 1/1 (178 assertions); independent JSON-RPC integration passed 77/77.
A new assertion verifies that omitting `rippleEffect` selects the three-layer Heartbeat document at 1400 ms.

## Changes in this pass

Both manifest and `Settings` defaults are `heart`, with matching dropdown labels, READMEs and release notes.
Explicit saved choices remain supported. Unknown ids and invalid custom JSON without a valid history still use Ring.
Includes the preceding preset, startup-readiness, custom-fallback isolation and sweep-continuity fixes.

## Production rendering and screenshots

Captured Ring, Heartbeat, Ping and Orbit from the actual exe on an isolated backdrop:
[Effect screenshots](../screenshots.en.md).
A real `wsbench` → `WinShunPluginHost` round trip confirmed default `effect=heart`, `preset (heart)`
and `3 layers, loop=1400ms`. The medium-integrity plugin completed queries and diagnostics.

## Plugin index

Pre-release inspection found no entry or pending submission for this plugin in `LingCore/WinShun-plugins`.
This is an author-repository release, installed from its GitHub Release archive; there is no existing index entry to update.

## Package SHA256

The release workflow builds the clean tagged commit, runs CTest and generates a `.wsplugin` plus `.sha256`.
Post-publication download, checksum, manifest and real-host results are recorded in workspace
`WinShun/docs/validation/findmouse-release-v0.4.4/`. This tagged document records local pre-release checks.

## Public release verification

Push main, wait for CI success, push v0.4.4, wait for the release workflow and verify the public release,
assets, download checksum and tag commit. Do not report completion if verification fails.

## Not tested

Physical gesture feel, everyday false positives, mixed-DPI monitor transitions, exclusive fullscreen,
administrator-window behavior and Settings-page interactions were not retested. Static frames do not establish
full animation smoothness. The user's running installed plugin was not replaced.

[简体中文](release-validation-v0.4.4.md)
