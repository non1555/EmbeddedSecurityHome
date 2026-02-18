#!/usr/bin/env python3
import json
import os
import platform
import re
import shlex
import shutil
import ctypes
import signal
import socket
import subprocess
import sys
import threading
import time
import urllib.request
import webbrowser
from pathlib import Path
from tkinter import Tk, StringVar, ttk, messagebox, Canvas
from glob import glob


ROOT = Path(__file__).resolve().parent
PROJECT_ROOT = ROOT.parent.parent
ENV_PATH = ROOT / ".env"
PLATFORMIO_INI_PATH = PROJECT_ROOT / "platformio.ini"
NGROK_BUNDLE_DIR = PROJECT_ROOT / "tools" / "ngrok"
LOG_DIR = ROOT / "logs"
LOG_DIR.mkdir(exist_ok=True)
DEFAULT_FW_ENV = "esp32doit-devkit-v1"
PREFERRED_FW_ENVS = ("main-board", "automation-board")
MAIN_BOARD_ENV = "main-board"
AUTOMATION_BOARD_ENV = "automation-board"
PIO_VENV_DIR = PROJECT_ROOT / ".venv_pio"
MOSQUITTO_LAN_CONF_PATH = Path("/etc/mosquitto/conf.d/securityhome-lan.conf")
MOSQUITTO_LAN_CONF_TEXT = (
    "# Managed by SecurityHome Launcher\n"
    "listener 1883 0.0.0.0\n"
    "allow_anonymous true\n"
)


def parse_platformio_envs(path: Path) -> tuple[list[str], dict[str, str]]:
    envs: list[str] = []
    details: dict[str, str] = {}
    if not path.exists():
        return envs, details

    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    pending_comments: list[str] = []
    current_env = ""

    for raw in lines:
        line = raw.strip()
        env_match = re.match(r"^\[env:([^\]]+)\]$", line)
        if env_match:
            current_env = env_match.group(1).strip()
            if current_env:
                envs.append(current_env)
                if pending_comments:
                    details[current_env] = " ".join(pending_comments)
            pending_comments = []
            continue

        if not line:
            pending_comments = []
            continue

        if line.startswith(";") or line.startswith("#"):
            txt = line.lstrip(";#").strip()
            if txt:
                pending_comments.append(txt)
            continue

        pending_comments = []
        if not current_env or "=" not in line:
            continue

        k, v = line.split("=", 1)
        key = k.strip().lower()
        val = v.strip()
        if key == "board":
            prev = details.get(current_env, "")
            details[current_env] = f"{prev} board={val}".strip()
        elif key == "build_src_filter":
            prev = details.get(current_env, "")
            details[current_env] = f"{prev} src={val}".strip()

    return envs, details


def select_fw_env_options(all_envs: list[str]) -> list[str]:
    preferred = [e for e in PREFERRED_FW_ENVS if e in all_envs]
    if preferred:
        return preferred
    return all_envs if all_envs else [DEFAULT_FW_ENV]


def normalize_fw_env_name(name: str) -> str:
    raw = (name or "").strip()
    aliases = {
        "prod": "main-board",
        "automation": "automation-board",
    }
    return aliases.get(raw, raw)


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


def hidden_proc_kwargs() -> dict[str, object]:
    if os.name == "nt":
        return {"creationflags": subprocess.CREATE_NO_WINDOW}
    return {}


def port_listeners(port: int) -> list[int]:
    if os.name == "nt":
        # Parse netstat output for LISTENING PID(s) on :port
        try:
            out = subprocess.check_output(["netstat", "-ano"], text=True, errors="replace", **hidden_proc_kwargs())
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
                    **hidden_proc_kwargs(),
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


def is_serial_port_usable(port: str) -> bool:
    p = (port or "").strip()
    if not p:
        return False
    try:
        import serial  # type: ignore

        s = serial.Serial(port=p, baudrate=115200, timeout=0.2)
        s.close()
        return True
    except Exception:
        return False


def can_validate_serial_ports() -> bool:
    try:
        import serial  # type: ignore

        return True
    except Exception:
        return False


def current_wifi_ssid() -> str:
    if os.name == "nt":
        try:
            out = subprocess.check_output(["netsh", "wlan", "show", "interfaces"], text=True, errors="replace", **hidden_proc_kwargs())
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


def ngrok_config_has_authtoken() -> bool:
    candidates: list[Path] = []
    if os.name == "nt":
        localapp = os.environ.get("LOCALAPPDATA", "").strip()
        if localapp:
            candidates.append(Path(localapp) / "ngrok" / "ngrok.yml")
    home = Path.home()
    candidates.append(home / ".config" / "ngrok" / "ngrok.yml")
    candidates.append(home / ".ngrok2" / "ngrok.yml")
    for p in candidates:
        try:
            if not p.exists():
                continue
            txt = p.read_text(encoding="utf-8", errors="replace")
            if "authtoken:" in txt and "authtoken: \"\"" not in txt and "authtoken: ''" not in txt:
                return True
        except Exception:
            continue
    return False


