import json
import os
import platform
import shutil
import signal
import subprocess
import sys
import time
import urllib.request
import webbrowser
from pathlib import Path
from tkinter import Button, Tk, StringVar, ttk, messagebox


ROOT = Path(__file__).resolve().parent
ENV_PATH = ROOT / ".env"
LOG_DIR = ROOT / "logs"
LOG_DIR.mkdir(exist_ok=True)


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


def http_post_json(url: str, body: dict, timeout_s: float = 2.0) -> dict:
    data = json.dumps(body).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={"User-Agent": "EmbeddedSecurityHome/launcher", "Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout_s) as resp:
        out = resp.read()
    return json.loads(out.decode("utf-8", errors="replace"))


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

        self.device_var = StringVar(value="Device: (unknown)")

        # Config fields (.env)
        self.cfg_http_port = StringVar(value=str(self.http_port))
        self.cfg_line_token = StringVar(value=self.env.get("LINE_CHANNEL_ACCESS_TOKEN", ""))
        self.cfg_line_secret = StringVar(value=self.env.get("LINE_CHANNEL_SECRET", ""))
        self.cfg_line_target_user = StringVar(value=self.env.get("LINE_TARGET_USER_ID", ""))
        self.cfg_line_target_group = StringVar(value=self.env.get("LINE_TARGET_GROUP_ID", ""))
        self.cfg_line_target_room = StringVar(value=self.env.get("LINE_TARGET_ROOM_ID", ""))
        self.cfg_ngrok_authtoken = StringVar(value="")
        self._show_secrets = False

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
        control_tab = ttk.Frame(nb, padding=12)
        nb.add(status_tab, text="Status")
        nb.add(control_tab, text="Control")
        nb.add(config_tab, text="Config")

        # Status tab
        ttk.Label(status_tab, text="Bridge").grid(row=0, column=0, sticky="w")
        ttk.Label(status_tab, textvariable=self.status_var).grid(row=0, column=1, sticky="e")
        ttk.Label(status_tab, textvariable=self.health_var).grid(row=1, column=0, columnspan=2, sticky="w", pady=(8, 0))
        ttk.Label(status_tab, textvariable=self.ngrok_var).grid(row=2, column=0, columnspan=2, sticky="w", pady=(2, 0))
        ttk.Label(status_tab, textvariable=self.webhook_var).grid(row=3, column=0, columnspan=2, sticky="w", pady=(2, 0))

        ttk.Separator(status_tab).grid(row=4, column=0, columnspan=2, sticky="ew", pady=(12, 12))

        btns = ttk.Frame(status_tab)
        btns.grid(row=5, column=0, columnspan=2, sticky="ew")
        btns.columnconfigure(0, weight=1)
        btns.columnconfigure(1, weight=1)
        btns.columnconfigure(2, weight=1)

        ttk.Button(btns, text="Start", command=self.start).grid(row=0, column=0, sticky="ew", padx=(0, 6))
        ttk.Button(btns, text="Stop", command=self.stop).grid(row=0, column=1, sticky="ew", padx=(0, 6))
        ttk.Button(btns, text="Copy Webhook", command=self.copy_webhook).grid(row=0, column=2, sticky="ew")

        links = ttk.Frame(status_tab)
        links.grid(row=6, column=0, columnspan=2, sticky="ew", pady=(10, 0))
        links.columnconfigure(0, weight=1)
        links.columnconfigure(1, weight=1)

        ttk.Button(links, text="Open Health", command=self.open_health).grid(row=0, column=0, sticky="ew", padx=(0, 6))
        ttk.Button(links, text="Open ngrok Inspector", command=self.open_inspector).grid(row=0, column=1, sticky="ew")

        # Control tab
        control_tab.columnconfigure(0, weight=1)

        ttk.Label(control_tab, textvariable=self.device_var).grid(row=0, column=0, sticky="w")
        ttk.Separator(control_tab).grid(row=1, column=0, sticky="ew", pady=(10, 12))

        self._ctrl_home = ttk.Frame(control_tab)
        self._ctrl_mode = ttk.Frame(control_tab)
        self._ctrl_lock = ttk.Frame(control_tab)
        for f in (self._ctrl_home, self._ctrl_mode, self._ctrl_lock):
            f.grid(row=2, column=0, sticky="nsew")

        self._show_ctrl("home")

        # Home view: two big buttons (Mode / Lock)
        self._ctrl_home.columnconfigure(0, weight=1)
        self._ctrl_home.columnconfigure(1, weight=1)

        self._btn_mode_home = Button(
            self._ctrl_home,
            text="Mode",
            bitmap="info",
            compound="left",
            command=lambda: self._show_ctrl("mode"),
            padx=14,
            pady=12,
        )
        self._btn_mode_home.grid(row=0, column=0, sticky="ew", padx=(0, 10))

        self._btn_lock_home = Button(
            self._ctrl_home,
            text="Lock",
            bitmap="questhead",
            compound="left",
            command=lambda: self._show_ctrl("lock"),
            padx=14,
            pady=12,
        )
        self._btn_lock_home.grid(row=0, column=1, sticky="ew")

        # Mode view
        ttk.Label(self._ctrl_mode, text="Select Mode").grid(row=0, column=0, columnspan=2, sticky="w")
        ttk.Separator(self._ctrl_mode).grid(row=1, column=0, columnspan=2, sticky="ew", pady=(8, 12))

        Button(self._ctrl_mode, text="Disarm", bitmap="info", compound="left", command=lambda: self.send_cmd("disarm")).grid(
            row=2, column=0, sticky="ew", padx=(0, 8), pady=(0, 8)
        )
        Button(self._ctrl_mode, text="Night", bitmap="info", compound="left", command=lambda: self.send_cmd("arm night")).grid(
            row=2, column=1, sticky="ew", pady=(0, 8)
        )
        Button(self._ctrl_mode, text="Away", bitmap="info", compound="left", command=lambda: self.send_cmd("arm away")).grid(
            row=3, column=0, sticky="ew", padx=(0, 8)
        )
        ttk.Button(self._ctrl_mode, text="Back", command=lambda: self._show_ctrl("home")).grid(row=3, column=1, sticky="ew")

        # Lock view
        ttk.Label(self._ctrl_lock, text="Door/Window Locks").grid(row=0, column=0, columnspan=2, sticky="w")
        ttk.Separator(self._ctrl_lock).grid(row=1, column=0, columnspan=2, sticky="ew", pady=(8, 12))

        self._btn_door = Button(self._ctrl_lock, text="Door", bitmap="gray25", compound="left", command=self.toggle_door)
        self._btn_window = Button(self._ctrl_lock, text="Window", bitmap="gray25", compound="left", command=self.toggle_window)
        self._btn_all = Button(self._ctrl_lock, text="Lock All", bitmap="warning", compound="left", command=lambda: self.send_cmd("lock all"))
        self._btn_status = Button(self._ctrl_lock, text="Refresh Status", bitmap="hourglass", compound="left", command=self.request_status)

        self._btn_door.grid(row=2, column=0, sticky="ew", padx=(0, 8), pady=(0, 8))
        self._btn_window.grid(row=2, column=1, sticky="ew", pady=(0, 8))
        self._btn_all.grid(row=3, column=0, sticky="ew", padx=(0, 8))
        ttk.Button(self._ctrl_lock, text="Back", command=lambda: self._show_ctrl("home")).grid(row=3, column=1, sticky="ew")
        self._btn_status.grid(row=4, column=0, columnspan=2, sticky="ew", pady=(10, 0))

        # Config tab
        config_tab.columnconfigure(1, weight=1)

        r = 0
        # MQTT config is intentionally not editable here. Keep MQTT as code/board-configured.
        mqtt_broker = self.env.get("MQTT_BROKER", "")
        mqtt_port = self.env.get("MQTT_PORT", "")
        ttk.Label(config_tab, text="MQTT (read-only)").grid(row=r, column=0, sticky="w")
        ttk.Label(config_tab, text=f"{mqtt_broker}:{mqtt_port}".strip(":")).grid(row=r, column=1, sticky="w")
        r += 1

        ttk.Label(config_tab, text="HTTP Port").grid(row=r, column=0, sticky="w", pady=(6, 0))
        ttk.Entry(config_tab, textvariable=self.cfg_http_port, width=10).grid(row=r, column=1, sticky="w", pady=(6, 0))
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

        ttk.Label(config_tab, text="ngrok Authtoken (optional)").grid(row=r, column=0, sticky="w")
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

        # device state (from bridge)
        try:
            j = http_get_json(f"http://127.0.0.1:{self.http_port}/state", timeout_s=0.8)
            dev = j.get("device", {}) if isinstance(j.get("device", {}), dict) else {}
            mode = str(dev.get("mode") or "?")
            dl = dev.get("door_locked", None)
            wl = dev.get("window_locked", None)
            do = dev.get("door_open", None)
            wo = dev.get("window_open", None)

            def fmt(b: object) -> str:
                if b is True:
                    return "1"
                if b is False:
                    return "0"
                return "?"

            self.device_var.set(f"Device: mode={mode} dL={fmt(dl)} wL={fmt(wl)} dO={fmt(do)} wO={fmt(wo)}")
            # Update home button labels too (quick glance).
            try:
                self._btn_mode_home.configure(text=f"Mode ({mode})")
                self._btn_lock_home.configure(text=f"Lock (D:{fmt(dl)} W:{fmt(wl)})")
            except Exception:
                pass
            self._update_control_buttons(dl, wl)
        except Exception:
            self.device_var.set("Device: (not reachable)")
            self._update_control_buttons(None, None)

        self.root.after(900, self._tick)

    def _show_ctrl(self, name: str) -> None:
        for f in (self._ctrl_home, self._ctrl_mode, self._ctrl_lock):
            f.grid_remove()
        if name == "mode":
            self._ctrl_mode.grid()
        elif name == "lock":
            self._ctrl_lock.grid()
            # If state is unknown, request it once to populate labels.
            if "(unknown)" in self._btn_door.cget("text") or "(unknown)" in self._btn_window.cget("text"):
                self.request_status()
        else:
            self._ctrl_home.grid()

    def _update_control_buttons(self, door_locked: object, window_locked: object) -> None:
        # Button label should be the action (opposite of current state)
        if door_locked is True:
            self._btn_door.configure(text="Unlock Door", bitmap="error")
        elif door_locked is False:
            self._btn_door.configure(text="Lock Door", bitmap="warning")
        else:
            self._btn_door.configure(text="Door (unknown)", bitmap="question")

        if window_locked is True:
            self._btn_window.configure(text="Unlock Window", bitmap="error")
        elif window_locked is False:
            self._btn_window.configure(text="Lock Window", bitmap="warning")
        else:
            self._btn_window.configure(text="Window (unknown)", bitmap="question")

    def request_status(self) -> None:
        # Ask firmware to publish a status snapshot (updates /state via ACK).
        self.send_cmd("status", refresh_after=False)

    def toggle_door(self) -> None:
        # Decide based on latest text in device_var (best-effort).
        txt = self.device_var.get()
        if "dL=1" in txt:
            self.send_cmd("unlock door")
        elif "dL=0" in txt:
            self.send_cmd("lock door")
        else:
            self.request_status()

    def toggle_window(self) -> None:
        txt = self.device_var.get()
        if "wL=1" in txt:
            self.send_cmd("unlock window")
        elif "wL=0" in txt:
            self.send_cmd("lock window")
        else:
            self.request_status()

    def send_cmd(self, cmd: str, refresh_after: bool = True) -> None:
        cmd = (cmd or "").strip().lower()
        if not cmd:
            return
        try:
            http_post_json(f"http://127.0.0.1:{self.http_port}/cmd", {"cmd": cmd}, timeout_s=1.6)
        except Exception as e:
            messagebox.showerror("Command failed", f"Failed to send cmd to bridge.\n{e}")
            return
        # After a command, request status once to update lock labels.
        if refresh_after and cmd != "status":
            self.root.after(250, self.request_status)

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

        write_env_lines(ENV_PATH, lines)
        # Keep UI actions (Start/Health) aligned with saved config without requiring restart.
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
