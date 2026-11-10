# findmouse v0.4.4 publication record

[简体中文](README.md)

## Problem

The user requested Heartbeat as the default, commits, a version tag and a public release.
The latest remote release before this pass was v0.4.3.

## Why

Both the manifest and runtime defaults were Ring. Changing only the dropdown would not cover
initialization without settings, so the manifest, `Settings::effect`, labels and default-path checks changed together.

## Changes

Default to three-layer Heartbeat at 1400 ms, preserving explicitly saved choices. Include the preceding
Ping/Heart/Orbit, startup-readiness, isolated custom fallback and continuous reverse-sweep changes.
Updated bilingual READMEs, design and release documentation, with four native-effect screenshots.

## Measured

- MSVC x64 build, CTest 1/1 and 178 assertions passed: [build.txt](build.txt), [ctest.txt](ctest.txt), [unit.txt](unit.txt).
- Local integration passed 77/77, including omitted-effect initialization: [integration.txt](integration.txt).
- Real host confirmed default Heartbeat, three layers and 1400 ms: [host-default.txt](host-default.txt).
- Native captures came from the real exe on an isolated backdrop: [captures.json](captures.json), [capture_effects.py](capture_effects.py).
  Initial cursor-position capture attempts failed; final captures used the actual overlay bounds within the backdrop.
- Feature commit `2b95d8d`, release-doc commit `c137e32`, integrated main commit **43dd1be80c08bf9edc305ac7b6baba914d8adc98**.
- Main CI **42477493842** passed before v0.4.4 was created and pushed: [ci.json](ci.json).
- Release workflow **42477583397** passed: [release-run.json](release-run.json).
- Published **2026-11-08 19:52:39 Hong Kong time**, public, stable and latest: [release.json](release.json).
- Downloaded the actual public asset: 31 files, zero validation errors, one expected unreviewed-native notice.
  Checksum sidecar and GitHub asset digest match; manifest defaults to heart. The downloaded binary passed
  the real host and **77/77** integration assertions: [published-verification.json](published-verification.json),
  [published-host.txt](published-host.txt), [published-integration.txt](published-integration.txt).

Public release: <https://github.com/ra9fael/winshun-findmouse/releases/tag/v0.4.4>

Package: `ra9fael.findmouse-0.4.4.wsplugin`, 769608 bytes.
SHA256: `6cdae7c324a985e89fd4c99756566817020b30144c741b75f16cda130c0414a9`.
Downloaded copy: `ra9fael.findmouse/dist/release-v0.4.4-public/`.

Remote main and the dereferenced tag match the commit above. The plugin working tree is clean.
There is no existing community-index entry or pending submission to update. The live installed plugin
was not replaced. Test processes, temporary profiles and extracted package directories were cleaned up;
the local and public packages plus original validation records remain. This directory is local workspace evidence,
not a commit to the host repository.

## Not tested

Physical gesture feel and false positives, mixed-DPI transitions, exclusive fullscreen, elevated windows
and the Settings page were not retested. Static frames do not establish animation feel. Saved Ring settings
are not forcibly migrated to Heartbeat.

[简体中文](README.md)