class LauncherApp:
    def __init__(self) -> None:
        self.root = Tk()
        self.root.title("EmbeddedSecurity LINE Bridge")
        self.root.resizable(True, True)
        self.root.configure(bg="#f4f7fb")
        self._fit_window_to_screen()

        self._canvas: Canvas | None = None
        self._canvas_window: int | None = None

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
        default_main_cid = (self.env.get("FW_MQTT_CLIENT_ID", "embedded-security-esp32") or "embedded-security-esp32").strip()
        default_auto_cid = (self.env.get("FW_MQTT_CLIENT_ID_AUTOMATION", "") or f"{default_main_cid}-auto").strip()
        if not default_auto_cid or default_auto_cid == default_main_cid:
            default_auto_cid = f"{default_main_cid}-auto"
        self.fw_mqtt_client_id_main = StringVar(value=default_main_cid)
        self.fw_mqtt_client_id_auto = StringVar(value=default_auto_cid)
        self.fw_cmd_token = StringVar(value=self.env.get("FW_CMD_TOKEN", ""))
        self.fw_upload_port = StringVar(value=self.env.get("FW_UPLOAD_PORT", ""))
        all_envs, self.fw_env_details = parse_platformio_envs(PLATFORMIO_INI_PATH)
        self.fw_env_options = select_fw_env_options(all_envs)
        fallback_fw_env = normalize_fw_env_name((self.env.get("FW_ENV", "") or DEFAULT_FW_ENV).strip() or DEFAULT_FW_ENV)
        default_fw_build_env = normalize_fw_env_name((self.env.get("FW_ENV_BUILD", "") or fallback_fw_env).strip() or fallback_fw_env)
        default_fw_upload_env = normalize_fw_env_name((self.env.get("FW_ENV_UPLOAD", "") or fallback_fw_env).strip() or fallback_fw_env)
        if default_fw_build_env not in self.fw_env_options:
            default_fw_build_env = self.fw_env_options[0]
        if default_fw_upload_env not in self.fw_env_options:
            default_fw_upload_env = self.fw_env_options[0]
        self.fw_build_env = StringVar(value=default_fw_build_env)
        self.fw_upload_env = StringVar(value=default_fw_upload_env)
        self.fw_build_env_desc = StringVar(value="")
        self.fw_upload_env_desc = StringVar(value="")
        self.serial_ports: list[str] = []
        self.current_wifi_ssid_var = StringVar(value="")
        self.current_wifi_ip_var = StringVar(value="")
        self._show_fw_wifi_password = False
        self._fw_build_env_box: ttk.Combobox | None = None
        self._fw_upload_env_box: ttk.Combobox | None = None

        # Services tab state
        self.check_python_var = StringVar(value="Python: (unknown)")
        self.check_venv_var = StringVar(value="Venv: (unknown)")
        self.check_deps_var = StringVar(value="Bridge Deps: (unknown)")
        self.check_pio_var = StringVar(value="PlatformIO: (unknown)")
        self.check_ngrok_var = StringVar(value="ngrok: (unknown)")
        self.admin_var = StringVar(value="Admin: (unknown)")
        self.service_bridge_var = StringVar(value="(unknown)")
        self.service_ngrok_var = StringVar(value="(unknown)")
        self.service_mosquitto_var = StringVar(value="(unknown)")
        self.service_mosquitto_listener_var = StringVar(value="(unknown)")
        self.install_action_var = StringVar(value="Ready")
        self.busy_var = StringVar(value="Idle")
        self._busy_count = 0
        self._busy_bar: ttk.Progressbar | None = None
        self._btn_setup_deps: ttk.Button | None = None
        self._btn_install_pio: ttk.Button | None = None
        self._btn_install_ngrok: ttk.Button | None = None
        self._btn_open_python_location: ttk.Button | None = None
        self._btn_bridge_toggle: ttk.Button | None = None
        self._btn_ngrok_toggle: ttk.Button | None = None
        self._btn_mosquitto_toggle: ttk.Button | None = None

        self._init_style()
        self._build_ui()
        self._tick()

        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _init_style(self) -> None:
        style = ttk.Style(self.root)
        try:
            style.theme_use("clam")
        except Exception:
            pass
        style.configure("App.TFrame", background="#f4f7fb")
        style.configure("Header.TLabel", background="#f4f7fb", foreground="#114a75", font=("Segoe UI", 16, "bold"))
        style.configure("SubHeader.TLabel", background="#f4f7fb", foreground="#4b6478", font=("Segoe UI", 9))
        style.configure("Section.TLabel", foreground="#114a75", font=("Segoe UI", 10, "bold"))
        style.configure("Primary.TButton", padding=(10, 6))
        style.configure("Accent.TButton", padding=(10, 6))
        style.map("Accent.TButton", background=[("active", "#e9f4ff")])

    def _build_ui(self) -> None:
        self.root.rowconfigure(0, weight=1)
        self.root.columnconfigure(0, weight=1)

        self._canvas = Canvas(self.root, highlightthickness=0, borderwidth=0, bg="#f4f7fb")
        vscroll = ttk.Scrollbar(self.root, orient="vertical", command=self._canvas.yview)
        self._canvas.configure(yscrollcommand=vscroll.set)
        self._canvas.grid(row=0, column=0, sticky="nsew")
        vscroll.grid(row=0, column=1, sticky="ns")

        frm = ttk.Frame(self._canvas, padding=12, style="App.TFrame")
        self._canvas_window = self._canvas.create_window((0, 0), window=frm, anchor="nw")
        frm.bind("<Configure>", self._on_content_configure)
        self._canvas.bind("<Configure>", self._on_canvas_configure)
        self._bind_scroll_shortcuts()

        frm.columnconfigure(0, weight=1)

        head = ttk.Frame(frm, style="App.TFrame")
        head.grid(row=0, column=0, sticky="ew", pady=(0, 8))
        ttk.Label(head, text="EmbeddedSecurity Control Center", style="Header.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Label(
            head,
            text="LINE bridge, firmware setup, and service control in one place",
            style="SubHeader.TLabel",
        ).grid(row=1, column=0, sticky="w", pady=(2, 0))

        nb = ttk.Notebook(frm)
        nb.grid(row=1, column=0, sticky="nsew")

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

        ttk.Button(btns, text="Start", command=self.start, style="Primary.TButton").grid(row=0, column=0, sticky="ew", padx=(0, 6))
        ttk.Button(btns, text="Stop", command=self.stop).grid(row=0, column=1, sticky="ew", padx=(0, 6))
        ttk.Button(btns, text="Copy Webhook", command=self.copy_webhook, style="Accent.TButton").grid(row=0, column=2, sticky="ew")

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
        ttk.Label(firmware_tab, text="Firmware Target", style="Section.TLabel").grid(row=fr, column=0, sticky="w", pady=(0, 6))
        fr += 1
        ttk.Label(firmware_tab, text="Build Env").grid(row=fr, column=0, sticky="w")
        self._fw_build_env_box = ttk.Combobox(firmware_tab, textvariable=self.fw_build_env, state="readonly", width=28)
        self._fw_build_env_box.grid(row=fr, column=1, sticky="w")
        self._fw_build_env_box.bind("<<ComboboxSelected>>", lambda _evt: self._update_fw_env_descriptions())
        fr += 1
        ttk.Label(firmware_tab, textvariable=self.fw_build_env_desc, foreground="#4b6478").grid(row=fr, column=1, sticky="w")
        fr += 1
        ttk.Label(firmware_tab, text="Upload Env").grid(row=fr, column=0, sticky="w")
        self._fw_upload_env_box = ttk.Combobox(firmware_tab, textvariable=self.fw_upload_env, state="readonly", width=28)
        self._fw_upload_env_box.grid(row=fr, column=1, sticky="w")
        self._fw_upload_env_box.bind("<<ComboboxSelected>>", lambda _evt: self._update_fw_env_descriptions())
        fr += 1
        ttk.Label(firmware_tab, textvariable=self.fw_upload_env_desc, foreground="#4b6478").grid(row=fr, column=1, sticky="w")
        fr += 1
        env_btns = ttk.Frame(firmware_tab)
        env_btns.grid(row=fr, column=0, columnspan=2, sticky="w", pady=(6, 0))
        ttk.Button(env_btns, text="Refresh Envs", command=self.refresh_fw_envs).grid(row=0, column=0, sticky="w")
        fr += 1

        ttk.Separator(firmware_tab).grid(row=fr, column=0, columnspan=2, sticky="ew", pady=(12, 12))
        fr += 1
        ttk.Label(firmware_tab, text="Current Network", style="Section.TLabel").grid(row=fr, column=0, sticky="w", pady=(0, 6))
        fr += 1
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
        ttk.Label(firmware_tab, text="Wi-Fi Settings", style="Section.TLabel").grid(row=fr, column=0, sticky="w", pady=(0, 6))
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
        ttk.Label(firmware_tab, text="MQTT Settings", style="Section.TLabel").grid(row=fr, column=0, sticky="w", pady=(0, 6))
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
        ttk.Label(firmware_tab, text="MQTT Client ID (Main)").grid(row=fr, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(firmware_tab, textvariable=self.fw_mqtt_client_id_main, width=36).grid(row=fr, column=1, sticky="ew", pady=(6, 0))
        fr += 1
        ttk.Label(firmware_tab, text="MQTT Client ID (Automation)").grid(row=fr, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(firmware_tab, textvariable=self.fw_mqtt_client_id_auto, width=36).grid(row=fr, column=1, sticky="ew", pady=(6, 0))
        fr += 1
        ttk.Label(firmware_tab, text="FW CMD Token").grid(row=fr, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(firmware_tab, textvariable=self.fw_cmd_token, width=36, show="*").grid(row=fr, column=1, sticky="ew", pady=(6, 0))
        fr += 1

        ttk.Separator(firmware_tab).grid(row=fr, column=0, columnspan=2, sticky="ew", pady=(12, 12))
        fr += 1
        ttk.Label(firmware_tab, text="Build & Upload", style="Section.TLabel").grid(row=fr, column=0, sticky="w", pady=(0, 6))
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
        fw_btns.columnconfigure(3, weight=1)
        fw_btns.columnconfigure(4, weight=1)

        ttk.Button(fw_btns, text="Save to .env", command=self.save_firmware_env).grid(row=0, column=0, sticky="ew", padx=(0, 6))
        ttk.Button(fw_btns, text="Build", command=self.build_firmware, style="Accent.TButton").grid(row=0, column=1, sticky="ew", padx=(0, 6))
        ttk.Button(fw_btns, text="Upload", command=self.upload_firmware, style="Primary.TButton").grid(row=0, column=2, sticky="ew", padx=(0, 6))
        ttk.Button(fw_btns, text="Deploy Main Board", command=self.deploy_main_board, style="Accent.TButton").grid(
            row=0, column=3, sticky="ew", padx=(0, 6)
        )
        ttk.Button(fw_btns, text="Deploy Automation", command=self.deploy_automation_board, style="Primary.TButton").grid(
            row=0, column=4, sticky="ew"
        )

        self.refresh_fw_envs()
        self.refresh_upload_ports()
        self.refresh_current_wifi_info()
        self._build_install_tab(install_tab)
        self.refresh_install_checks()

    def _fit_window_to_screen(self) -> None:
        sw = self.root.winfo_screenwidth()
        sh = self.root.winfo_screenheight()
        width = max(640, min(1120, sw - 80))
        height = max(520, min(820, sh - 120))
        x = max(0, (sw - width) // 2)
        y = max(0, (sh - height) // 2)
        self.root.geometry(f"{width}x{height}+{x}+{y}")
        self.root.minsize(640, 520)

    def _on_content_configure(self, _event=None) -> None:
        if self._canvas is None:
            return
        self._canvas.configure(scrollregion=self._canvas.bbox("all"))

    def _on_canvas_configure(self, event) -> None:
        if self._canvas is None or self._canvas_window is None:
            return
        self._canvas.itemconfigure(self._canvas_window, width=event.width)

    def _bind_scroll_shortcuts(self) -> None:
        self.root.bind_all("<MouseWheel>", self._on_mousewheel, add="+")
        self.root.bind_all("<Button-4>", self._on_mousewheel, add="+")
        self.root.bind_all("<Button-5>", self._on_mousewheel, add="+")

    def _on_mousewheel(self, event) -> None:
        if self._canvas is None:
            return
        if getattr(event, "num", None) == 4:
            self._canvas.yview_scroll(-3, "units")
            return
        if getattr(event, "num", None) == 5:
            self._canvas.yview_scroll(3, "units")
            return
        delta = int(getattr(event, "delta", 0))
        if delta != 0:
            steps = max(1, abs(delta) // 120)
            self._canvas.yview_scroll(-steps if delta > 0 else steps, "units")

    def _build_install_tab(self, install_tab: ttk.Frame) -> None:
        install_tab.columnconfigure(0, weight=1)
        top = ttk.Frame(install_tab)
        top.grid(row=0, column=0, sticky="ew")
        top.columnconfigure(0, weight=0)
        top.columnconfigure(1, weight=0)
        top.columnconfigure(2, weight=1)
        ttk.Button(top, text="Quick Setup", command=self.quick_setup_all, style="Primary.TButton").grid(row=0, column=0, sticky="w")
        ttk.Button(top, text="Run as Admin", command=self.relaunch_as_admin).grid(row=0, column=1, sticky="w", padx=(8, 0))
        ttk.Button(top, text="Refresh Checks", command=self.refresh_install_checks).grid(row=0, column=2, sticky="w", padx=(8, 0))

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
        ttk.Label(table, text="Windows Admin").grid(row=r, column=0, sticky="w", pady=(0, 4))
        ttk.Label(table, textvariable=self.admin_var).grid(row=r, column=1, sticky="w", pady=(0, 4))
        ttk.Label(table, text="-").grid(row=r, column=2, sticky="w", padx=(8, 0), pady=(0, 4))
        ttk.Label(table, text="-").grid(row=r, column=3, sticky="w", padx=(6, 0), pady=(0, 4))

        r += 1
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

        r += 1
        ttk.Label(table, text="Mosquitto Listener").grid(row=r, column=0, sticky="w", pady=(2, 0))
        ttk.Label(table, textvariable=self.service_mosquitto_listener_var).grid(row=r, column=1, sticky="w", pady=(2, 0))
        listener_actions = ttk.Frame(table)
        listener_actions.grid(row=r, column=2, columnspan=2, sticky="w", padx=(8, 0), pady=(2, 0))
        ttk.Button(listener_actions, text="LAN 1883", command=self.enable_mosquitto_lan_listener).grid(row=0, column=0, sticky="w")
        ttk.Button(listener_actions, text="Local only", command=self.disable_mosquitto_lan_listener).grid(row=0, column=1, sticky="w", padx=(6, 0))

        action = ttk.LabelFrame(install_tab, text="Action Log", padding=10)
        action.grid(row=2, column=0, sticky="ew", pady=(10, 0))
        action.columnconfigure(0, weight=1)
        ttk.Label(action, textvariable=self.install_action_var).grid(row=0, column=0, sticky="w")
        ttk.Label(action, textvariable=self.busy_var).grid(row=1, column=0, sticky="w", pady=(4, 0))
        self._busy_bar = ttk.Progressbar(action, mode="indeterminate")
        self._busy_bar.grid(row=2, column=0, sticky="ew", pady=(6, 0))

    def _begin_busy(self, label: str) -> None:
        self._busy_count += 1
        self.busy_var.set(f"Working: {label}")
        if self._busy_bar is not None:
            self._busy_bar.start(10)

    def _end_busy(self) -> None:
        self._busy_count = max(0, self._busy_count - 1)
        if self._busy_count == 0:
            self.busy_var.set("Idle")
            if self._busy_bar is not None:
                self._busy_bar.stop()

    def _venv_python(self) -> Path:
        # Launcher is intended to be started by .venv pythonw, but don't assume.
        if os.name == "nt":
            return ROOT / ".venv" / "Scripts" / "python.exe"
        # Ubuntu: python in venv bin/
        return ROOT / ".venv" / "bin" / "python3"

    def _platformio_venv_python(self) -> Path:
        if os.name == "nt":
            return PIO_VENV_DIR / "Scripts" / "python.exe"
        return PIO_VENV_DIR / "bin" / "python3"

    def _project_platformio_cmd(self) -> list[str]:
        if os.name == "nt":
            candidates = [
                PIO_VENV_DIR / "Scripts" / "pio.exe",
                PIO_VENV_DIR / "Scripts" / "platformio.exe",
            ]
        else:
            candidates = [
                PIO_VENV_DIR / "bin" / "pio",
                PIO_VENV_DIR / "bin" / "platformio",
            ]
        for candidate in candidates:
            if not candidate.exists() or candidate.is_dir():
                continue
            if os.name != "nt" and not os.access(candidate, os.X_OK):
                continue
            return [str(candidate)]
        return []

    def _python_has_platformio(self, python_cmd: str) -> bool:
        if not python_cmd:
            return False
        try:
            subprocess.check_call(
                [python_cmd, "-c", "import platformio"],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            return True
        except Exception:
            return False

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

        ngrok_cmd = self._ngrok_base_cmd()
        if not ngrok_cmd:
            messagebox.showerror("Missing ngrok", "ngrok not found. Install ngrok or provide tools/ngrok/ngrok(.exe).")
            return

        ngrok_token = self.env.get("NGROK_AUTHTOKEN", "").strip()
        if ngrok_token:
            try:
                subprocess.check_call(ngrok_cmd + ["config", "add-authtoken", ngrok_token])
            except Exception as e:
                messagebox.showerror("ngrok authtoken", f"Failed to apply NGROK_AUTHTOKEN.\n{e}")
                return
        elif not ngrok_config_has_authtoken():
            messagebox.showerror(
                "Missing ngrok authtoken",
                "NGROK_AUTHTOKEN is empty and no authtoken found in ngrok config.\nSave token in Config tab first.",
            )
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

        # Start ngrok (no console window) and log output.
        # If another launcher/session already has a tunnel for this port, reuse it.
        existing_ngrok_url = self._existing_ngrok_https_url_for_port(self.http_port)
        if existing_ngrok_url:
            self.ngrok_var.set(f"ngrok: {existing_ngrok_url}")
            self.webhook_var.set(f"Webhook: {existing_ngrok_url}/line/webhook")
        elif self.ngrok_proc is None or self.ngrok_proc.poll() is not None:
            ngrok_log = self._logfile("ngrok")
            f = open(ngrok_log, "a", encoding="utf-8", errors="replace")
            popen_kwargs = {"cwd": str(ROOT), "stdout": f, "stderr": subprocess.STDOUT}
            if os.name == "nt":
                popen_kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
            else:
                popen_kwargs["start_new_session"] = True
            self.ngrok_proc = subprocess.Popen(ngrok_cmd + ["http", str(self.http_port)], **popen_kwargs)

        self.status_var.set("Running")

    def stop(self) -> None:
        # Stop by PID if we own it, and also free port just in case.
        if self.bridge_proc is not None and self.bridge_proc.poll() is None:
            taskkill(self.bridge_proc.pid)
        self.bridge_proc = None

        if self.ngrok_proc is not None and self.ngrok_proc.poll() is None:
            taskkill(self.ngrok_proc.pid)
        self.ngrok_proc = None
        for pid in port_listeners(4040):
            taskkill(pid)

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
        project_cmd = self._project_platformio_cmd()
        if project_cmd:
            return project_cmd
        if shutil.which("platformio"):
            return ["platformio"]
        if shutil.which("pio"):
            return ["pio"]

        py_candidates: list[str] = []
        if sys.executable:
            py_candidates.append(sys.executable)
        for name in ("python3", "python"):
            resolved = shutil.which(name)
            if resolved:
                py_candidates.append(resolved)

        seen: set[str] = set()
        for py in py_candidates:
            if py in seen:
                continue
            seen.add(py)
            if self._python_has_platformio(py):
                return [py, "-m", "platformio"]
        return []

    def _bundled_ngrok_cmd(self) -> list[str]:
        # Prefer PATH ngrok. If unavailable, use a bundled binary if present.
        is_win = os.name == "nt"
        names = ["ngrok.exe", "ngrok"] if is_win else ["ngrok", "ngrok.exe"]
        for name in names:
            p = NGROK_BUNDLE_DIR / name
            if not p.exists() or p.is_dir():
                continue
            if is_win or os.access(p, os.X_OK):
                return [str(p)]
        return []

    def _ngrok_base_cmd(self) -> list[str]:
        if shutil.which("ngrok"):
            return ["ngrok"]
        return self._bundled_ngrok_cmd()

    def _existing_ngrok_https_url_for_port(self, port: int) -> str:
        try:
            data = http_get_json("http://127.0.0.1:4040/api/tunnels", timeout_s=0.6)
        except Exception:
            return ""

        needle = f":{int(port)}"
        fallback = ""
        for tunnel in data.get("tunnels", []):
            if not isinstance(tunnel, dict):
                continue
            cfg = tunnel.get("config", {})
            addr = str(cfg.get("addr", "") or "") if isinstance(cfg, dict) else ""
            if needle not in addr:
                continue

            public_url = str(tunnel.get("public_url") or "")
            if not public_url:
                continue
            if str(tunnel.get("proto") or "").lower() == "https":
                return public_url
            if not fallback:
                fallback = public_url
        return fallback

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

    def is_windows_admin(self) -> bool:
        if os.name != "nt":
            return False
        try:
            return bool(ctypes.windll.shell32.IsUserAnAdmin())
        except Exception:
            return False

    def relaunch_as_admin(self) -> None:
        if os.name != "nt":
            messagebox.showinfo("Run as Admin", "This action is available on Windows only.")
            return
        if self.is_windows_admin():
            messagebox.showinfo("Run as Admin", "Launcher is already running as Administrator.")
            return
        try:
            exe = sys.executable
            script = str(Path(__file__).resolve())
            rc = ctypes.windll.shell32.ShellExecuteW(None, "runas", exe, f'"{script}"', None, 1)
            if rc <= 32:
                raise RuntimeError(f"ShellExecuteW returned {rc}")
            self.root.after(150, self.root.destroy)
        except Exception as e:
            messagebox.showerror("Run as Admin", f"Unable to relaunch as Administrator.\n{e}")

    def refresh_install_checks(self) -> None:
        host_py = self._resolved_host_python_path()
        self.check_python_var.set("OK" if host_py else "Missing")
        if os.name == "nt":
            self.admin_var.set("Yes" if self.is_windows_admin() else "No")
        else:
            self.admin_var.set("N/A")

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
        ngrok_ok = bool(self._ngrok_base_cmd())
        self.check_ngrok_var.set("OK" if ngrok_ok else "Missing")
        if self._btn_open_python_location is not None:
            self._btn_open_python_location.configure(state="normal" if host_py else "disabled")
        self._set_install_button_state(self._btn_setup_deps, installed=(vpy.exists() and deps_ok), base_label="Setup")
        self._set_install_button_state(self._btn_install_pio, installed=bool(self._platformio_base_cmd()), base_label="Install")
        self._set_install_button_state(self._btn_install_ngrok, installed=ngrok_ok, base_label="Install")
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

        state, startup = self._query_mosquitto_service()
        if state == "Unsupported":
            self.service_mosquitto_var.set("Unsupported on this OS")
        elif state == "Not found":
            self.service_mosquitto_var.set("Not found")
        else:
            self.service_mosquitto_var.set(f"{state} | {startup}")
        self.service_mosquitto_listener_var.set(self._query_mosquitto_listener_mode())
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

    def _query_mosquitto_service(self) -> tuple[str, str]:
        if os.name == "nt":
            return self._query_windows_service("mosquitto")
        if os.name == "posix":
            return self._query_linux_service("mosquitto")
        return ("Unsupported", "Unsupported")

    def _query_mosquitto_listener_mode(self) -> str:
        if os.name != "posix":
            return "N/A"
        if not shutil.which("ss"):
            return "Unknown (ss missing)"
        try:
            out = subprocess.check_output(
                ["ss", "-ltnH"],
                text=True,
                errors="replace",
                stderr=subprocess.STDOUT,
            )
        except Exception:
            return "Unknown"

        has_1883 = False
        has_local = False
        has_lan = False
        for line in out.splitlines():
            if ":1883" not in line:
                continue
            has_1883 = True
            if "127.0.0.1:1883" in line or "[::1]:1883" in line:
                has_local = True
            if "0.0.0.0:1883" in line or "[::]:1883" in line or "*:1883" in line:
                has_lan = True

        if has_lan:
            return "LAN (0.0.0.0:1883)"
        if has_local:
            return "Localhost only (127.0.0.1:1883)"
        if has_1883:
            return "Custom bind on 1883"
        return "Not listening on 1883"

    def _query_linux_service(self, name: str) -> tuple[str, str]:
        if os.name != "posix":
            return ("Unsupported", "Unsupported")
        if not shutil.which("systemctl"):
            return ("Unsupported", "No systemctl")
        try:
            out = subprocess.check_output(
                ["systemctl", "show", name, "--no-pager", "--property=LoadState,ActiveState,UnitFileState"],
                text=True,
                errors="replace",
                stderr=subprocess.STDOUT,
            )
        except subprocess.CalledProcessError as e:
            msg = (e.output or "").strip().lower()
            if "not-found" in msg or "could not be found" in msg:
                return ("Not found", "Unknown")
            return ("Query failed", "Unknown")
        except Exception:
            return ("Query failed", "Unknown")

        props: dict[str, str] = {}
        for line in out.splitlines():
            if "=" not in line:
                continue
            k, v = line.split("=", 1)
            props[k.strip()] = v.strip()

        if props.get("LoadState", "") == "not-found":
            return ("Not found", "Unknown")

        active = props.get("ActiveState", "").strip().lower()
        if active == "active":
            state = "Running"
        elif active in ("inactive", "deactivating"):
            state = "Stopped"
        elif active == "failed":
            state = "Failed"
        elif active:
            state = active.capitalize()
        else:
            state = "Unknown"

        unit_state = props.get("UnitFileState", "").strip().lower()
        if unit_state.startswith("enabled"):
            startup = "Automatic"
        elif unit_state == "masked":
            startup = "Disabled"
        elif unit_state in ("disabled", "static", "indirect"):
            startup = "Manual"
        elif unit_state:
            startup = unit_state.capitalize()
        else:
            startup = "Unknown"
        return (state, startup)

    def _query_windows_service(self, name: str) -> tuple[str, str]:
        if os.name != "nt":
            return ("Unsupported", "Unsupported")
        try:
            q = subprocess.check_output(
                ["sc.exe", "query", name],
                text=True,
                errors="replace",
                stderr=subprocess.STDOUT,
                **hidden_proc_kwargs(),
            )
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

            qc = subprocess.check_output(
                ["sc.exe", "qc", name],
                text=True,
                errors="replace",
                stderr=subprocess.STDOUT,
                **hidden_proc_kwargs(),
            )
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
            # If starting from disabled state, switch to manual first.
            if action_name.lower().strip() == "start":
                try:
                    qc = subprocess.check_output(["sc.exe", "qc", "mosquitto"], text=True, errors="replace", stderr=subprocess.STDOUT)
                    if "START_TYPE" in qc and "DISABLED" in qc:
                        subprocess.check_output(["sc.exe", "config", "mosquitto", "start= demand"], text=True, errors="replace", stderr=subprocess.STDOUT)
                except Exception:
                    pass

            out = subprocess.check_output(["sc.exe"] + args, text=True, errors="replace", stderr=subprocess.STDOUT)
            self.install_action_var.set(f"{action_name}: mosquitto")
            if out.strip():
                self.install_action_var.set(f"{action_name}: mosquitto ({out.splitlines()[-1].strip()})")
        except subprocess.CalledProcessError as e:
            msg = (e.output or "").strip()
            if "Access is denied" in msg and not self.is_windows_admin():
                ask = messagebox.askyesno(
                    "Administrator Required",
                    "Windows denied service control.\nRun this Mosquitto action with Administrator permission now?",
                )
                if ask:
                    ok = self._run_sc_mosquitto_elevated(args, action_name)
                    if ok:
                        self.install_action_var.set(f"{action_name}: mosquitto (elevated)")
                    else:
                        messagebox.showerror(
                            "Mosquitto service",
                            f"{action_name} failed in elevated mode.\nPlease allow UAC prompt and try again.",
                        )
                    return
            if "Access is denied" in msg:
                msg = msg + "\nTip: click Run as Admin or allow elevated action prompt."
            messagebox.showerror("Mosquitto service", f"{action_name} failed.\n{msg or e}")
        except Exception as e:
            messagebox.showerror("Mosquitto service", f"{action_name} failed.\n{e}")
        finally:
            self.refresh_runtime_services_status()

    def _run_linux_systemctl(self, args: list[str], action_name: str) -> bool:
        if os.name != "posix":
            return False
        if not shutil.which("systemctl"):
            messagebox.showerror("Mosquitto service", "systemctl not found on this Linux system.")
            return False

        cmd = ["systemctl"] + args
        try:
            out = subprocess.check_output(cmd, text=True, errors="replace", stderr=subprocess.STDOUT)
            self.install_action_var.set(f"{action_name}: mosquitto")
            if out.strip():
                self.install_action_var.set(f"{action_name}: mosquitto ({out.splitlines()[-1].strip()})")
            return True
        except subprocess.CalledProcessError as e:
            msg = (e.output or "").strip()
            low = msg.lower()
            needs_auth = (
                "access denied" in low
                or "permission denied" in low
                or "interactive authentication required" in low
                or "authentication is required" in low
            )

            if needs_auth and shutil.which("pkexec"):
                try:
                    out = subprocess.check_output(
                        ["pkexec", "systemctl"] + args,
                        text=True,
                        errors="replace",
                        stderr=subprocess.STDOUT,
                    )
                    self.install_action_var.set(f"{action_name}: mosquitto (elevated)")
                    if out.strip():
                        self.install_action_var.set(
                            f"{action_name}: mosquitto ({out.splitlines()[-1].strip()})"
                        )
                    return True
                except subprocess.CalledProcessError as pe:
                    msg = (pe.output or "").strip() or str(pe)
                except Exception as pe:
                    msg = str(pe)

            manual = "sudo " + " ".join(cmd)
            install_hint = ""
            if "could not be found" in low or "not be found" in low or "not-found" in low:
                install_hint = "\n\nInstall first:\nsudo apt-get install -y mosquitto"
            messagebox.showerror(
                "Mosquitto service",
                f"{action_name} failed.\n{msg or e}\n\nTry in terminal:\n{manual}{install_hint}",
            )
            return False
        except Exception as e:
            messagebox.showerror("Mosquitto service", f"{action_name} failed.\n{e}")
            return False

    def _run_linux_mosquitto_sequence(self, steps: list[tuple[list[str], str]]) -> None:
        if os.name != "posix":
            return
        try:
            for args, action_name in steps:
                if not self._run_linux_systemctl(args, action_name):
                    return
        finally:
            self.refresh_runtime_services_status()

    def _run_linux_command_with_elevation(self, cmd: list[str], action_name: str) -> bool:
        if os.name != "posix":
            return False
        try:
            out = subprocess.check_output(
                cmd,
                text=True,
                errors="replace",
                stderr=subprocess.STDOUT,
            )
            self.install_action_var.set(action_name)
            if out.strip():
                self.install_action_var.set(f"{action_name} ({out.splitlines()[-1].strip()})")
            return True
        except subprocess.CalledProcessError as e:
            msg = (e.output or "").strip()
            if shutil.which("pkexec"):
                try:
                    out = subprocess.check_output(
                        ["pkexec"] + cmd,
                        text=True,
                        errors="replace",
                        stderr=subprocess.STDOUT,
                    )
                    self.install_action_var.set(f"{action_name} (elevated)")
                    if out.strip():
                        self.install_action_var.set(
                            f"{action_name} ({out.splitlines()[-1].strip()})"
                        )
                    return True
                except subprocess.CalledProcessError as pe:
                    msg = (pe.output or "").strip() or str(pe)
                except Exception as pe:
                    msg = str(pe)

            manual_cmd = "sudo " + " ".join(shlex.quote(str(x)) for x in cmd)
            messagebox.showerror(
                "Mosquitto listener",
                f"{action_name} failed.\n{msg or e}\n\nTry in terminal:\n{manual_cmd}",
            )
            return False
        except Exception as e:
            messagebox.showerror("Mosquitto listener", f"{action_name} failed.\n{e}")
            return False

    def _configure_mosquitto_listener_linux(self, lan_enabled: bool) -> None:
        if os.name != "posix":
            messagebox.showerror("Mosquitto listener", "This action is available on Linux only.")
            return

        if not shutil.which("systemctl"):
            messagebox.showerror("Mosquitto listener", "systemctl not found on this Linux system.")
            return

        conf_path = str(MOSQUITTO_LAN_CONF_PATH)
        action_name = "Set mosquitto LAN listener" if lan_enabled else "Set mosquitto localhost listener"
        tmp_path: Path | None = None
        try:
            if lan_enabled:
                tmp_path = LOG_DIR / "mosquitto-lan.conf.tmp"
                tmp_path.write_text(MOSQUITTO_LAN_CONF_TEXT, encoding="utf-8")
                cmd = ["install", "-m", "644", str(tmp_path), conf_path]
            else:
                cmd = ["rm", "-f", conf_path]

            if not self._run_linux_command_with_elevation(cmd, action_name):
                return

            # Restart broker so new bind mode applies immediately.
            self._run_linux_mosquitto_sequence([(["restart", "mosquitto"], "Restart")])
            self.install_action_var.set(
                "Mosquitto listener: LAN 1883 enabled"
                if lan_enabled
                else "Mosquitto listener: localhost-only"
            )
        finally:
            if tmp_path is not None:
                try:
                    tmp_path.unlink(missing_ok=True)
                except Exception:
                    pass
            self.refresh_runtime_services_status()

    def enable_mosquitto_lan_listener(self) -> None:
        self._configure_mosquitto_listener_linux(lan_enabled=True)

    def disable_mosquitto_lan_listener(self) -> None:
        self._configure_mosquitto_listener_linux(lan_enabled=False)

    def _run_sc_mosquitto_elevated(self, args: list[str], action_name: str) -> bool:
        if os.name != "nt":
            return False
        try:
            # Ensure service can be started when disabled.
            if action_name.lower().strip() == "start":
                state, startup = self._query_windows_service("mosquitto")
                if startup == "Disabled":
                    if not self._run_sc_elevated(["config", "mosquitto", "start= demand"]):
                        return False
            return self._run_sc_elevated(args)
        except Exception:
            return False

    def _run_sc_elevated(self, args: list[str]) -> bool:
        if os.name != "nt":
            return False
        # Use PowerShell Start-Process -Verb RunAs so Windows shows UAC prompt.
        quoted: list[str] = []
        for a in args:
            q = str(a).replace("'", "''")
            quoted.append("'" + q + "'")
        ps_args = ", ".join(quoted)
        script = (
            f"$p = Start-Process -FilePath 'sc.exe' -ArgumentList @({ps_args}) "
            "-Verb RunAs -Wait -PassThru; exit $p.ExitCode"
        )
        rc = subprocess.call(["powershell", "-NoProfile", "-Command", script])
        return rc == 0

    def start_mosquitto_service(self) -> None:
        if os.name == "nt":
            self._run_sc_mosquitto(["start", "mosquitto"], "Start")
            return
        if os.name == "posix":
            _, startup = self._query_linux_service("mosquitto")
            steps: list[tuple[list[str], str]] = []
            if startup == "Disabled":
                steps.append((["unmask", "mosquitto"], "Unmask"))
            steps.append((["start", "mosquitto"], "Start"))
            self._run_linux_mosquitto_sequence(steps)
            return
        messagebox.showerror("Mosquitto service", "Unsupported OS for mosquitto service control.")

    def stop_mosquitto_service(self) -> None:
        if os.name == "nt":
            self._run_sc_mosquitto(["stop", "mosquitto"], "Stop")
            return
        if os.name == "posix":
            self._run_linux_mosquitto_sequence([(["stop", "mosquitto"], "Stop")])
            return
        messagebox.showerror("Mosquitto service", "Unsupported OS for mosquitto service control.")

    def restart_mosquitto_service(self) -> None:
        if os.name == "nt":
            self.stop_mosquitto_service()
            time.sleep(0.5)
            self.start_mosquitto_service()
            return
        if os.name == "posix":
            _, startup = self._query_linux_service("mosquitto")
            steps: list[tuple[list[str], str]] = []
            if startup == "Disabled":
                steps.append((["unmask", "mosquitto"], "Unmask"))
            steps.append((["restart", "mosquitto"], "Restart"))
            self._run_linux_mosquitto_sequence(steps)
            return
        messagebox.showerror("Mosquitto service", "Unsupported OS for mosquitto service control.")

    def set_mosquitto_startup(self, mode: str) -> None:
        normalized = mode.lower().strip()
        if os.name == "nt":
            mapping = {"automatic": "auto", "manual": "demand", "disabled": "disabled"}
            m = mapping.get(normalized)
            if not m:
                return
            self._run_sc_mosquitto(["config", "mosquitto", f"start= {m}"], f"Set startup {mode}")
            return
        if os.name == "posix":
            steps: list[tuple[list[str], str]] = []
            if normalized == "automatic":
                steps = [
                    (["unmask", "mosquitto"], "Unmask"),
                    (["enable", "mosquitto"], "Set startup automatic"),
                ]
            elif normalized == "manual":
                steps = [
                    (["unmask", "mosquitto"], "Unmask"),
                    (["disable", "mosquitto"], "Set startup manual"),
                ]
            elif normalized == "disabled":
                steps = [
                    (["disable", "--now", "mosquitto"], "Disable now"),
                    (["mask", "mosquitto"], "Set startup disabled"),
                ]
            if steps:
                self._run_linux_mosquitto_sequence(steps)
            return
        messagebox.showerror("Mosquitto service", "Unsupported OS for mosquitto service control.")

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
        ngrok_cmd = self._ngrok_base_cmd()
        if not ngrok_cmd:
            messagebox.showerror("Missing ngrok", "ngrok not found.")
            return
        ngrok_token = self.env.get("NGROK_AUTHTOKEN", "").strip()
        if ngrok_token:
            try:
                subprocess.check_call(ngrok_cmd + ["config", "add-authtoken", ngrok_token])
            except Exception as e:
                messagebox.showerror("ngrok authtoken", f"Failed to apply NGROK_AUTHTOKEN.\n{e}")
                return
        elif not ngrok_config_has_authtoken():
            messagebox.showerror("Missing ngrok authtoken", "NGROK_AUTHTOKEN is required.")
            return
        existing_ngrok_url = self._existing_ngrok_https_url_for_port(self.http_port)
        if existing_ngrok_url:
            self.install_action_var.set("ngrok already running")
            self.ngrok_var.set(f"ngrok: {existing_ngrok_url}")
            self.webhook_var.set(f"Webhook: {existing_ngrok_url}/line/webhook")
            self.refresh_runtime_services_status()
            return
        if self.ngrok_proc is None or self.ngrok_proc.poll() is not None:
            ngrok_log = self._logfile("ngrok")
            f = open(ngrok_log, "a", encoding="utf-8", errors="replace")
            popen_kwargs: dict[str, object] = {"cwd": str(ROOT), "stdout": f, "stderr": subprocess.STDOUT}
            if os.name == "nt":
                popen_kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
            else:
                popen_kwargs["start_new_session"] = True
            self.ngrok_proc = subprocess.Popen(ngrok_cmd + ["http", str(self.http_port)], **popen_kwargs)
        self.install_action_var.set("ngrok started")
        self.refresh_runtime_services_status()

    def stop_ngrok_only(self) -> None:
        if self.ngrok_proc is not None and self.ngrok_proc.poll() is None:
            taskkill(self.ngrok_proc.pid)
        self.ngrok_proc = None
        for pid in port_listeners(4040):
            taskkill(pid)
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
        self._begin_busy(title)

        def worker() -> None:
            rc = 0
            wd = str(cwd or ROOT)
            with open(log_path, "w", encoding="utf-8", errors="replace") as f:
                f.write(f"TITLE: {title}\nCWD: {wd}\n\n")
                for cmd in commands:
                    f.write("CMD: " + " ".join(cmd) + "\n")
                    call_kwargs: dict[str, object] = {"cwd": wd, "stdout": f, "stderr": subprocess.STDOUT}
                    if os.name == "nt":
                        call_kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
                    rc = subprocess.call(cmd, **call_kwargs)
                    f.write(f"RC: {rc}\n\n")
                    if rc != 0:
                        break

            def done() -> None:
                self._end_busy()
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

    def quick_setup_all(self) -> None:
        if os.name == "nt":
            setup_cmd = PROJECT_ROOT / "setup.cmd"
            if not setup_cmd.exists():
                messagebox.showerror("Missing setup script", f"Missing file: {setup_cmd}")
                return
            self._run_job_async("Quick Setup", [["cmd", "/c", str(setup_cmd)]], cwd=PROJECT_ROOT)
            return

        setup_sh = PROJECT_ROOT / "setup.sh"
        if not setup_sh.exists():
            messagebox.showerror("Missing setup script", f"Missing file: {setup_sh}")
            return
        if shutil.which("bash"):
            self._run_job_async("Quick Setup", [["bash", str(setup_sh)]], cwd=PROJECT_ROOT)
            return
        messagebox.showerror("bash missing", "Cannot run setup.sh because 'bash' is not available on PATH.")

    def install_platformio_core(self) -> None:
        py = self._host_python_cmd()
        if not py:
            messagebox.showerror("Python missing", "Cannot find python/python3.")
            return
        pio_py = self._platformio_venv_python()
        cmds: list[list[str]] = []
        if not pio_py.exists():
            cmds.append(py + ["-m", "venv", str(PIO_VENV_DIR)])
        cmds.append([str(pio_py), "-m", "pip", "install", "--upgrade", "pip"])
        cmds.append([str(pio_py), "-m", "pip", "install", "--upgrade", "platformio"])
        self._run_job_async("Install PlatformIO", cmds, cwd=PROJECT_ROOT)

    def install_ngrok_cli(self) -> None:
        if self._ngrok_base_cmd():
            self.install_action_var.set("Install ngrok: already available")
            messagebox.showinfo("Install ngrok", "ngrok is already available.")
            return
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

    def _describe_env(self, env_name: str) -> str:
        custom = {
            "main-board": "production firmware (main board)",
            "automation-board": "automation firmware (automation board)",
        }
        if env_name in custom:
            return f"{env_name}: {custom[env_name]}"
        info = (self.fw_env_details.get(env_name, "") or "").strip()
        if not info:
            return f"{env_name}: no extra details"
        return f"{env_name}: {info}"

    def _update_fw_env_descriptions(self) -> None:
        self.fw_build_env_desc.set(self._describe_env((self.fw_build_env.get() or "").strip()))
        self.fw_upload_env_desc.set(self._describe_env((self.fw_upload_env.get() or "").strip()))

    def refresh_fw_envs(self) -> None:
        current_build = normalize_fw_env_name((self.fw_build_env.get() or "").strip())
        current_upload = normalize_fw_env_name((self.fw_upload_env.get() or "").strip())
        all_envs, details = parse_platformio_envs(PLATFORMIO_INI_PATH)
        envs = select_fw_env_options(all_envs)
        if current_build and current_build not in envs:
            current_build = envs[0]
        if current_upload and current_upload not in envs:
            current_upload = envs[0]
        self.fw_env_options = envs
        self.fw_env_details = details
        if self._fw_build_env_box is not None:
            self._fw_build_env_box["values"] = self.fw_env_options
        if self._fw_upload_env_box is not None:
            self._fw_upload_env_box["values"] = self.fw_env_options
        if current_build in self.fw_env_options:
            self.fw_build_env.set(current_build)
        else:
            self.fw_build_env.set(self.fw_env_options[0])
        if current_upload in self.fw_env_options:
            self.fw_upload_env.set(current_upload)
        else:
            self.fw_upload_env.set(self.fw_env_options[0])
        self._update_fw_env_descriptions()

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
        main_cid = self.fw_mqtt_client_id_main.get().strip() or "embedded-security-esp32"
        auto_cid = self.fw_mqtt_client_id_auto.get().strip() or f"{main_cid}-auto"
        if auto_cid == main_cid:
            auto_cid = f"{auto_cid}-auto"
        lines = upsert_env_kv(lines, "FW_MQTT_CLIENT_ID", main_cid)
        lines = upsert_env_kv(lines, "FW_MQTT_CLIENT_ID_AUTOMATION", auto_cid)
        lines = upsert_env_kv(lines, "FW_CMD_TOKEN", self.fw_cmd_token.get().strip())
        selected_build_env = normalize_fw_env_name((self.fw_build_env.get() or "").strip())
        if selected_build_env not in self.fw_env_options:
            selected_build_env = self.fw_env_options[0] if self.fw_env_options else DEFAULT_FW_ENV
            self.fw_build_env.set(selected_build_env)
        selected_upload_env = normalize_fw_env_name((self.fw_upload_env.get() or "").strip())
        if selected_upload_env not in self.fw_env_options:
            selected_upload_env = selected_build_env
            self.fw_upload_env.set(selected_upload_env)
        lines = upsert_env_kv(lines, "FW_ENV_BUILD", selected_build_env)
        lines = upsert_env_kv(lines, "FW_ENV_UPLOAD", selected_upload_env)
        lines = upsert_env_kv(lines, "FW_ENV", selected_build_env)
        upload_port = self.fw_upload_port.get().strip()
        if upload_port not in self.serial_ports:
            upload_port = ""
        lines = upsert_env_kv(lines, "FW_UPLOAD_PORT", upload_port)

        write_env_lines(ENV_PATH, lines)
        self.env = read_env(ENV_PATH)
        if notify:
            messagebox.showinfo("Saved", f"Firmware fields saved to {ENV_PATH}")
        return True

    def _run_platformio_async(self, args: list[str], success_msg: str, fw_env: str) -> None:
        base = self._platformio_base_cmd()
        if not base:
            messagebox.showerror("PlatformIO not found", "Cannot find platformio/pio/python in PATH.")
            return

        log_path = self._logfile("platformio")
        selected_fw_env = normalize_fw_env_name((fw_env or DEFAULT_FW_ENV).strip() or DEFAULT_FW_ENV)
        self._begin_busy(f"PlatformIO {selected_fw_env}")
        cmd = base + [
            "run",
            "-c",
            str(PLATFORMIO_INI_PATH),
            "-e",
            selected_fw_env,
        ] + args

        def worker() -> None:
            with open(log_path, "w", encoding="utf-8", errors="replace") as f:
                f.write("CMD: " + " ".join(cmd) + "\n\n")
                call_kwargs: dict[str, object] = {"cwd": str(PROJECT_ROOT), "stdout": f, "stderr": subprocess.STDOUT}
                if os.name == "nt":
                    call_kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
                rc = subprocess.call(cmd, **call_kwargs)

            def done() -> None:
                self._end_busy()
                if rc == 0:
                    messagebox.showinfo("PlatformIO", f"{success_msg}\nLog: {log_path}")
                else:
                    messagebox.showerror("PlatformIO", f"Command failed (rc={rc}).\nLog: {log_path}")

            self.root.after(0, done)

        threading.Thread(target=worker, daemon=True).start()

    def build_firmware(self) -> None:
        if not self.save_firmware_env(notify=False):
            return
        selected_fw_env = normalize_fw_env_name((self.fw_build_env.get() or self.env.get("FW_ENV_BUILD", "") or self.env.get("FW_ENV", "") or DEFAULT_FW_ENV).strip())
        self._run_platformio_async([], "Build completed", selected_fw_env)

    def _deploy_env(self, env_name: str) -> None:
        selected = normalize_fw_env_name(env_name)
        if selected not in self.fw_env_options:
            messagebox.showerror("Invalid deploy env", f"Environment '{selected}' is not available in this project.")
            return
        self.fw_build_env.set(selected)
        self.fw_upload_env.set(selected)
        self._update_fw_env_descriptions()
        self.upload_firmware()

    def deploy_main_board(self) -> None:
        self._deploy_env(MAIN_BOARD_ENV)

    def deploy_automation_board(self) -> None:
        self._deploy_env(AUTOMATION_BOARD_ENV)

    def upload_firmware(self) -> None:
        # Refresh port list right before upload to avoid stale COM selections.
        self.refresh_upload_ports()
        selected_port = (self.fw_upload_port.get() or "").strip()
        if selected_port and selected_port not in self.serial_ports:
            selected_port = ""

        # Guard against stale ports that appear in the list but cannot be opened.
        validate_ports = can_validate_serial_ports()
        if selected_port and validate_ports and not is_serial_port_usable(selected_port):
            # Try to pick another currently usable port automatically.
            fallback = ""
            for p in self.serial_ports:
                if is_serial_port_usable(p):
                    fallback = p
                    break
            if fallback:
                self.fw_upload_port.set(fallback)
                selected_port = fallback
                messagebox.showwarning(
                    "Upload port changed",
                    f"Selected port is not usable now.\nSwitched to '{fallback}'.",
                )
            else:
                # Let PlatformIO auto-detect if we cannot validate any port now.
                self.fw_upload_port.set("")
                selected_port = ""
                messagebox.showwarning(
                    "Upload port unavailable",
                    "No currently usable serial port was found.\nWill try PlatformIO auto-detect.",
                )
        elif selected_port and not validate_ports:
            # pyserial isn't installed in launcher venv, so skip local open-port probes.
            # PlatformIO can still upload with explicit port or auto-detect.
            pass

        if not self.save_firmware_env(notify=False):
            return
        extra: list[str] = ["-t", "upload"]
        port = selected_port
        if port not in self.serial_ports:
            port = ""
        if port:
            extra += ["--upload-port", port]
        selected_fw_env = normalize_fw_env_name((self.fw_upload_env.get() or self.env.get("FW_ENV_UPLOAD", "") or self.env.get("FW_ENV", "") or DEFAULT_FW_ENV).strip())
        self._run_platformio_async(extra, "Upload completed", selected_fw_env)

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
        ngrok_cmd = self._ngrok_base_cmd()
        if not ngrok_cmd:
            messagebox.showerror("ngrok Authtoken", "ngrok not found. Install ngrok first.")
            return
        try:
            subprocess.check_call(ngrok_cmd + ["config", "add-authtoken", tok])
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
