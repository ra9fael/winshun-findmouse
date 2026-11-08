#!/usr/bin/env python3
# integration.py — drives the built findmouse.exe over its JSON-RPC line
# protocol and verifies real behaviour:
#   * initialize/query/action/settingsChanged/shutdown round-trips
#   * queries are side-effect-free (no state.json write, no window shown)
#   * toggleShake persists to state.json and takes effect
#   * copyDiagnostics returns real counters and the applied config
#   * settingsChanged is applied (visible in the next diagnostics copy)
#   * the ripple window exists, is layered + click-through + topmost, and
#     actually shows on showNow (checked via Win32 from this process)
#   * garbage lines and unknown methods do not kill the process
#
# The follow check briefly moves and restores the cursor. Physical gestures
# still need manual verification; no clicks or keystrokes are injected.

import json
import os
import subprocess
import sys
import tempfile
import threading
import time
import ctypes
from pathlib import Path
from ctypes import wintypes

HERE = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(HERE, "..", "findmouse.exe")

passed = 0
failed = 0


def check(cond, label, extra=""):
    global passed, failed
    if cond:
        passed += 1
        print(f"  PASS  {label}")
    else:
        failed += 1
        print(f"  FAIL  {label}  {extra}")


def find_ripple_window():
    return ctypes.windll.user32.FindWindowW("WinShunFindMouseRipple", None)


def ripple_windows_of_pid(pid):
    """All visible-top-level windows of class WinShunFindMouseRipple whose
    process is `pid`. FindWindow alone can hit another live copy (e.g. the
    WinShun-hosted plugin), so every check is scoped to our process."""
    out = []
    user32 = ctypes.windll.user32
    WNDENUM = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)

    def on_window(hwnd, _lparam):
        buf = ctypes.create_unicode_buffer(64)
        user32.GetClassNameW(hwnd, buf, 64)
        if buf.value == "WinShunFindMouseRipple":
            owner = wintypes.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
            if owner.value == pid:
                out.append(hwnd)
        return True

    user32.EnumWindows(WNDENUM(on_window), 0)
    return out


def window_styles(hwnd):
    # GWL_EXSTYLE == -20; 64-bit Python on a 64-bit exe: GetWindowLongPtrW
    user32 = ctypes.windll.user32
    if hasattr(user32, "GetWindowLongPtrW"):
        return user32.GetWindowLongPtrW(hwnd, -20)
    return user32.GetWindowLongW(hwnd, -20)


