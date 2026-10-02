#!/usr/bin/env python3
"""Serve the web UI with a simulated device API, to work on components/web_ui/www without flashing.

    python3 tools/mock_server.py [--port 8080] [--portal [unconfigured|manual|fallback]]

--portal starts in setup hotspot mode: "unconfigured" (the default) like a new device, "manual" as if opened with
the BOOT button, "fallback" as if it opened by itself while the gateway was offline (cloud settings locked).

In the Wi-Fi setup, a password starting with "wrong" (e.g. "wrongpass") simulates a rejected password. In the cloud
settings, a certificate containing "wrong" is rejected as belonging to another key, and an endpoint starting with
"wrong" never connects.
"""
import argparse
import base64
import hashlib
import json
import re
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
LABEL = r"[A-Za-z0-9]([A-Za-z0-9-]{0,61}[A-Za-z0-9])?"
ENDPOINT = re.compile(rf"{LABEL}(\.{LABEL})+")
HOSTNAME = re.compile(LABEL)
THING_NAME = re.compile(r"[A-Za-z0-9_:-]{1,128}")
AP_PASSWORD = re.compile(r"[\x20-\x7e]{8,63}")

lock = threading.Lock()
state = {
    "portal": "off",
    "sta": "connected",
    "ssid": "cron-office",
    "trial": "none",
    "trial_ssid": "",
    "trial_ok": True,
    "trial_since": 0.0,
    "last_reason": 0,
    "endpoint": "a1b2c3d4e5f6g7-ats.iot.eu-central-1.amazonaws.com",
    "thing": "SiedleGateway",
    "cert": "mock certificate",
    "key": 1,  # changes with every new key
    "cloud_since": 0.0,  # connecting takes a few seconds after a change
    "hostname": "",
    "ap_password": True,
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


def cloud_status(wifi_connected):
    configured = bool(state["endpoint"] and state["thing"] and state["cert"])
    cloud = {"configured": configured, "connected": False, "connects": 3, "published": 128, "received": 4,
             "dropped": 0}
    if configured:
        cloud["client_id"] = state["thing"]
        if not wifi_connected or time.time() - state["cloud_since"] < 3:
            pass
        elif state["endpoint"].startswith("wrong"):
            cloud["error"] = "Endpoint not found"
        else:
            cloud["connected"] = True
    return cloud


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
        portal = state["portal"] != "off"
        return {
            "device": {
                "hostname": state["hostname"] or "siedle", "version": "v0.1.0-mock", "idf": "v6.1", "chip": "esp32",
                "uptime_s": int(time.time() - STARTED) + 3 * 86400 + 5 * 3600,
                "reset_reason": "power on", "heap_free": 182340, "heap_min": 151200,
                "time_synced": connected, "time": int(time.time()),
            },
            "wifi": {
                "state": "connecting" if state["trial"] == "pending" else state["sta"],
                "ssid": shown_ssid, "ip": "192.168.1.42" if connected else "",
                "rssi": -58 if connected else 0, "last_reason": state["last_reason"],
                "portal": portal, "portal_mode": state["portal"], "ap_ssid": "Siedle-Setup-A1B2",
                "ap_secured": state["ap_password"], "trial": state["trial"],
            },
            "cloud": cloud_status(connected),
            "bus": {"available": False},
            "via_hotspot": portal,  # the mock's browser is always on the hotspot while it is open
        }


def cloud_doc():
    cert = None
    if state["cert"]:
        cert = {
            "subject": "CN=" + state["thing"],
            "issuer": "OU=Amazon Web Services, O=Amazon.com Inc., L=Seattle, ST=Washington, C=US",
            "not_after": "2049-12-31T23:59:59Z",
            "fingerprint": hashlib.sha256(state["cert"].encode()).hexdigest(),
        }
    cloud = cloud_status(state["sta"] == "connected")
    return {"endpoint": state["endpoint"], "thing_name": state["thing"], "key": True, "certificate": cert,
            "connected": cloud["connected"], "error": cloud.get("error", "")}


def fake_csr(thing):
    body = base64.b64encode(hashlib.sha512(f"{thing}/{state['key']}".encode()).digest() * 4).decode()
    lines = [body[i:i + 64] for i in range(0, len(body), 64)]
    return "-----BEGIN CERTIFICATE REQUEST-----\n" + "\n".join(lines) + "\n-----END CERTIFICATE REQUEST-----\n"


def normalize_endpoint(endpoint):
    endpoint = endpoint.strip()
    endpoint = endpoint.split("://", 1)[-1]
    endpoint = re.split(r"[/\s]", endpoint, maxsplit=1)[0]
    endpoint = endpoint.removesuffix(":8883")
    return endpoint.lower()


class Handler(BaseHTTPRequestHandler):
    def send_json(self, code, payload):
        body = json.dumps(payload).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def require_portal(self):
        if state["portal"] == "off":
            self.send_json(403, {"error": "Wi-Fi setup is only available on the setup hotspot"})
            return False
        return True

    def require_admin(self):
        if state["portal"] == "off":
            self.send_json(403, {"error": "Only available on the setup hotspot"})
            return False
        if state["portal"] == "fallback":
            self.send_json(403, {"error": "Locked, this hotspot opened by itself. Open it with the setup button."})
            return False
        return True

    def read_json(self):
        """The body, None after answering 400 (like the device, only application/json is accepted)."""
        if not self.headers.get("Content-Type", "").lower().startswith("application/json"):
            self.send_json(400, {"error": "invalid body"})
            return None
        try:
            data = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))))
            if not isinstance(data, dict):
                raise ValueError
            return data
        except ValueError:
            self.send_json(400, {"error": "invalid body"})
            return None

    def do_GET(self):
        path, _, query = self.path.partition("?")
        if path == "/":
            body = (WWW / "index.html").read_bytes()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
        elif path == "/api/status":
            self.send_json(200, status())
        elif path == "/api/log":
            self.send_json(200, bus_log())
        elif path == "/api/wifi/scan":
            if self.require_portal():
                time.sleep(1.5)
                self.send_json(200, NETWORKS)
        elif path == "/api/cloud":
            if self.require_admin():
                self.send_json(200, cloud_doc())
        elif path == "/api/cloud/csr":
            if self.require_admin():
                # like the device, no URL decoding: thing names don't need it
                params = dict(kv.split("=", 1) for kv in query.split("&") if "=" in kv)
                thing = params.get("thing_name", "")
                if not THING_NAME.fullmatch(thing):
                    return self.send_json(400, {"error": "Enter the thing name first."})
                body = fake_csr(thing).encode()
                self.send_response(200)
                self.send_header("Content-Type", "application/pkcs10")
                self.send_header("Content-Disposition", f'attachment; filename="{thing}.csr"')
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)
        elif path == "/api/device":
            if self.require_admin():
                self.send_json(200, {"hostname": state["hostname"], "ap_password": state["ap_password"]})
        elif state["portal"] != "off":
            self.send_response(302)
            self.send_header("Location", "/")
            self.end_headers()
        else:
            self.send_json(404, {"error": "not found"})

    def do_POST(self):
        if self.path == "/api/wifi":
            self.post_wifi()
        elif self.path == "/api/cloud":
            self.post_cloud()
        elif self.path == "/api/cloud/key":
            self.post_key()
        elif self.path == "/api/device":
            self.post_device()
        else:
            self.send_json(404, {"error": "not found"})

    def post_wifi(self):
        if not self.require_portal() or (data := self.read_json()) is None:
            return
        ssid, password = data.get("ssid"), data.get("password", "")
        if not isinstance(ssid, str) or not isinstance(password, str):
            return self.send_json(400, {"error": "invalid body"})
        if not 1 <= len(ssid) <= 32 or not (password == "" or 8 <= len(password) <= 64):
            return self.send_json(400, {"error": "SSID must be 1-32 characters, the password empty or 8-64 characters"})
        with lock:
            state.update(trial="pending", trial_ssid=ssid, trial_ok=not password.startswith("wrong"), trial_since=time.time())
        self.send_json(202, {})

    def post_cloud(self):
        if not self.require_admin() or (data := self.read_json()) is None:
            return
        endpoint, thing, cert = data.get("endpoint"), data.get("thing_name"), data.get("certificate")
        if not isinstance(endpoint, str) or not isinstance(thing, str) or not (cert is None or isinstance(cert, str)):
            return self.send_json(400, {"error": "invalid body"})
        endpoint = normalize_endpoint(endpoint)
        if not ENDPOINT.fullmatch(endpoint) or len(endpoint) > 253:
            return self.send_json(400, {"error": "Enter the endpoint from the AWS IoT console (Settings, Device data endpoint)."})
        if not THING_NAME.fullmatch(thing):
            return self.send_json(400, {"error": "The thing name can only have letters, digits, '-', '_' and ':' (up to 128)."})
        if cert and "-----BEGIN CERTIFICATE-----" not in cert:
            return self.send_json(400, {"error": "This is not a certificate. Use the device certificate file from AWS IoT (...-certificate.pem.crt)."})
        if cert and "wrong" in cert:
            return self.send_json(400, {"error": "This certificate is for another key. Create it in AWS IoT from this gateway's current CSR."})
        time.sleep(0.5)  # checking the certificate takes a moment on the device
        with lock:
            state.update(endpoint=endpoint, thing=thing, cloud_since=time.time())
            if cert:
                state["cert"] = cert
        self.send_json(200, cloud_doc())

    def post_key(self):
        if not self.require_admin() or self.read_json() is None:
            return
        time.sleep(1)
        with lock:
            state.update(key=state["key"] + 1, cert=None, cloud_since=time.time())
        self.send_json(200, cloud_doc())

    def post_device(self):
        if not self.require_admin() or (data := self.read_json()) is None:
            return
        hostname, password = data.get("hostname"), data.get("ap_password")
        if not (hostname is None or isinstance(hostname, str)) or not (password is None or isinstance(password, str)):
            return self.send_json(400, {"error": "invalid body"})
        if hostname and not HOSTNAME.fullmatch(hostname):
            return self.send_json(400, {"error": "The name can only have letters, digits and hyphens (up to 63), no hyphen at either end."})
        if password and not AP_PASSWORD.fullmatch(password):
            return self.send_json(400, {"error": "The hotspot password needs 8 to 63 characters (letters, digits, punctuation)."})
        with lock:
            if hostname is not None:
                state["hostname"] = hostname
            if password is not None:
                state["ap_password"] = password != ""
        self.send_json(200, {"hostname": state["hostname"], "ap_password": state["ap_password"]})

    def log_message(self, fmt, *args):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8080)
    parser.add_argument("--portal", nargs="?", const="unconfigured", choices=["unconfigured", "manual", "fallback"],
                        help="start in setup hotspot mode")
    args = parser.parse_args()
    if args.portal == "unconfigured":
        # a new device: no Wi-Fi, no cloud identity yet, an open hotspot
        state.update(portal="unconfigured", sta="unconfigured", ssid="", endpoint="", thing="", cert=None,
                     ap_password=False)
    elif args.portal == "manual":
        state["portal"] = "manual"
    elif args.portal == "fallback":
        state.update(portal="fallback", sta="connecting")
    print(f"Serving the web UI on http://localhost:{args.port}/")
    ThreadingHTTPServer(("127.0.0.1", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
