#!/usr/bin/env python3
"""Serve the web UI with a simulated device API, to work on components/web_ui/www without flashing.

    python3 tools/mock_server.py [--port 8080] [--portal]

--portal starts in setup hotspot mode. In the setup form, a password starting with "wrong" (e.g. "wrongpass")
simulates a rejected password.
"""
import argparse
import json
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

WWW = Path(__file__).resolve().parent.parent / "components" / "web_ui" / "www"
STARTED = time.time()
SIGNALS = {0: "talk start", 2: "ring", 3: "door open", 8: "talk end", 11: "light", 12: "ring (alt)"}
NETWORKS = [
    {"ssid": "cron-office", "rssi": -52, "secured": True},
    {"ssid": "cron-guest", "rssi": -61, "secured": False},
    {"ssid": "FRITZ!Box 7590 XY", "rssi": -70, "secured": True},
    {"ssid": "<script>alert('xss')</script>", "rssi": -84, "secured": True},
]

lock = threading.Lock()
state = {
    "portal": False,
    "sta": "connected",
    "ssid": "cron-office",
    "trial": "none",
    "trial_ssid": "",
    "trial_ok": True,
    "trial_since": 0.0,
    "last_reason": 0,
}


def bus_log():
    now = int(time.time())
    events = [(now - 40, 1151144720, "rx", True), (now - 300, 1181356688, "tx", True),
              (now - 310, 1151144720, "rx", True), (now - 7200, 1141051484, "tx", False),
              (now - 90000, 1174491228, "rx", True)]
    entries = []
    for ts, cmd, direction, ok in events:
        signal = (cmd >> 25) & 0xF
        entry = {"ts": ts, "cmd": cmd, "dir": direction, "ok": ok, "signal": signal,
                 "src": (cmd >> 2) & 0x1FF, "dst": (cmd >> 14) & 0x1FF}
        if signal in SIGNALS:
            entry["signal_name"] = SIGNALS[signal]
        entries.append(entry)
    return entries


def status():
    with lock:
        if state["trial"] == "pending" and time.time() - state["trial_since"] > 3:
            if state["trial_ok"]:
                state.update(trial="ok", sta="connected", ssid=state["trial_ssid"])
                # the real device closes the hotspot after CONFIG_WIFI_MGR_PORTAL_LINGER_SEC
            else:
                state.update(trial="failed", last_reason=15, sta="unconfigured" if not state["ssid"] else "connecting")
        connected = state["sta"] == "connected"
        shown_ssid = state["trial_ssid"] if state["trial"] == "pending" else state["ssid"]
        return {
            "device": {
                "hostname": "siedle", "version": "v0.1.0-mock", "idf": "v6.1", "chip": "esp32",
                "uptime_s": int(time.time() - STARTED) + 3 * 86400 + 5 * 3600,
                "reset_reason": "power on", "heap_free": 182340, "heap_min": 151200,
                "time_synced": connected, "time": int(time.time()),
            },
            "wifi": {
                "state": "connecting" if state["trial"] == "pending" else state["sta"],
                "ssid": shown_ssid, "ip": "192.168.1.42" if connected else "",
                "rssi": -58 if connected else 0, "last_reason": state["last_reason"],
                "portal": state["portal"], "ap_ssid": "Siedle-Setup-A1B2", "ap_secured": True,
                "trial": state["trial"],
            },
            "cloud": {
                "configured": True, "client_id": "SiedleGateway", "connected": connected,
                "connects": 3, "published": 128, "received": 4, "dropped": 0,
            },
            "bus": {"available": False},
        }


class Handler(BaseHTTPRequestHandler):
    def send_json(self, code, payload):
        body = json.dumps(payload).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/":
            body = (WWW / "index.html").read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif self.path == "/api/status":
            self.send_json(200, status())
        elif self.path == "/api/log":
            self.send_json(200, bus_log())
        elif self.path == "/api/wifi/scan":
            if not state["portal"]:
                return self.send_json(403, {"error": "Wi-Fi setup is only available on the setup hotspot"})
            time.sleep(1.5)
            self.send_json(200, NETWORKS)
        elif state["portal"]:
            self.send_response(302)
            self.send_header("Location", "/")
            self.end_headers()
        else:
            self.send_json(404, {"error": "not found"})

    def do_POST(self):
        if self.path != "/api/wifi":
            return self.send_json(404, {"error": "not found"})
        if not state["portal"]:
            return self.send_json(403, {"error": "Wi-Fi setup is only available on the setup hotspot"})
        try:
            data = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
            ssid, password = data["ssid"], data.get("password", "")
        except (ValueError, KeyError, TypeError):
            return self.send_json(400, {"error": "invalid body"})
        if not 1 <= len(ssid) <= 32 or not (password == "" or 8 <= len(password) <= 64):
            return self.send_json(400, {"error": "SSID must be 1-32 characters, the password empty or 8-64 characters"})
        with lock:
            state.update(trial="pending", trial_ssid=ssid, trial_ok=not password.startswith("wrong"), trial_since=time.time())
        self.send_json(202, {})

    def log_message(self, fmt, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--portal", action="store_true", help="start in setup hotspot mode")
    args = parser.parse_args()
    if args.portal:
        state.update(portal=True, sta="unconfigured", ssid="")
    print(f"Serving the web UI on http://localhost:{args.port}/")
    ThreadingHTTPServer(("127.0.0.1", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