class Driver:
    def __init__(self, data_dir):
        self.proc = subprocess.Popen(
            [EXE], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE, text=True, bufsize=1)
        self._id = 0
        self._buf = []
        self._lock = threading.Lock()
        self._reader = threading.Thread(target=self._pump, daemon=True)
        self._reader.start()
        self.data_dir = data_dir
        self.stderr_tail = []

    def _pump(self):
        for line in self.proc.stdout:
            with self._lock:
                self._buf.append(line)

    def _send(self, msg):
        self.proc.stdin.write(json.dumps(msg) + "\n")
        self.proc.stdin.flush()

    def recv(self, timeout=5.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            with self._lock:
                if self._buf:
                    return json.loads(self._buf.pop(0))
            time.sleep(0.01)
        return None

    def request(self, method, params, timeout=5.0):
        self._id += 1
        rid = self._id
        self._send({"jsonrpc": "2.0", "id": rid, "method": method, "params": params})
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msg = self.recv(max(0, deadline - time.monotonic()))
            if msg is None:
                return None
            if msg.get("id") == rid:
                return msg
            # notifications (winshun/log ...) are collected, not dropped
            self.stderr_tail.append(msg)
        return None

    def settings_changed(self, settings):
        self._send({"jsonrpc": "2.0", "method": "settingsChanged",
                    "params": {"settings": settings}})

    def diagnostics(self):
        response = self.action("copyDiagnostics") or {}
        return next((e.get("text", "") for e in response.get("result", {}).get("effects", [])
                     if e.get("type") == "copy"), "")

    def initialize(self, settings=None, locale="en", theme="light"):
        return self.request("initialize", {
            "settings": settings or {},
            "locale": locale,
            "theme": theme,
            "appearance": {},
            "pluginDir": os.path.dirname(EXE),
            "dataDir": self.data_dir,
        })

    def query(self, text="", query_id=1):
        return self.request("query", {
            "queryId": query_id, "command": "mouse", "trigger": "keyword",
            "keyword": "mouse", "text": text, "raw": "mouse " + text,
            "match": [], "limit": 200,
        })

    def action(self, action_id, item=None):
        return self.request("action", {
            "command": "mouse", "action": action_id,
            "text": "", "item": item or {}, "trigger": "keyword", "files": [],
        })

    def close(self):
        if self.proc.poll() is None:
            try:
                self._send({"jsonrpc": "2.0", "id": 99999, "method": "shutdown", "params": {}})
            except OSError:
                pass
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(timeout=5)
        self._reader.join(timeout=2)
        for pipe in (self.proc.stdin, self.proc.stdout, self.proc.stderr):
            if pipe:
                pipe.close()


def state_content(data_dir):
    p = os.path.join(data_dir, "state.json")
    if not os.path.exists(p):
        return None
    with open(p, "r", encoding="utf-8") as f:
        return f.read()


def run(drv, data_dir):
    print("== initialize ==")
    init = drv.initialize({"followTimeoutMs": 0})
    check(init is not None and init.get("result", {}).get("protocolVersion") == "1",
          "initialize answers protocolVersion 1", str(init))
    check((init or {}).get("result", {}).get("name") == "ra9fael.findmouse",
          "initialize answers the plugin name", str(init))
    time.sleep(0.3)  # give the worker threads a beat

    wins = ripple_windows_of_pid(drv.proc.pid)
    check(len(wins) == 1, "exactly one ripple window in our process", str(wins))
    hwnd = wins[0] if wins else 0
    check(hwnd != 0, "ripple window exists after initialize")
    if hwnd:
        ex = window_styles(hwnd)
        WS_EX_LAYERED, WS_EX_TRANSPARENT, WS_EX_TOPMOST = 0x80000, 0x20, 0x8
        check(bool(ex & WS_EX_LAYERED), "ripple window is layered (per-pixel alpha)")
        check(bool(ex & WS_EX_TRANSPARENT), "ripple window is click-through")
        check(bool(ex & WS_EX_TOPMOST), "ripple window is topmost")

    print("== query: read-only, three rows ==")
    before = state_content(data_dir)
    resp = drv.query("")
    items = (resp or {}).get("result", {}).get("items", [])
    check(len(items) == 3, "empty query returns 3 rows", str(len(items)))
    ids = [i.get("id") for i in items]
    check(ids == ["showNow", "toggleShake", "copyDiagnostics"],
          "row ids are stable", str(ids))
    check(all(i.get("title") and i.get("subtitle") for i in items),
          "all rows carry title and subtitle")
    check(all(i.get("icon", "").startswith("glyph:") for i in items),
          "rows use glyph icons (no binary assets needed)")
    time.sleep(0.2)
    check(state_content(data_dir) == before, "query wrote no state file")
    check(not ctypes.windll.user32.IsWindowVisible(hwnd), "query did not show the beacon")

    print("== query: text filters rows ==")
    resp = drv.query("diag")
    items = (resp or {}).get("result", {}).get("items", [])
    check([i.get("id") for i in items] == ["copyDiagnostics"],
          "query 'diag' filters to the diagnostics row", str(items))
    resp = drv.query("zzz-no-match")
    items = (resp or {}).get("result", {}).get("items", [])
    check(items == [], "no-match query returns an empty list honestly", str(items))

    print("== action: showNow ==")
    resp = drv.action("showNow")
    effects = (resp or {}).get("result", {}).get("effects", [])
    check(any(e.get("type") == "hide" for e in effects),
          "showNow hides the launcher", str(effects))
    shown = False
    deadline = time.time() + 1.5
    while time.time() < deadline:
        if hwnd and ctypes.windll.user32.IsWindowVisible(hwnd):
            shown = True
            break
        time.sleep(0.05)
    check(shown, "beacon window actually becomes visible on showNow")

    print("== follow mode: the beacon tracks the cursor ==")
    # Move the real cursor a little and verify the window re-centers on it
    # (follow mode), then restore the cursor. No input injection beyond this
    # controlled move. Retry: whoever is using the desktop moves the real
    # mouse too, so one attempt can be dragged off target; up to 3 tries.
    class POINT(ctypes.Structure):
        _fields_ = [("x", wintypes.LONG), ("y", wintypes.LONG)]

    tracked = False
    detail = ""
    pt0 = POINT()
    ctypes.windll.user32.GetCursorPos(ctypes.byref(pt0))
    try:
        for attempt in range(3):
            ctypes.windll.user32.SetCursorPos(pt0.x + 120, pt0.y + 80)
            time.sleep(0.15)
            target = POINT()
            ctypes.windll.user32.GetCursorPos(ctypes.byref(target))
            rect = wintypes.RECT()
            ok_rect = ctypes.windll.user32.GetWindowRect(hwnd, ctypes.byref(rect))
            cx, cy = (rect.left + rect.right) // 2, (rect.top + rect.bottom) // 2
            detail = f"center=({cx},{cy}) cursor=({target.x},{target.y})"
            if bool(ok_rect) and abs(cx - target.x) <= 6 and abs(cy - target.y) <= 6:
                tracked = True
                break
            time.sleep(0.2)
    finally:
        ctypes.windll.user32.SetCursorPos(pt0.x, pt0.y)
    check(tracked, "beacon re-centers on the moved cursor", detail)
    time.sleep(0.3)  # a few follow frames; still visible (no click yet)
    check(hwnd and ctypes.windll.user32.IsWindowVisible(hwnd),
          "beacon stays visible until a button is pressed")

    print("== action: toggleShake persists ==")
    resp = drv.action("toggleShake")
    effects = (resp or {}).get("result", {}).get("effects", [])
    check(any(e.get("type") == "notify" for e in effects),
          "toggleShake notifies the user", str(effects))
    state = state_content(data_dir) or ""
    check('"shakeEnabled":false' in state.replace(" ", ""),
          "toggleShake wrote shakeEnabled:false", state)
    resp = drv.query("")
    items = (resp or {}).get("result", {}).get("items", [])
    check(any("off" in i.get("title", "").lower() for i in items),
          "query reflects the disabled state in the row title")
    resp = drv.action("toggleShake")
    state = state_content(data_dir) or ""
    check('"shakeEnabled":true' in state.replace(" ", ""),
          "second toggle re-enables and persists", state)

    print("== action: copyDiagnostics ==")
    resp = drv.action("copyDiagnostics")
    effects = (resp or {}).get("result", {}).get("effects", [])
    copy_eff = next((e for e in effects if e.get("type") == "copy"), None)
    check(copy_eff is not None, "copyDiagnostics returns a copy effect", str(effects))
    text = (copy_eff or {}).get("text", "")
    check("hook installed: yes" in text, "diagnostics say the hook is installed", text)
    check("moves seen:" in text and "ripples triggered: 1" in text,
          "diagnostics carry the gesture counters", text)
    check("accent=#2AA8FF" in text, "diagnostics echo the default accent", text)
    check("effect=heart" in text and "custom style: preset (heart)" in text
          and "style document: 3 layers, loop=1400ms" in text,
          "omitted effect selects the default heartbeat document", text)
    check(not any(e.get("type") in ("open", "setQuery") for e in effects),
          "copyDiagnostics does not open anything or rewrite the query")

    print("== settingsChanged is applied ==")
    notify_only = {"jsonrpc": "2.0", "method": "settingsChanged", "params": {"settings": {
        "shakeEnabled": True, "shakeReversals": 6, "shakeWindowMs": 250,
        "rippleMs": 400, "rippleColor": "#FF0000",
    }}}
    drv._send(notify_only)
    time.sleep(0.2)
    resp = drv.action("copyDiagnostics")
    text = next((e for e in (resp or {}).get("result", {}).get("effects", [])
                 if e.get("type") == "copy"), {}).get("text", "")
    check("reversals=6" in text, "new shakeReversals visible in diagnostics", text)
    check("accent=#FF0000" in text, "new color visible in diagnostics", text)
    check("window=250ms" in text, "new window visible in diagnostics", text)

    print("== custom style: valid JSON drives the renderer ==")
    good_style = ("{\"loopMs\":600,\"layers\":["
                  "{\"shape\":\"ring\",\"from\":0.2,\"to\":1.0,\"ease\":\"outCubic\","
                  "\"width\":3.5,\"alpha\":{\"mode\":\"linearFade\"}},"
                  "{\"shape\":\"disc\",\"radius\":0.05,\"alpha\":1.0}]}")
    drv._send({"jsonrpc": "2.0", "method": "settingsChanged", "params": {"settings": {
        "rippleEffect": "custom", "customStyle": good_style,
    }}})
    time.sleep(0.2)
    resp = drv.action("copyDiagnostics")
    text = next((e for e in (resp or {}).get("result", {}).get("effects", [])
                 if e.get("type") == "copy"), {}).get("text", "")
    check("effect=custom" in text, "effect=custom visible in diagnostics", text)
    check("custom style: ok (2 layers)" in text, "custom doc parsed (2 layers)", text)
    resp = drv.action("showNow")
    time.sleep(0.25)
    check(hwnd and ctypes.windll.user32.IsWindowVisible(hwnd),
          "custom style renders a real window")

    print("== custom style: bad JSON falls back, never blanks ==")
    drv._send({"jsonrpc": "2.0", "method": "settingsChanged", "params": {"settings": {
        "rippleEffect": "custom", "customStyle": "{\"layers\":[{\"shape\":\"sparkle\"}]}",
    }}})
    time.sleep(0.2)
    resp = drv.action("copyDiagnostics")
    text = next((e for e in (resp or {}).get("result", {}).get("effects", [])
                 if e.get("type") == "copy"), {}).get("text", "")
    check("custom style: parse error" in text, "bad customStyle reported in diagnostics", text)
    resp = drv.action("showNow")
    time.sleep(0.25)
    check(hwnd and ctypes.windll.user32.IsWindowVisible(hwnd),
          "beacon still visible after a bad customStyle (previous custom retained)")

    print("== presets: live switching and custom fallback isolation ==")
    for effect, loop in (("ping", 1100), ("heart", 1400), ("orbit", 1600)):
        drv.settings_changed({"rippleEffect": effect, "followTimeoutMs": 0})
        text = drv.diagnostics()  # the response is a barrier after settingsChanged
        check(f"effect={effect}" in text and f"custom style: preset ({effect})" in text,
              f"{effect}: selection resolves to the preset", text)
        check(f"style document: 3 layers, loop={loop}ms" in text,
              f"{effect}: runtime selects the validated preset document", text)
        drv.action("showNow")
        time.sleep(0.15)
        check(ripple_windows_of_pid(drv.proc.pid) == [hwnd] and ctypes.windll.user32.IsWindowVisible(hwnd),
              f"{effect}: reuses the visible overlay")
        drv.settings_changed({"rippleEffect": "custom", "customStyle": "not json"})
        text = drv.diagnostics()
        check("parse error (previous custom style kept)" in text
              and "style document: 2 layers, loop=600ms" in text,
              f"{effect}: invalid custom edit restores the user's document, not the preset", text)

    drv.settings_changed({"rippleEffect": "ring"})
    drv.diagnostics()
    drv.settings_changed({"rippleEffect": "custom", "customStyle": "not json"})
    text = drv.diagnostics()
    check("style document: 2 layers, loop=600ms" in text,
          "switching through a built-in retains the last valid custom document", text)
    drv.settings_changed({"rippleEffect": "custom", "customStyle": ""})
    drv.diagnostics()
    drv.settings_changed({"rippleEffect": "orbit"})
    drv.diagnostics()
    drv.settings_changed({"rippleEffect": "custom", "customStyle": "not json"})
    text = drv.diagnostics()
    check("parse error (ring fallback)" in text and "style document: none" in text,
          "clearing custom JSON clears its fallback; presets never become a user document", text)

    print("== restore built-in style ==")
    drv._send({"jsonrpc": "2.0", "method": "settingsChanged", "params": {"settings": {
        "rippleEffect": "ring",
    }}})
    time.sleep(0.2)
    resp = drv.action("copyDiagnostics")
    text = next((e for e in (resp or {}).get("result", {}).get("effects", [])
                 if e.get("type") == "copy"), {}).get("text", "")
    check("effect=ring" in text, "built-in style restored", text)

    print("== decay: the beacon fades out instead of vanishing ==")
    # End via the auto-dismiss timeout (no synthetic clicks needed): follow
    # 300 ms, then a 900 ms fade. Mid-fade the window must still be shown;
    # after the fade it must be hidden.
    drv._send({"jsonrpc": "2.0", "method": "settingsChanged", "params": {"settings": {
        "followTimeoutMs": 300, "fadeOutMs": 900,
    }}})
    time.sleep(0.2)
    resp = drv.action("copyDiagnostics")
    text = next((e for e in (resp or {}).get("result", {}).get("effects", [])
                 if e.get("type") == "copy"), {}).get("text", "")
    check("fade out: 900ms" in text, "fadeOutMs visible in diagnostics", text)
    drv.action("showNow")
    time.sleep(0.75)  # timeout passed at ~0.3 s; fade runs until ~1.2 s
    check(hwnd and ctypes.windll.user32.IsWindowVisible(hwnd),
          "beacon still visible mid-fade (no abrupt hide)")
    hidden = False
    deadline = time.time() + 2.0
    while time.time() < deadline:
        if not ctypes.windll.user32.IsWindowVisible(hwnd):
            hidden = True
            break
        time.sleep(0.05)
    check(hidden, "beacon hidden once the fade completes")

    print("== decay: fadeOutMs = 0 hides at once ==")
    drv._send({"jsonrpc": "2.0", "method": "settingsChanged", "params": {"settings": {
        "followTimeoutMs": 300, "fadeOutMs": 0,
    }}})
    time.sleep(0.2)
    drv.action("showNow")
    time.sleep(0.6)  # timeout at ~0.3 s, no fade
    check(not ctypes.windll.user32.IsWindowVisible(hwnd),
          "fadeOutMs=0 hides right after the timeout")

    print("== robustness: garbage and unknown methods ==")
    drv.proc.stdin.write("this is not json\n")
    drv.proc.stdin.flush()
    time.sleep(0.2)
    resp = drv.query("")
    check(resp is not None and "items" in resp.get("result", {}),
          "process survives a garbage line")
    resp = drv.request("bogus/method", {})
    check(resp is not None and resp.get("error", {}).get("code") == -32601,
          "unknown method answers -32601", str(resp))

    print("== shutdown ==")
    # stop the workers and end; the window must be gone afterwards
    resp = drv.request("shutdown", {})
    check(resp is not None and "result" in resp and "error" not in resp,
          "shutdown answers cleanly", str(resp))
    code = drv.proc.wait(timeout=5)
    check(code == 0, "process exits 0 after shutdown", str(code))
    time.sleep(0.3)
    check(ripple_windows_of_pid(drv.proc.pid) == [],
          "ripple window destroyed after shutdown")
    err = drv.proc.stderr.read() if drv.proc.stderr else ""
    check("throw" not in err and "Internal error" not in err,
          "stderr is clean of crashes", err[:400])


def main():
    if not os.path.exists(EXE):
        print(f"findmouse.exe not found at {EXE}; build first.")
        return 1
    scratch = Path(HERE).parent / "build" / "integration"
    scratch.mkdir(parents=True, exist_ok=True)
    try:
        with tempfile.TemporaryDirectory(prefix="run-", dir=scratch) as data_dir:
            drv = Driver(data_dir)
            try:
                run(drv, data_dir)
            finally:
                drv.close()

        print("== presets: initialize-time settings reach the worker ==")
        for effect in ("ping", "heart", "orbit"):
            with tempfile.TemporaryDirectory(prefix="startup-", dir=scratch) as data_dir:
                drv = Driver(data_dir)
                try:
                    response = drv.initialize({"rippleEffect": effect, "followTimeoutMs": 250,
                                               "fadeOutMs": 0, "shakeEnabled": False})
                    check(response is not None and "error" not in response, f"{effect}: initializes cleanly")
                    # The SDK acknowledges initialize before calling initialized().
                    # A subsequent request is the protocol barrier for worker startup.
                    text = drv.diagnostics()
                    wins = ripple_windows_of_pid(drv.proc.pid)
                    check(len(wins) == 1, f"{effect}: overlay ready after initialization barrier")
                    hwnd = wins[0] if wins else 0
                    check(f"custom style: preset ({effect})" in text, f"{effect}: selected at initialize", text)
                    drv.action("showNow")
                    time.sleep(0.1)
                    check(hwnd and ctypes.windll.user32.IsWindowVisible(hwnd), f"{effect}: initial beacon shows")
                    time.sleep(0.6)
                    check(hwnd and not ctypes.windll.user32.IsWindowVisible(hwnd),
                          f"{effect}: initialize-time timeout and fade settings take effect")
                finally:
                    drv.close()
    finally:
        scratch.rmdir()  # TemporaryDirectory has already removed all test state
    print(f"\n{passed} passed, {failed} failed")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
