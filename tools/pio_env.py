from pathlib import Path

Import("env")


def _read_env(path: Path) -> dict:
    out = {}
    if not path.exists():
        return out
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        out[k.strip()] = v.strip()
    return out


def _cstr(value: str) -> str:
    return '\\"' + value.replace("\\", "\\\\").replace('"', '\\"') + '\\"'


def _as_int(value: str, default: int) -> int:
    try:
        return int(str(value).strip())
    except Exception:
        return default


project_dir = Path(env["PROJECT_DIR"])
env_file = project_dir / "tools" / "line_bridge" / ".env"
cfg = _read_env(env_file)

wifi_ssid = cfg.get("FW_WIFI_SSID", "")
wifi_password = cfg.get("FW_WIFI_PASSWORD", "")
mqtt_broker = cfg.get("FW_MQTT_BROKER", "127.0.0.1")
mqtt_port = _as_int(cfg.get("FW_MQTT_PORT", "1883"), 1883)
mqtt_username = cfg.get("FW_MQTT_USERNAME", "")
mqtt_password = cfg.get("FW_MQTT_PASSWORD", "")
mqtt_client_id = cfg.get("FW_MQTT_CLIENT_ID", "embedded-security-esp32")

env.Append(
    CPPDEFINES=[
        ("WIFI_SSID", _cstr(wifi_ssid)),
        ("WIFI_PASSWORD", _cstr(wifi_password)),
        ("MQTT_BROKER", _cstr(mqtt_broker)),
        ("MQTT_PORT", mqtt_port),
        ("MQTT_USERNAME", _cstr(mqtt_username)),
        ("MQTT_PASSWORD", _cstr(mqtt_password)),
        ("MQTT_CLIENT_ID", _cstr(mqtt_client_id)),
    ]
)

