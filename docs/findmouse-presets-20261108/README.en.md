# findmouse preset completion validation

[简体中文](README.md)

Implementation: `ra9fael.findmouse` workspace version 0.4.4, 2026-11-08.

## Problem

Continue the unfinished Ping, Heart, Orbit and gesture-threshold work. Code review also found that startup could drop the first overlay style message, the reverse-speed Orbit sweep jumped at a cycle boundary, and invalid custom JSON could reuse a preset document.

## Why

`start()` returned before the overlay thread had created its window message queue, so the following `updateStyle()` post could fail. The generic renderer received only modulo pulse progress, so reverse-speed Orbit returned to its starting angle each cycle. A shared document pointer could not distinguish a user fallback from an active preset.

## Changes

- Added validated Ping, Heart and Orbit registry documents, with matching manifest, runtime enums and dropdown options.
- Overlay startup waits for the window and message queue; failed style posts and queued messages at shutdown are cleaned up.
- Presets and the last valid user-authored document are stored separately; clearing custom JSON clears the user fallback.
- Orbit uses accumulated cycle time and fixes reverse trails; registry lookup accepts mixed-case ids.
- Added manifest/L1, JSON-RPC/L2, real-host/L3 regressions and bilingual design documentation.

## Measured

Environment: Windows x64, Visual Studio 18.11.1, Windows SDK 10.0.26100.0. Tests used isolated temporary data and did not modify the live install.

- `cmake --build _msvc/findmouse --config Release`: passed; [build.txt](build.txt).
- `ctest --test-dir _msvc/findmouse -C Release --output-on-failure`: 1/1 passed; direct run with the manifest argument reported **178 passed, 0 failed**; [ctest.txt](ctest.txt).
- `python tests/integration.py`: **76 passed, 0 failed**, covering protocol, side-effect-free queries, follow/fade behavior, preset startup, live switching and custom fallback; [integration.txt](integration.txt).
- `wsbench --plugin ... --setting rippleEffect=ping/heart/orbit --action copyDiagnostics`: real `WinShunPluginHost.exe` loaded all three; diagnostics reported 3 layers and 1100/1400/1600 ms; [host-ping.txt](host-ping.txt), [host-heart.txt](host-heart.txt), [host-orbit.txt](host-orbit.txt).
- The same host completed `showNow` for each preset with the expected hide effect: [host-show-ping.txt](host-show-ping.txt), [host-show-heart.txt](host-show-heart.txt), [host-show-orbit.txt](host-show-orbit.txt). Automated window visibility and timeout assertions are covered by L2.
- The 22-file local development package passed validation with **0 errors and 1 unreviewed-native notice**. Required sources are present and its executable matches the tested binary: [package.json](package.json). Orbit also passed a real-host round trip from the extracted package: [host-package.txt](host-package.txt).
- `_msvc/validate_docs.py`: README links, anchors and language switches passed.

Package: `ra9fael.findmouse/dist/presets-20261108/ra9fael.findmouse-0.4.4.wsplugin`.
SHA256: `fe036f7227e4739960ad7ed8d1c596923dcee4c52dfc9dd831e8ce905b4ad6b9`.
This is a local development snapshot. A temporary Git index included new uncommitted sources in the existing packager without changing the real staging area.

An intermediate build caught one stale variable name. The first startup regression incorrectly treated the SDK's initialize response as worker readiness; using the subsequent diagnostic request as a protocol barrier resolved that test error. Only passing results were used for delivery.

## Not tested

Physical gesture feel and false-positive rates, mixed-DPI monitor transitions, exclusive fullscreen, visibility over administrator windows and a real Settings-page dropdown capture were not automated this round. `wsbench` proves a real host round trip, not full Settings-page acceptance.

## Cleanup

Reused the existing `_msvc/findmouse` build directory. Integration state, host profiles, the temporary package index and extracted package were cleaned up. Retained the development package and selected evidence. No live user plugin was replaced and no release was published; source edits remain uncommitted.

[简体中文](README.md)
