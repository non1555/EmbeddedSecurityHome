import json
import os
import platform
import shutil
import signal
import socket
import subprocess
import sys
import threading
import time
import urllib.request
import webbrowser
from pathlib import Path
from tkinter import Tk, StringVar, ttk, messagebox
from glob import glob


ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = ROOT.parent.parent
ENV_PATH = ROOT / ".env"
PLATFORMIO_INI_PATH = PROJECT_ROOT / "platformio.ini"
LOG_DIR = ROOT / "logs"
LOG_DIR.mkdir(exist_ok=True)
DEFAULT_FW_ENV = "esp32doit-devkit-v1"


def read_env(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    if not path.exists():
        return out
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[k.strip()] = v.strip()
    return out


def read_env_lines(path: Path) -> list[str]:
    if not path.exists():
        return []
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def write_env_lines(path: Path, lines: list[str]) -> None:
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def upsert_env_kv(lines: list[str], key: str, value: str) -> list[str]:
    out: list[str] = []
    found = False
    for ln in lines:
        if ln.startswith(f"{key}="):
            out.append(f"{key}={value}")
            found = True
        else:
            out.append(ln)
    if not found:
        out.append(f"{key}={value}")
    return out


def http_get_json(url: str, timeout_s: float = 2.0) -> dict:
    req = urllib.request.Request(url, headers={"User-Agent": "EmbeddedSecurityHome/launcher"})
    with urllib.request.urlopen(req, timeout=timeout_s) as resp:
        data = resp.read()
    return json.loads(data.decode("utf-8", errors="replace"))


def port_listeners(port: int) -> list[int]:
    if os.name == "nt":
        # Parse netstat output for LISTENING PID(s) on :port
        try:
            out = subprocess.check_output(["netstat", "-ano"], text=True, errors="replace")
        except Exception:
            return []

        pids: list[int] = []
        needle = f":{port} "
        for line in out.splitlines():
            if "LISTENING" not in line:
                continue
            if needle not in line:
                continue
            parts = [p for p in line.split() if p]
            if not parts:
                continue
            try:
                pid = int(parts[-1])
            except Exception:
                continue
            pids.append(pid)
        return sorted(set(pids))

    # Linux/macOS best-effort: use lsof if available.
    if shutil.which("lsof"):
        try:
            out = subprocess.check_output(
                ["lsof", "-tiTCP:%d" % port, "-sTCP:LISTEN"],
                text=True,
                errors="replace",
            )
        except Exception:
            return []
        pids: list[int] = []
        for ln in out.splitlines():
            try:
                pids.append(int(ln.strip()))
            except Exception:
                pass
        return sorted(set(pids))

    return []


def taskkill(pid: int) -> None:
    if pid <= 0:
        return
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(pid), "/F"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return
    try:
        os.kill(pid, signal.SIGKILL)
    except Exception:
        pass


def detect_serial_ports() -> list[str]:
    ports: list[str] = []

    # Preferred: pyserial
    try:
        import serial.tools.list_ports  # type: ignore

        ports = [str(p.device) for p in serial.tools.list_ports.comports()]
    except Exception:
        ports = []

    # Fallbacks if pyserial isn't available.
    if not ports:
        if os.name == "nt":
            try:
                out = subprocess.check_output(
                    [
                        "powershell",
                        "-NoProfile",
                        "-Command",
                        "[System.IO.Ports.SerialPort]::GetPortNames() -join \"`n\"",
                    ],
                    text=True,
                    errors="replace",
                )
                ports = [ln.strip() for ln in out.splitlines() if ln.strip()]
            except Exception:
                ports = []
        else:
            devs = []
            devs.extend(glob("/dev/ttyUSB*"))
            devs.extend(glob("/dev/ttyACM*"))
            devs.extend(glob("/dev/tty.usbserial*"))
            devs.extend(glob("/dev/tty.SLAB_USBtoUART*"))
            ports = [d for d in devs if d]

    # Deduplicate while preserving order.
    seen = set()
    out_ports: list[str] = []
    for p in ports:
        if p in seen:
            continue
        seen.add(p)
        out_ports.append(p)
    return out_ports


def current_wifi_ssid() -> str:
    if os.name == "nt":
        try:
            out = subprocess.check_output(["netsh", "wlan", "show", "interfaces"], text=True, errors="replace")
            for ln in out.splitlines():
                line = ln.strip()
                if line.startswith("SSID") and " : " in line and not line.startswith("BSSID"):
                    return line.split(" : ", 1)[1].strip()
        except Exception:
            return ""
        return ""
    if shutil.which("nmcli"):
        try:
            out = subprocess.check_output(["nmcli", "-t", "-f", "active,ssid", "dev", "wifi"], text=True, errors="replace")
            for ln in out.splitlines():
                if ln.startswith("yes:"):
                    return ln.split(":", 1)[1].strip()
        except Exception:
            return ""
    return ""


def local_ip() -> str:
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.connect(("8.8.8.8", 80))
        ip = sock.getsockname()[0]
        sock.close()
        return ip
    except Exception:
        return ""


class LauncherApp:
    def __init__(self) -> None:
        self.root = Tk()
        self.root.title("EmbeddedSecurity LINE Bridge")
        self.root.resizable(False, False)

        self.env = read_env(ENV_PATH)
        self.http_port = int(self.env.get("HTTP_PORT", "8080") or "8080")

        self.bridge_proc: subprocess.Popen | None = None
        self.ngrok_proc: subprocess.Popen | None = None

        self.status_var = StringVar(value="Stopped")
        self.health_var = StringVar(value="Health: (unknown)")
        self.ngrok_var = StringVar(value="ngrok: (unknown)")
        self.webhook_var = StringVar(value="Webhook: (unknown)")

        # Config fields (.env)
        self.cfg_http_port = StringVar(value=str(self.http_port))
        self.cfg_line_token = StringVar(value=self.env.get("LINE_CHANNEL_ACCESS_TOKEN", ""))
        self.cfg_line_secret = StringVar(value=self.env.get("LINE_CHANNEL_SECRET", ""))
        self.cfg_line_target_user = StringVar(value=self.env.get("LINE_TARGET_USER_ID", ""))
        self.cfg_line_target_group = StringVar(value=self.env.get("LINE_TARGET_GROUP_ID", ""))
        self.cfg_line_target_room = StringVar(value=self.env.get("LINE_TARGET_ROOM_ID", ""))
        self.cfg_ngrok_authtoken = StringVar(value=self.env.get("NGROK_AUTHTOKEN", ""))
        self._show_secrets = False

        # Firmware config (stored in .env; PlatformIO reads it via tools/pio_env.py)
        self.fw_wifi_ssid = StringVar(value=self.env.get("FW_WIFI_SSID", ""))
        self.fw_wifi_password = StringVar(value=self.env.get("FW_WIFI_PASSWORD", ""))
        self.fw_mqtt_broker = StringVar(value=self.env.get("FW_MQTT_BROKER", ""))
        self.fw_mqtt_port = StringVar(value=self.env.get("FW_MQTT_PORT", "1883"))
        self.fw_mqtt_username = StringVar(value=self.env.get("FW_MQTT_USERNAME", ""))
        self.fw_mqtt_password = StringVar(value=self.env.get("FW_MQTT_PASSWORD", ""))
        self.fw_mqtt_client_id = StringVar(value=self.env.get("FW_MQTT_CLIENT_ID", "embedded-security-esp32"))
        self.fw_upload_port = StringVar(value=self.env.get("FW_UPLOAD_PORT", ""))
        self.serial_ports: list[str] = []
        self.current_wifi_ssid_var = StringVar(value="")
        self.current_wifi_ip_var = StringVar(value="")
        self._show_fw_wifi_password = False

        # Services tab state
        self.check_python_var = StringVar(value="Python: (unknown)")
        self.check_venv_var = StringVar(value="Venv: (unknown)")
        self.check_deps_var = StringVar(value="Bridge Deps: (unknown)")
        self.check_pio_var = StringVar(value="PlatformIO: (unknown)")
        self.check_ngrok_var = StringVar(value="ngrok: (unknown)")
        self.service_bridge_var = StringVar(value="(unknown)")
        self.service_ngrok_var = StringVar(value="(unknown)")
        self.service_mosquitto_var = StringVar(value="(unknown)")
        self.install_action_var = StringVar(value="Ready")
        self._btn_setup_deps: ttk.Button | None = None
        self._btn_install_pio: ttk.Button | None = None
        self._btn_install_ngrok: ttk.Button | None = None
        self._btn_open_python_location: ttk.Button | None = None
        self._btn_bridge_toggle: ttk.Button | None = None
        self._btn_ngrok_toggle: ttk.Button | None = None
        self._btn_mosquitto_toggle: ttk.Button | None = None

        self._build_ui()
        self._tick()

        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_ui(self) -> None:
        frm = ttk.Frame(self.root, padding=12)
        frm.grid(row=0, column=0, sticky="nsew")

        nb = ttk.Notebook(frm)
        nb.grid(row=0, column=0, sticky="nsew")

        status_tab = ttk.Frame(nb, padding=12)
        config_tab = ttk.Frame(nb, padding=12)
        firmware_tab = ttk.Frame(nb, padding=12)
        install_tab = ttk.Frame(nb, padding=12)
        nb.add(status_tab, text="Status")
        nb.add(config_tab, text="Config")
        nb.add(firmware_tab, text="Firmware")
        nb.add(install_tab, text="Services")

        # Status tab
        status_tab.columnconfigure(0, weight=1)

        status_head = ttk.Frame(status_tab)
        status_head.grid(row=0, column=0, sticky="ew")
        status_head.columnconfigure(0, weight=0)
        status_head.columnconfigure(1, weight=1)

        ttk.Label(status_head, text="Bridge").grid(row=0, column=0, sticky="w")
        ttk.Label(status_head, textvariable=self.status_var).grid(row=0, column=1, sticky="w", padx=(10, 0))

        status_info = ttk.Frame(status_tab)
        status_info.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        status_info.columnconfigure(0, weight=1)
        ttk.Label(status_info, textvariable=self.health_var).grid(row=0, column=0, sticky="w")
        ttk.Label(status_info, textvariable=self.ngrok_var).grid(row=1, column=0, sticky="w", pady=(4, 0))
        ttk.Label(status_info, textvariable=self.webhook_var).grid(row=2, column=0, sticky="w", pady=(4, 0))

        ttk.Separator(status_tab).grid(row=2, column=0, sticky="ew", pady=(12, 12))

        btns = ttk.Frame(status_tab)
        btns.grid(row=3, column=0, sticky="ew")
        btns.columnconfigure(0, weight=1)
        btns.columnconfigure(1, weight=1)
        btns.columnconfigure(2, weight=1)

        ttk.Button(btns, text="Start", command=self.start).grid(row=0, column=0, sticky="ew", padx=(0, 6))
        ttk.Button(btns, text="Stop", command=self.stop).grid(row=0, column=1, sticky="ew", padx=(0, 6))
        ttk.Button(btns, text="Copy Webhook", command=self.copy_webhook).grid(row=0, column=2, sticky="ew")

        links = ttk.Frame(status_tab)
        links.grid(row=4, column=0, sticky="ew", pady=(10, 0))
        links.columnconfigure(0, weight=1)
        links.columnconfigure(1, weight=1)

        ttk.Button(links, text="Open Health", command=self.open_health).grid(row=0, column=0, sticky="ew", padx=(0, 6))
        ttk.Button(links, text="Open ngrok Inspector", command=self.open_inspector).grid(row=0, column=1, sticky="ew")

        # Config tab
        config_tab.columnconfigure(1, weight=1)

        r = 0
        ttk.Label(config_tab, text="HTTP Port").grid(row=r, column=0, sticky="w")
        ttk.Entry(config_tab, textvariable=self.cfg_http_port, width=10).grid(row=r, column=1, sticky="w")
        r += 1

        ttk.Separator(config_tab).grid(row=r, column=0, columnspan=2, sticky="ew", pady=(12, 12))
        r += 1

        ttk.Label(config_tab, text="LINE Access Token").grid(row=r, column=0, sticky="w")
        self._token_entry = ttk.Entry(config_tab, textvariable=self.cfg_line_token, width=36, show="*")
        self._token_entry.grid(row=r, column=1, sticky="ew")
        r += 1

        ttk.Label(config_tab, text="LINE Channel Secret").grid(row=r, column=0, sticky="w", pady=(6, 0))
        self._secret_entry = ttk.Entry(config_tab, textvariable=self.cfg_line_secret, width=36, show="*")
        self._secret_entry.grid(row=r, column=1, sticky="ew", pady=(6, 0))
        r += 1

        ttk.Button(config_tab, text="Show/Hide Secrets", command=self.toggle_secrets).grid(
            row=r, column=0, columnspan=2, sticky="w", pady=(8, 0)
        )
        r += 1

        ttk.Label(config_tab, text="LINE Target User ID (optional)").grid(row=r, column=0, sticky="w", pady=(10, 0))
        ttk.Entry(config_tab, textvariable=self.cfg_line_target_user, width=36).grid(row=r, column=1, sticky="ew", pady=(10, 0))
        r += 1
        ttk.Label(config_tab, text="LINE Target Group ID (optional)").grid(row=r, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(config_tab, textvariable=self.cfg_line_target_group, width=36).grid(row=r, column=1, sticky="ew", pady=(6, 0))
        r += 1
        ttk.Label(config_tab, text="LINE Target Room ID (optional)").grid(row=r, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(config_tab, textvariable=self.cfg_line_target_room, width=36).grid(row=r, column=1, sticky="ew", pady=(6, 0))
        r += 1

        ttk.Separator(config_tab).grid(row=r, column=0, columnspan=2, sticky="ew", pady=(12, 12))
        r += 1

        ttk.Label(config_tab, text="ngrok Authtoken").grid(row=r, column=0, sticky="w")
        self._ngrok_entry = ttk.Entry(config_tab, textvariable=self.cfg_ngrok_authtoken, width=36, show="*")
        self._ngrok_entry.grid(row=r, column=1, sticky="ew")
        r += 1

        cfg_btns = ttk.Frame(config_tab)
        cfg_btns.grid(row=r, column=0, columnspan=2, sticky="ew", pady=(12, 0))
        cfg_btns.columnconfigure(0, weight=1)
        cfg_btns.columnconfigure(1, weight=1)
        cfg_btns.columnconfigure(2, weight=1)

        ttk.Button(cfg_btns, text="Save .env", command=self.save_env).grid(row=0, column=0, sticky="ew", padx=(0, 6))
        ttk.Button(cfg_btns, text="Set ngrok Authtoken", command=self.set_ngrok_authtoken).grid(
            row=0, column=1, sticky="ew", padx=(0, 6)
        )
        ttk.Button(cfg_btns, text="Open .env Folder", command=self.open_env_folder).grid(row=0, column=2, sticky="ew")

        # Firmware tab
        firmware_tab.columnconfigure(1, weight=1)
        fr = 0
        ttk.Label(firmware_tab, text="Current Wi-Fi SSID").grid(row=fr, column=0, sticky="w")
        ttk.Entry(firmware_tab, textvariable=self.current_wifi_ssid_var, width=36, state="readonly").grid(row=fr, column=1, sticky="ew")
        fr += 1
        ttk.Label(firmware_tab, text="Current Local IP").grid(row=fr, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(firmware_tab, textvariable=self.current_wifi_ip_var, width=36, state="readonly").grid(row=fr, column=1, sticky="ew", pady=(6, 0))
        fr += 1
        cur_wifi_btns = ttk.Frame(firmware_tab)
        cur_wifi_btns.grid(row=fr, column=0, columnspan=2, sticky="w", pady=(8, 0))
        ttk.Button(cur_wifi_btns, text="Refresh Current Wi-Fi", command=self.refresh_current_wifi_info).grid(row=0, column=0, sticky="w")
        ttk.Button(cur_wifi_btns, text="Use Current Wi-Fi", command=self.use_current_wifi_info).grid(row=0, column=1, sticky="w", padx=(6, 0))
        fr += 1

        ttk.Separator(firmware_tab).grid(row=fr, column=0, columnspan=2, sticky="ew", pady=(12, 12))
        fr += 1

        ttk.Label(firmware_tab, text="Wi-Fi SSID").grid(row=fr, column=0, sticky="w")
        ttk.Entry(firmware_tab, textvariable=self.fw_wifi_ssid, width=36).grid(row=fr, column=1, sticky="ew")
        fr += 1
        ttk.Label(firmware_tab, text="Wi-Fi Password").grid(row=fr, column=0, sticky="w", pady=(6, 0))
        self._fw_wifi_password_entry = ttk.Entry(firmware_tab, textvariable=self.fw_wifi_password, width=36, show="*")
        self._fw_wifi_password_entry.grid(row=fr, column=1, sticky="ew", pady=(6, 0))
        fr += 1
        fw_pwd_btns = ttk.Frame(firmware_tab)
        fw_pwd_btns.grid(row=fr, column=0, columnspan=2, sticky="w", pady=(8, 0))
        ttk.Button(fw_pwd_btns, text="Show/Hide Wi-Fi Password", command=self.toggle_fw_wifi_password).grid(row=0, column=0, sticky="w")
        ttk.Button(fw_pwd_btns, text="Clear Wi-Fi Password", command=self.clear_fw_wifi_password).grid(row=0, column=1, sticky="w", padx=(6, 0))
        fr += 1

        ttk.Separator(firmware_tab).grid(row=fr, column=0, columnspan=2, sticky="ew", pady=(12, 12))
        fr += 1

        ttk.Label(firmware_tab, text="MQTT Broker").grid(row=fr, column=0, sticky="w")
        ttk.Entry(firmware_tab, textvariable=self.fw_mqtt_broker, width=36).grid(row=fr, column=1, sticky="ew")
        fr += 1
        ttk.Label(firmware_tab, text="MQTT Port").grid(row=fr, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(firmware_tab, textvariable=self.fw_mqtt_port, width=10).grid(row=fr, column=1, sticky="w", pady=(6, 0))
        fr += 1
        ttk.Label(firmware_tab, text="MQTT Username").grid(row=fr, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(firmware_tab, textvariable=self.fw_mqtt_username, width=36).grid(row=fr, column=1, sticky="ew", pady=(6, 0))
        fr += 1
        ttk.Label(firmware_tab, text="MQTT Password").grid(row=fr, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(firmware_tab, textvariable=self.fw_mqtt_password, width=36, show="*").grid(row=fr, column=1, sticky="ew", pady=(6, 0))
        fr += 1
        ttk.Label(firmware_tab, text="MQTT Client ID").grid(row=fr, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(firmware_tab, textvariable=self.fw_mqtt_client_id, width=36).grid(row=fr, column=1, sticky="ew", pady=(6, 0))
        fr += 1

        ttk.Separator(firmware_tab).grid(row=fr, column=0, columnspan=2, sticky="ew", pady=(12, 12))
        fr += 1

        ttk.Label(firmware_tab, text="Upload Port").grid(row=fr, column=0, sticky="w")
        self._upload_port_box = ttk.Combobox(firmware_tab, textvariable=self.fw_upload_port, state="readonly", width=28)
        self._upload_port_box.grid(row=fr, column=1, sticky="w")
        fr += 1

        refresh_frm = ttk.Frame(firmware_tab)
        refresh_frm.grid(row=fr, column=0, columnspan=2, sticky="w", pady=(6, 0))
        ttk.Button(refresh_frm, text="Refresh Ports", command=self.refresh_upload_ports).grid(row=0, column=0, sticky="w")
        fr += 1

        fw_btns = ttk.Frame(firmware_tab)
        fw_btns.grid(row=fr, column=0, columnspan=2, sticky="ew", pady=(12, 0))
        fw_btns.columnconfigure(0, weight=1)
        fw_btns.columnconfigure(1, weight=1)
        fw_btns.columnconfigure(2, weight=1)

        ttk.Button(fw_btns, text="Save to .env", command=self.save_firmware_env).grid(row=0, column=0, sticky="ew", padx=(0, 6))
        ttk.Button(fw_btns, text="Build", command=self.build_firmware).grid(row=0, column=1, sticky="ew", padx=(0, 6))
        ttk.Button(fw_btns, text="Upload", command=self.upload_firmware).grid(row=0, column=2, sticky="ew")

        self.refresh_upload_ports()
        self.refresh_current_wifi_info()
        self._build_install_tab(install_tab)
        self.refresh_install_checks()

    def _build_install_tab(self, install_tab: ttk.Frame) -> None:
        install_tab.columnconfigure(0, weight=1)
        top = ttk.Frame(install_tab)
        top.grid(row=0, column=0, sticky="ew")
        top.columnconfigure(0, weight=1)
        ttk.Button(top, text="Refresh Checks", command=self.refresh_install_checks).grid(row=0, column=0, sticky="w")

        table = ttk.LabelFrame(install_tab, text="Modules", padding=10)
        table.grid(row=1, column=0, sticky="ew", pady=(8, 0))
        table.columnconfigure(0, weight=0)
        table.columnconfigure(1, weight=1)
        table.columnconfigure(2, weight=0)
        table.columnconfigure(3, weight=0)

        ttk.Label(table, text="Module").grid(row=0, column=0, sticky="w")
        ttk.Label(table, text="Status").grid(row=0, column=1, sticky="w")
        ttk.Label(table, text="Action").grid(row=0, column=2, sticky="w", padx=(8, 0))
        ttk.Label(table, text="More").grid(row=0, column=3, sticky="w", padx=(6, 0))
        ttk.Separator(table).grid(row=1, column=0, columnspan=4, sticky="ew", pady=(6, 8))

        r = 2
        ttk.Label(table, text="Python").grid(row=r, column=0, sticky="w", pady=(0, 4))
        ttk.Label(table, textvariable=self.check_python_var).grid(row=r, column=1, sticky="w", pady=(0, 4))
        ttk.Button(table, text="Check", command=self.refresh_install_checks).grid(row=r, column=2, sticky="ew", padx=(8, 0), pady=(0, 4))
        self._btn_open_python_location = ttk.Button(table, text="Open File Location", command=self.open_python_file_location)
        self._btn_open_python_location.grid(row=r, column=3, sticky="ew", padx=(6, 0), pady=(0, 4))

        r += 1
        ttk.Label(table, text=".venv + Bridge Deps").grid(row=r, column=0, sticky="w", pady=(0, 4))
        deps_row = ttk.Frame(table)
        deps_row.grid(row=r, column=1, sticky="w", pady=(0, 4))
        ttk.Label(deps_row, textvariable=self.check_venv_var).grid(row=0, column=0, sticky="w")
        ttk.Label(deps_row, text=" | ").grid(row=0, column=1, sticky="w")
        ttk.Label(deps_row, textvariable=self.check_deps_var).grid(row=0, column=2, sticky="w")
        self._btn_setup_deps = ttk.Button(table, text="Setup", command=self.install_venv_and_deps)
        self._btn_setup_deps.grid(row=r, column=2, sticky="ew", padx=(8, 0), pady=(0, 4))
        ttk.Label(table, text="-").grid(row=r, column=3, sticky="w", padx=(6, 0), pady=(0, 4))

        r += 1
        ttk.Label(table, text="PlatformIO").grid(row=r, column=0, sticky="w", pady=(0, 4))
        ttk.Label(table, textvariable=self.check_pio_var).grid(row=r, column=1, sticky="w", pady=(0, 4))
        self._btn_install_pio = ttk.Button(table, text="Install", command=self.install_platformio_core)
        self._btn_install_pio.grid(row=r, column=2, sticky="ew", padx=(8, 0), pady=(0, 4))
        ttk.Label(table, text="-").grid(row=r, column=3, sticky="w", padx=(6, 0), pady=(0, 4))

        r += 1
        ttk.Label(table, text="ngrok").grid(row=r, column=0, sticky="w", pady=(0, 4))
        ttk.Label(table, textvariable=self.check_ngrok_var).grid(row=r, column=1, sticky="w", pady=(0, 4))
        self._btn_install_ngrok = ttk.Button(table, text="Install", command=self.install_ngrok_cli)
        self._btn_install_ngrok.grid(row=r, column=2, sticky="ew", padx=(8, 0), pady=(0, 4))
        ttk.Label(table, text="-").grid(row=r, column=3, sticky="w", padx=(6, 0), pady=(0, 4))

        r += 1
        ttk.Label(table, text="Bridge").grid(row=r, column=0, sticky="w", pady=(2, 0))
        ttk.Label(table, textvariable=self.service_bridge_var).grid(row=r, column=1, sticky="w", pady=(2, 0))
        bridge_actions = ttk.Frame(table)
        bridge_actions.grid(row=r, column=2, columnspan=2, sticky="w", padx=(8, 0), pady=(2, 0))
        self._btn_bridge_toggle = ttk.Button(bridge_actions, text="Start", command=self.toggle_bridge_only)
        self._btn_bridge_toggle.grid(row=0, column=0, sticky="w")
        ttk.Button(bridge_actions, text="Restart", command=self.restart_bridge_only).grid(row=0, column=1, sticky="w", padx=(6, 0))

        r += 1
        ttk.Label(table, text="ngrok Runtime").grid(row=r, column=0, sticky="w", pady=(2, 0))
        ttk.Label(table, textvariable=self.service_ngrok_var).grid(row=r, column=1, sticky="w", pady=(2, 0))
        ngrok_actions = ttk.Frame(table)
        ngrok_actions.grid(row=r, column=2, columnspan=2, sticky="w", padx=(8, 0), pady=(2, 0))
        self._btn_ngrok_toggle = ttk.Button(ngrok_actions, text="Start", command=self.toggle_ngrok_only)
        self._btn_ngrok_toggle.grid(row=0, column=0, sticky="w")
        ttk.Button(ngrok_actions, text="Restart", command=self.restart_ngrok_only).grid(row=0, column=1, sticky="w", padx=(6, 0))

        r += 1
        ttk.Label(table, text="Mosquitto Service").grid(row=r, column=0, sticky="w", pady=(2, 0))
        ttk.Label(table, textvariable=self.service_mosquitto_var).grid(row=r, column=1, sticky="w", pady=(2, 0))
        mqtt_actions = ttk.Frame(table)
        mqtt_actions.grid(row=r, column=2, columnspan=2, sticky="w", padx=(8, 0), pady=(2, 0))
        self._btn_mosquitto_toggle = ttk.Button(mqtt_actions, text="Start", command=self.toggle_mosquitto_service)
        self._btn_mosquitto_toggle.grid(row=0, column=0, sticky="w")
        ttk.Button(mqtt_actions, text="Restart", command=self.restart_mosquitto_service).grid(row=0, column=1, sticky="w", padx=(6, 0))
        ttk.Button(mqtt_actions, text="Automatic", command=lambda: self.set_mosquitto_startup("automatic")).grid(row=0, column=2, sticky="w", padx=(10, 0))
        ttk.Button(mqtt_actions, text="Manual", command=lambda: self.set_mosquitto_startup("manual")).grid(row=0, column=3, sticky="w", padx=(6, 0))
        ttk.Button(mqtt_actions, text="Disabled", command=lambda: self.set_mosquitto_startup("disabled")).grid(row=0, column=4, sticky="w", padx=(6, 0))

        action = ttk.LabelFrame(install_tab, text="Action Log", padding=10)
        action.grid(row=2, column=0, sticky="ew", pady=(10, 0))
        action.columnconfigure(0, weight=1)
        ttk.Label(action, textvariable=self.install_action_var).grid(row=0, column=0, sticky="w")

    def _venv_python(self) -> Path:
        # Launcher is intended to be started by .venv pythonw, but don't assume.
        if os.name == "nt":
            return ROOT / ".venv" / "Scripts" / "python.exe"
        # Ubuntu: python in venv bin/
        return ROOT / ".venv" / "bin" / "python3"

    def _logfile(self, name: str) -> Path:
        ts = time.strftime("%Y%m%d-%H%M%S")
        return LOG_DIR / f"{name}-{ts}.log"

    def start(self) -> None:
        if not ENV_PATH.exists():
            messagebox.showerror("Missing .env", f"Missing {ENV_PATH}\nCreate it from .env.example first.")
            return

        # Reload env (user may have edited it in UI)
        self.env = read_env(ENV_PATH)
        self.http_port = int(self.env.get("HTTP_PORT", "8080") or "8080")

        py = self._venv_python()
        if not py.exists():
            messagebox.showerror("Missing venv", f"Missing venv python:\n{py}\nCreate .venv and install requirements.txt.")
            return

        try:
            subprocess.check_call([str(py), "-c", "import fastapi,uvicorn,requests,paho.mqtt.client as mqtt"])
        except Exception:
            messagebox.showerror(
                "Missing deps",
                "Python deps missing in .venv.\nRun:\n  Windows: tools\\line_bridge\\run.cmd\n  Linux:   tools/line_bridge/run.sh\nOr install manually:\n  python -m pip install -r requirements.txt",
            )
            return

        if not shutil.which("ngrok"):
            messagebox.showerror("Missing ngrok", "ngrok not found in PATH.\nInstall ngrok and make sure `ngrok` works.")
            return

        ngrok_token = self.env.get("NGROK_AUTHTOKEN", "").strip()
        if not ngrok_token:
            messagebox.showerror("Missing ngrok authtoken", "NGROK_AUTHTOKEN is required. Save it in Config tab first.")
            return
        try:
            subprocess.check_call(["ngrok", "config", "add-authtoken", ngrok_token])
        except Exception as e:
            messagebox.showerror("ngrok authtoken", f"Failed to apply NGROK_AUTHTOKEN.\n{e}")
            return

        # Free port (best effort)
        for pid in port_listeners(self.http_port):
            taskkill(pid)

        # Start bridge (no console window) and log output
        if self.bridge_proc is None or self.bridge_proc.poll() is not None:
            bridge_log = self._logfile("bridge")
            f = open(bridge_log, "a", encoding="utf-8", errors="replace")
            popen_kwargs: dict[str, object] = {"cwd": str(ROOT), "stdout": f, "stderr": subprocess.STDOUT}
            if os.name == "nt":
                popen_kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
            else:
                popen_kwargs["start_new_session"] = True
            self.bridge_proc = subprocess.Popen(
                [str(py), "bridge.py"],
                **popen_kwargs,
            )

        # Start ngrok (no console window) and log output
        if self.ngrok_proc is None or self.ngrok_proc.poll() is not None:
            ngrok_log = self._logfile("ngrok")
            f = open(ngrok_log, "a", encoding="utf-8", errors="replace")
            popen_kwargs = {"cwd": str(ROOT), "stdout": f, "stderr": subprocess.STDOUT}
            if os.name == "nt":
                popen_kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
            else:
                popen_kwargs["start_new_session"] = True
            self.ngrok_proc = subprocess.Popen(
                ["ngrok", "http", str(self.http_port)],
                **popen_kwargs,
            )

        self.status_var.set("Running")

    def stop(self) -> None:
        # Stop by PID if we own it, and also free port just in case.
        if self.bridge_proc is not None and self.bridge_proc.poll() is None:
            taskkill(self.bridge_proc.pid)
        self.bridge_proc = None

        if self.ngrok_proc is not None and self.ngrok_proc.poll() is None:
            taskkill(self.ngrok_proc.pid)
        self.ngrok_proc = None

        for pid in port_listeners(self.http_port):
            taskkill(pid)

        if os.name == "nt":
            subprocess.run(["taskkill", "/IM", "ngrok.exe", "/F"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.status_var.set("Stopped")

    def _tick(self) -> None:
        # Health
        try:
            j = http_get_json(f"http://127.0.0.1:{self.http_port}/health", timeout_s=0.8)
            ready = j.get("ready")
            probs = j.get("problems", [])
            self.health_var.set(f"Health: ready={ready} problems={probs}")
        except Exception:
            self.health_var.set("Health: (not reachable)")

        # ngrok
        try:
            data = http_get_json("http://127.0.0.1:4040/api/tunnels", timeout_s=0.8)
            https_url = ""
            for t in data.get("tunnels", []):
                if t.get("proto") == "https":
                    https_url = str(t.get("public_url") or "")
                    break
            if https_url:
                self.ngrok_var.set(f"ngrok: {https_url}")
                self.webhook_var.set(f"Webhook: {https_url}/line/webhook")
            else:
                self.ngrok_var.set("ngrok: (no https tunnel yet)")
                self.webhook_var.set("Webhook: (unknown)")
        except Exception:
            self.ngrok_var.set("ngrok: (not reachable)")
            self.webhook_var.set("Webhook: (unknown)")

        self.refresh_runtime_services_status()
        self.root.after(900, self._tick)

    def _platformio_base_cmd(self) -> list[str]:
        if shutil.which("platformio"):
            return ["platformio"]
        if shutil.which("pio"):
            return ["pio"]
        if shutil.which("python"):
            return ["python", "-m", "platformio"]
        if shutil.which("python3"):
            return ["python3", "-m", "platformio"]
        return []

    def _host_python_cmd(self) -> list[str]:
        if sys.executable:
            return [sys.executable]
        if shutil.which("python"):
            return ["python"]
        if shutil.which("python3"):
            return ["python3"]
        return []

    def _resolved_host_python_path(self) -> str:
        cmd = self._host_python_cmd()
        if not cmd:
            return ""
        candidate = cmd[0]
        if os.path.isabs(candidate) and Path(candidate).exists():
            return str(Path(candidate))
        resolved = shutil.which(candidate)
        return str(Path(resolved)) if resolved else ""

    def refresh_install_checks(self) -> None:
        host_py = self._resolved_host_python_path()
        self.check_python_var.set("OK" if host_py else "Missing")

        vpy = self._venv_python()
        self.check_venv_var.set("OK" if vpy.exists() else "Missing")

        deps_ok = False
        if vpy.exists():
            try:
                subprocess.check_call(
                    [str(vpy), "-c", "import fastapi,uvicorn,requests,paho.mqtt.client as mqtt"],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                )
                deps_ok = True
            except Exception:
                deps_ok = False
        self.check_deps_var.set("OK" if deps_ok else "Missing")

        self.check_pio_var.set("OK" if bool(self._platformio_base_cmd()) else "Missing")
        self.check_ngrok_var.set("OK" if bool(shutil.which("ngrok")) else "Missing")
        if self._btn_open_python_location is not None:
            self._btn_open_python_location.configure(state="normal" if host_py else "disabled")
        self._set_install_button_state(self._btn_setup_deps, installed=(vpy.exists() and deps_ok), base_label="Setup")
        self._set_install_button_state(self._btn_install_pio, installed=bool(self._platformio_base_cmd()), base_label="Install")
        self._set_install_button_state(self._btn_install_ngrok, installed=bool(shutil.which("ngrok")), base_label="Install")
        self.refresh_runtime_services_status()

    def open_python_file_location(self) -> None:
        py_path = self._resolved_host_python_path()
        if not py_path:
            messagebox.showerror("Python", "Python path not found.")
            return
        p = Path(py_path)
        if os.name == "nt":
            subprocess.Popen(["explorer", "/select,", str(p)])
            return
        folder = p.parent
        if shutil.which("xdg-open"):
            subprocess.Popen(["xdg-open", str(folder)])
            return
        messagebox.showinfo("Python", str(folder))

    def _set_install_button_state(self, btn: ttk.Button | None, installed: bool, base_label: str) -> None:
        if btn is None:
            return
        if installed:
            btn.configure(text="Installed", state="disabled")
        else:
            btn.configure(text=base_label, state="normal")

    def refresh_runtime_services_status(self) -> None:
        # Bridge runtime (FastAPI health endpoint)
        try:
            j = http_get_json(f"http://127.0.0.1:{self.http_port}/health", timeout_s=0.6)
            self.service_bridge_var.set("Running" if bool(j.get("ready")) else "Running (degraded)")
        except Exception:
            self.service_bridge_var.set("Stopped")

        # ngrok runtime (local inspector API)
        try:
            _ = http_get_json("http://127.0.0.1:4040/api/tunnels", timeout_s=0.6)
            self.service_ngrok_var.set("Running")
        except Exception:
            self.service_ngrok_var.set("Stopped")

        state, startup = self._query_windows_service("mosquitto")
        if state == "Unsupported":
            self.service_mosquitto_var.set("Unsupported on this OS")
        elif state == "Not found":
            self.service_mosquitto_var.set("Not found")
        else:
            self.service_mosquitto_var.set(f"{state} | {startup}")
        self._update_service_toggle_labels()

    def _update_service_toggle_labels(self) -> None:
        bridge_running = self.service_bridge_var.get().startswith("Running")
        ngrok_running = self.service_ngrok_var.get().startswith("Running")
        mosq_running = self.service_mosquitto_var.get().startswith("Running")
        if self._btn_bridge_toggle is not None:
            self._btn_bridge_toggle.configure(text="Stop" if bridge_running else "Start")
        if self._btn_ngrok_toggle is not None:
            self._btn_ngrok_toggle.configure(text="Stop" if ngrok_running else "Start")
        if self._btn_mosquitto_toggle is not None:
            self._btn_mosquitto_toggle.configure(text="Stop" if mosq_running else "Start")

    def _query_windows_service(self, name: str) -> tuple[str, str]:
        if os.name != "nt":
            return ("Unsupported", "Unsupported")
        try:
            q = subprocess.check_output(["sc", "query", name], text=True, errors="replace", stderr=subprocess.STDOUT)
            if "FAILED 1060" in q:
                return ("Not found", "Unknown")
            state = "Unknown"
            for ln in q.splitlines():
                if "STATE" not in ln:
                    continue
                if "RUNNING" in ln:
                    state = "Running"
                elif "STOPPED" in ln:
                    state = "Stopped"
                else:
                    state = ln.split(":", 1)[-1].strip()
                break

            qc = subprocess.check_output(["sc", "qc", name], text=True, errors="replace", stderr=subprocess.STDOUT)
            startup = "Unknown"
            for ln in qc.splitlines():
                if "START_TYPE" not in ln:
                    continue
                if "AUTO_START" in ln:
                    startup = "Automatic"
                elif "DEMAND_START" in ln:
                    startup = "Manual"
                elif "DISABLED" in ln:
                    startup = "Disabled"
                break
            return (state, startup)
        except Exception:
            return ("Query failed", "Unknown")

    def _run_sc_mosquitto(self, args: list[str], action_name: str) -> None:
        if os.name != "nt":
            messagebox.showerror("Windows only", "Mosquitto service control is available on Windows only.")
            return
        try:
            subprocess.check_call(["sc"] + args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            self.install_action_var.set(f"{action_name}: mosquitto")
        except Exception as e:
            messagebox.showerror("Mosquitto service", f"{action_name} failed.\n{e}")
        finally:
            self.refresh_runtime_services_status()

    def start_mosquitto_service(self) -> None:
        self._run_sc_mosquitto(["start", "mosquitto"], "Start")

    def stop_mosquitto_service(self) -> None:
        self._run_sc_mosquitto(["stop", "mosquitto"], "Stop")

    def restart_mosquitto_service(self) -> None:
        self.stop_mosquitto_service()
        time.sleep(0.5)
        self.start_mosquitto_service()

    def set_mosquitto_startup(self, mode: str) -> None:
        mapping = {"automatic": "auto", "manual": "demand", "disabled": "disabled"}
        m = mapping.get(mode.lower().strip())
        if not m:
            return
        self._run_sc_mosquitto(["config", "mosquitto", f"start= {m}"], f"Set startup {mode}")

    def start_bridge_only(self) -> None:
        if not ENV_PATH.exists():
            messagebox.showerror("Missing .env", f"Missing {ENV_PATH}\nCreate it from .env.example first.")
            return
        self.env = read_env(ENV_PATH)
        self.http_port = int(self.env.get("HTTP_PORT", "8080") or "8080")
        py = self._venv_python()
        if not py.exists():
            messagebox.showerror("Missing venv", f"Missing venv python:\n{py}")
            return
        try:
            subprocess.check_call([str(py), "-c", "import fastapi,uvicorn,requests,paho.mqtt.client as mqtt"])
        except Exception:
            messagebox.showerror("Missing deps", "Python deps missing in .venv.")
            return
        for pid in port_listeners(self.http_port):
            taskkill(pid)
        if self.bridge_proc is None or self.bridge_proc.poll() is not None:
            bridge_log = self._logfile("bridge")
            f = open(bridge_log, "a", encoding="utf-8", errors="replace")
            popen_kwargs: dict[str, object] = {"cwd": str(ROOT), "stdout": f, "stderr": subprocess.STDOUT}
            if os.name == "nt":
                popen_kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
            else:
                popen_kwargs["start_new_session"] = True
            self.bridge_proc = subprocess.Popen([str(py), "bridge.py"], **popen_kwargs)
        self.install_action_var.set("Bridge started")
        self.refresh_runtime_services_status()

    def stop_bridge_only(self) -> None:
        if self.bridge_proc is not None and self.bridge_proc.poll() is None:
            taskkill(self.bridge_proc.pid)
        self.bridge_proc = None
        self.install_action_var.set("Bridge stopped")
        self.refresh_runtime_services_status()

    def restart_bridge_only(self) -> None:
        self.stop_bridge_only()
        time.sleep(0.3)
        self.start_bridge_only()

    def toggle_bridge_only(self) -> None:
        if self.service_bridge_var.get().startswith("Running"):
            self.stop_bridge_only()
        else:
            self.start_bridge_only()

    def start_ngrok_only(self) -> None:
        self.env = read_env(ENV_PATH)
        self.http_port = int(self.env.get("HTTP_PORT", "8080") or "8080")
        if not shutil.which("ngrok"):
            messagebox.showerror("Missing ngrok", "ngrok not found in PATH.")
            return
        ngrok_token = self.env.get("NGROK_AUTHTOKEN", "").strip()
        if not ngrok_token:
            messagebox.showerror("Missing ngrok authtoken", "NGROK_AUTHTOKEN is required.")
            return
        try:
            subprocess.check_call(["ngrok", "config", "add-authtoken", ngrok_token])
        except Exception as e:
            messagebox.showerror("ngrok authtoken", f"Failed to apply NGROK_AUTHTOKEN.\n{e}")
            return
        if self.ngrok_proc is None or self.ngrok_proc.poll() is not None:
            ngrok_log = self._logfile("ngrok")
            f = open(ngrok_log, "a", encoding="utf-8", errors="replace")
            popen_kwargs: dict[str, object] = {"cwd": str(ROOT), "stdout": f, "stderr": subprocess.STDOUT}
            if os.name == "nt":
                popen_kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
            else:
                popen_kwargs["start_new_session"] = True
            self.ngrok_proc = subprocess.Popen(["ngrok", "http", str(self.http_port)], **popen_kwargs)
        self.install_action_var.set("ngrok started")
        self.refresh_runtime_services_status()

    def stop_ngrok_only(self) -> None:
        if self.ngrok_proc is not None and self.ngrok_proc.poll() is None:
            taskkill(self.ngrok_proc.pid)
        self.ngrok_proc = None
        if os.name == "nt":
            subprocess.run(["taskkill", "/IM", "ngrok.exe", "/F"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.install_action_var.set("ngrok stopped")
        self.refresh_runtime_services_status()

    def restart_ngrok_only(self) -> None:
        self.stop_ngrok_only()
        time.sleep(0.3)
        self.start_ngrok_only()

    def toggle_ngrok_only(self) -> None:
        if self.service_ngrok_var.get().startswith("Running"):
            self.stop_ngrok_only()
        else:
            self.start_ngrok_only()

    def toggle_mosquitto_service(self) -> None:
        if self.service_mosquitto_var.get().startswith("Running"):
            self.stop_mosquitto_service()
        else:
            self.start_mosquitto_service()

    def _run_job_async(self, title: str, commands: list[list[str]], cwd: Path | None = None) -> None:
        log_path = self._logfile("install")
        self.install_action_var.set(f"{title}: running...")

        def worker() -> None:
            rc = 0
            wd = str(cwd or ROOT)
            with open(log_path, "w", encoding="utf-8", errors="replace") as f:
                f.write(f"TITLE: {title}\nCWD: {wd}\n\n")
                for cmd in commands:
                    f.write("CMD: " + " ".join(cmd) + "\n")
                    rc = subprocess.call(cmd, cwd=wd, stdout=f, stderr=subprocess.STDOUT)
                    f.write(f"RC: {rc}\n\n")
                    if rc != 0:
                        break

            def done() -> None:
                self.refresh_install_checks()
                if rc == 0:
                    self.install_action_var.set(f"{title}: done (log: {log_path.name})")
                    messagebox.showinfo("Services", f"{title} completed.\nLog: {log_path}")
                else:
                    self.install_action_var.set(f"{title}: failed (log: {log_path.name})")
                    messagebox.showerror("Services", f"{title} failed (rc={rc}).\nLog: {log_path}")

            self.root.after(0, done)

        threading.Thread(target=worker, daemon=True).start()

    def install_venv_and_deps(self) -> None:
        py = self._host_python_cmd()
        if not py:
            messagebox.showerror("Python missing", "Cannot find python/python3.")
            return

        vpy = self._venv_python()
        req = ROOT / "requirements.txt"
        if not req.exists():
            messagebox.showerror("requirements.txt missing", f"Missing file: {req}")
            return

        cmds: list[list[str]] = []
        if not vpy.exists():
            cmds.append(py + ["-m", "venv", str(vpy.parent.parent if os.name == "nt" else vpy.parent.parent)])
        cmds.append([str(vpy), "-m", "pip", "install", "--upgrade", "pip"])
        cmds.append([str(vpy), "-m", "pip", "install", "-r", str(req)])
        self._run_job_async("Setup venv + deps", cmds, cwd=ROOT)

    def install_platformio_core(self) -> None:
        py = self._host_python_cmd()
        if not py:
            messagebox.showerror("Python missing", "Cannot find python/python3.")
            return
        self._run_job_async("Install PlatformIO", [py + ["-m", "pip", "install", "--upgrade", "platformio"]], cwd=PROJECT_ROOT)

    def install_ngrok_cli(self) -> None:
        if os.name == "nt" and shutil.which("winget"):
            cmd = [
                "winget",
                "install",
                "-e",
                "--id",
                "Ngrok.Ngrok",
                "--accept-source-agreements",
                "--accept-package-agreements",
            ]
            self._run_job_async("Install ngrok", [cmd], cwd=PROJECT_ROOT)
            return
        self.open_ngrok_download()
        self.install_action_var.set("ngrok install: opened download page (manual)")
        messagebox.showinfo("Install ngrok", "Auto install not available on this OS. Download page opened.")

    def open_ngrok_download(self) -> None:
        webbrowser.open("https://ngrok.com/downloads")

    def refresh_current_wifi_info(self) -> None:
        ssid = current_wifi_ssid().strip()
        ip = local_ip().strip()
        self.current_wifi_ssid_var.set(ssid if ssid else "(not connected)")
        self.current_wifi_ip_var.set(ip if ip else "(unknown)")

    def use_current_wifi_info(self) -> None:
        self.refresh_current_wifi_info()
        ssid = self.current_wifi_ssid_var.get().strip()
        ip = self.current_wifi_ip_var.get().strip()
        if ssid and not ssid.startswith("("):
            self.fw_wifi_ssid.set(ssid)
        if ip and not ip.startswith("("):
            self.fw_mqtt_broker.set(ip)

    def toggle_fw_wifi_password(self) -> None:
        self._show_fw_wifi_password = not self._show_fw_wifi_password
        show = "" if self._show_fw_wifi_password else "*"
        self._fw_wifi_password_entry.configure(show=show)

    def clear_fw_wifi_password(self) -> None:
        self.fw_wifi_password.set("")

    def refresh_upload_ports(self) -> None:
        self.serial_ports = detect_serial_ports()
        current = (self.fw_upload_port.get() or "").strip()

        if self.serial_ports:
            self._upload_port_box.configure(state="readonly", foreground="black")
            self._upload_port_box["values"] = self.serial_ports
            if current in self.serial_ports:
                self.fw_upload_port.set(current)
            else:
                self.fw_upload_port.set(self.serial_ports[0])
            return

        self._upload_port_box.configure(state="disabled", foreground="gray")
        self._upload_port_box["values"] = ()
        self.fw_upload_port.set("no port found")

    def save_firmware_env(self, notify: bool = True) -> bool:
        try:
            port = int(self.fw_mqtt_port.get().strip() or "1883")
            if port <= 0 or port > 65535:
                raise ValueError
        except Exception:
            messagebox.showerror("Invalid MQTT Port", "MQTT port must be an integer 1..65535")
            return False

        lines = read_env_lines(ENV_PATH)
        if not lines:
            lines = []

        lines = upsert_env_kv(lines, "FW_WIFI_SSID", self.fw_wifi_ssid.get().strip())
        lines = upsert_env_kv(lines, "FW_WIFI_PASSWORD", self.fw_wifi_password.get().strip())
        lines = upsert_env_kv(lines, "FW_MQTT_BROKER", self.fw_mqtt_broker.get().strip())
        lines = upsert_env_kv(lines, "FW_MQTT_PORT", str(port))
        lines = upsert_env_kv(lines, "FW_MQTT_USERNAME", self.fw_mqtt_username.get().strip())
        lines = upsert_env_kv(lines, "FW_MQTT_PASSWORD", self.fw_mqtt_password.get().strip())
        lines = upsert_env_kv(lines, "FW_MQTT_CLIENT_ID", self.fw_mqtt_client_id.get().strip() or "embedded-security-esp32")
        upload_port = self.fw_upload_port.get().strip()
        if upload_port not in self.serial_ports:
            upload_port = ""
        lines = upsert_env_kv(lines, "FW_UPLOAD_PORT", upload_port)

        write_env_lines(ENV_PATH, lines)
        self.env = read_env(ENV_PATH)
        if notify:
            messagebox.showinfo("Saved", f"Firmware fields saved to {ENV_PATH}")
        return True

    def _run_platformio_async(self, args: list[str], success_msg: str) -> None:
        base = self._platformio_base_cmd()
        if not base:
            messagebox.showerror("PlatformIO not found", "Cannot find platformio/pio/python in PATH.")
            return

        log_path = self._logfile("platformio")
        cmd = base + [
            "run",
            "-c",
            str(PLATFORMIO_INI_PATH),
            "-e",
            (self.env.get("FW_ENV", "") or DEFAULT_FW_ENV).strip() or DEFAULT_FW_ENV,
        ] + args

        def worker() -> None:
            with open(log_path, "w", encoding="utf-8", errors="replace") as f:
                f.write("CMD: " + " ".join(cmd) + "\n\n")
                rc = subprocess.call(cmd, cwd=str(PROJECT_ROOT), stdout=f, stderr=subprocess.STDOUT)

            def done() -> None:
                if rc == 0:
                    messagebox.showinfo("PlatformIO", f"{success_msg}\nLog: {log_path}")
                else:
                    messagebox.showerror("PlatformIO", f"Command failed (rc={rc}).\nLog: {log_path}")

            self.root.after(0, done)

        threading.Thread(target=worker, daemon=True).start()

    def build_firmware(self) -> None:
        if not self.save_firmware_env(notify=False):
            return
        self._run_platformio_async([], "Build completed")

    def upload_firmware(self) -> None:
        if not self.save_firmware_env(notify=False):
            return
        extra: list[str] = ["-t", "upload"]
        port = self.fw_upload_port.get().strip()
        if port not in self.serial_ports:
            port = ""
        if port:
            extra += ["--upload-port", port]
        self._run_platformio_async(extra, "Upload completed")

    def toggle_secrets(self) -> None:
        self._show_secrets = not self._show_secrets
        show = "" if self._show_secrets else "*"
        self._token_entry.configure(show=show)
        self._secret_entry.configure(show=show)
        self._ngrok_entry.configure(show=show)

    def save_env(self) -> None:
        # Minimal validation: keep it easy to use.
        try:
            port = int(self.cfg_http_port.get().strip() or "8080")
            if port <= 0 or port > 65535:
                raise ValueError
        except Exception:
            messagebox.showerror("Invalid HTTP_PORT", "HTTP_PORT must be an integer 1..65535")
            return

        lines = read_env_lines(ENV_PATH)
        if not lines:
            # Create a basic file if missing/empty.
            lines = []

        lines = upsert_env_kv(lines, "HTTP_PORT", str(port))

        lines = upsert_env_kv(lines, "LINE_CHANNEL_ACCESS_TOKEN", self.cfg_line_token.get().strip())
        lines = upsert_env_kv(lines, "LINE_CHANNEL_SECRET", self.cfg_line_secret.get().strip())

        lines = upsert_env_kv(lines, "LINE_TARGET_USER_ID", self.cfg_line_target_user.get().strip())
        lines = upsert_env_kv(lines, "LINE_TARGET_GROUP_ID", self.cfg_line_target_group.get().strip())
        lines = upsert_env_kv(lines, "LINE_TARGET_ROOM_ID", self.cfg_line_target_room.get().strip())

        ngrok_token = self.cfg_ngrok_authtoken.get().strip()
        if not ngrok_token:
            messagebox.showerror("Missing ngrok authtoken", "ngrok authtoken is required.")
            return
        lines = upsert_env_kv(lines, "NGROK_AUTHTOKEN", ngrok_token)

        write_env_lines(ENV_PATH, lines)
        # Keep UI actions (Start/Health) aligned with saved config without requiring restart.
        self.env = read_env(ENV_PATH)
        self.http_port = port
        messagebox.showinfo("Saved", f"Updated {ENV_PATH}\nRestart Bridge to apply changes.")

    def set_ngrok_authtoken(self) -> None:
        tok = self.cfg_ngrok_authtoken.get().strip()
        if not tok:
            messagebox.showinfo("ngrok Authtoken", "Paste an authtoken first.")
            return
        try:
            subprocess.check_call(["ngrok", "config", "add-authtoken", tok])
            messagebox.showinfo("ngrok Authtoken", "Authtoken set. Restart ngrok to apply.")
        except Exception as e:
            messagebox.showerror("ngrok Authtoken", f"Failed to set authtoken.\n{e}")

    def open_env_folder(self) -> None:
        webbrowser.open(str(ROOT))

    def copy_webhook(self) -> None:
        val = self.webhook_var.get()
        if "Webhook:" not in val or "/line/webhook" not in val:
            messagebox.showinfo("Webhook", "Webhook URL not available yet. Start ngrok first.")
            return
        url = val.split("Webhook:", 1)[1].strip()
        self.root.clipboard_clear()
        self.root.clipboard_append(url)
        self.root.update()

    def open_health(self) -> None:
        webbrowser.open(f"http://127.0.0.1:{self.http_port}/health")

    def open_inspector(self) -> None:
        webbrowser.open("http://127.0.0.1:4040")

    def _on_close(self) -> None:
        # Keep processes running when UI closes (more convenient).
        self.root.destroy()

    def run(self) -> None:
        self.root.mainloop()


def main() -> int:
    app = LauncherApp()
    app.run()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
