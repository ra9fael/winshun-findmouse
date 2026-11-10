"""Capture the real plugin over a solid backdrop using existing screenshot tools.

Run from the workspace with the bundled Python (Pillow/numpy required).
Only the test overlay and its own backdrop are captured. Does not move the cursor.
"""
import ctypes
import hashlib
import json
import sys
import tempfile
import time
from ctypes import wintypes
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
REPO = ROOT / "ra9fael.findmouse"
sys.path.insert(0, str(ROOT / "WinShun/tools/screenshots"))
sys.path.insert(0, str(REPO / "tests"))
from capture import Desktop, Backdrop
from integration import Driver, ripple_windows_of_pid
from PIL import ImageGrab


def main():
    desktop = Desktop()
    old = wintypes.POINT()
    desktop.user32.GetCursorPos(ctypes.byref(old))
    x, y = old.x, old.y
    rect = (x - 350, y - 350, x + 350, y + 350)
    desktop.user32.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    output = REPO / "docs/images"
    output.mkdir(parents=True, exist_ok=True)
    reports = []
    scratch = ROOT / "build/findmouse-tasks"
    with tempfile.TemporaryDirectory(prefix="capture-", dir=scratch) as data:
        driver = Driver(data)
        backdrop = None
        try:
            driver.initialize({"shakeEnabled": False, "followTimeoutMs": 0})
            diagnostic = driver.diagnostics()
            assert "effect=heart" in diagnostic and "preset (heart)" in diagnostic
            hwnd, = ripple_windows_of_pid(driver.proc.pid)
            backdrop = Backdrop(desktop, rect, [hwnd])
            backdrop.fill(0x191412)  # COLORREF; RGB 18,20,25
            for effect, delay in (("ring", 0.20), ("heart", 0.25), ("ping", 0.20), ("orbit", 0.40)):
                # Omit rippleEffect for heart to exercise the runtime default.
                settings = {"shakeEnabled": False, "followTimeoutMs": 0}
                if effect != "heart":
                    settings["rippleEffect"] = effect
                driver.settings_changed(settings)
                diagnostic = driver.diagnostics()
                assert f"effect={effect}" in diagnostic
                driver.action("showNow")
                deadline = time.monotonic() + delay
                while time.monotonic() < deadline:
                    desktop.pump()
                    time.sleep(0.01)
                desktop.settle()
                assert desktop.user32.IsWindowVisible(hwnd)
                window = wintypes.RECT()
                desktop.user32.GetWindowRect(hwnd, ctypes.byref(window))
                crop = (window.left-8, window.top-8, window.right+8, window.bottom+8)
                assert crop[0] >= rect[0] and crop[1] >= rect[1] and crop[2] <= rect[2] and crop[3] <= rect[3], "Overlay left the isolated backdrop"
                picture = ImageGrab.grab(bbox=crop, include_layered_windows=True, all_screens=True)
                assert picture.getpixel((0, 0))[:3] == (18, 20, 25), "Backdrop not covering the capture"
                assert len(picture.getcolors(102400) or []) > 20, "Blank overlay"
                name = f"{effect}-v0.4.4.png"
                picture.save(output / name, optimize=True)
                reports.append({"effect": effect, "file": name, "delay_seconds": delay,
                                "sha256": hashlib.sha256((output/name).read_bytes()).hexdigest(),
                                "diagnostics": diagnostic})
        finally:
            driver.close()
            if backdrop:
                backdrop.close()
    record = {"source": "real findmouse.exe layered window on an isolated solid backdrop",
              "exe_sha256": hashlib.sha256((REPO / "findmouse.exe").read_bytes()).hexdigest(),
              "frames": reports, "cursor_moved": False}
    Path(__file__).with_name("captures.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print("Captured Ring, default Heartbeat, Ping and Orbit from the real overlay.")


if __name__ == "__main__":
    main()
