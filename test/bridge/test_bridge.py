import base64
import hashlib
import hmac
import importlib.util
from pathlib import Path
import sys
import types
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[2]
BRIDGE_PATH = ROOT / "tools" / "line_bridge" / "bridge.py"


def _install_dependency_stubs() -> None:
    if "fastapi" not in sys.modules:
        fastapi = types.ModuleType("fastapi")

        class HTTPException(Exception):
            def __init__(self, status_code: int, detail: str = ""):
                super().__init__(detail)
                self.status_code = status_code
                self.detail = detail

        class FastAPI:
            def __init__(self, *args, **kwargs):
                pass

            def get(self, *args, **kwargs):
                def deco(func):
                    return func
                return deco

            def post(self, *args, **kwargs):
                def deco(func):
                    return func
                return deco

        def Header(default="", *args, **kwargs):  # noqa: N802
            return default

        class Request:
            pass

        fastapi.FastAPI = FastAPI
        fastapi.Header = Header
        fastapi.HTTPException = HTTPException
        fastapi.Request = Request
        sys.modules["fastapi"] = fastapi

    if "requests" not in sys.modules:
        requests = types.ModuleType("requests")

        def post(*args, **kwargs):
            return None

        requests.post = post
        sys.modules["requests"] = requests

    if "paho.mqtt.client" not in sys.modules:
        paho = types.ModuleType("paho")
        paho_mqtt = types.ModuleType("paho.mqtt")
        client_mod = types.ModuleType("paho.mqtt.client")

        class CallbackAPIVersion:
            VERSION2 = 2

        class _Info:
            def __init__(self, rc: int = 0):
                self.rc = rc

        class Client:
            def __init__(self, *args, **kwargs):
                self.on_connect = None
                self.on_disconnect = None
                self.on_message = None

            def username_pw_set(self, *args, **kwargs):
                return None

            def connect(self, *args, **kwargs):
                return 0

            def loop_forever(self, *args, **kwargs):
                return None

            def subscribe(self, *args, **kwargs):
                return (0, 1)

            def publish(self, *args, **kwargs):
                return _Info(0)

        class MQTTMessage:
            payload = b""
            topic = ""

        client_mod.CallbackAPIVersion = CallbackAPIVersion
        client_mod.Client = Client
        client_mod.MQTT_ERR_SUCCESS = 0
        client_mod.MQTTMessage = MQTTMessage

        sys.modules["paho"] = paho
        sys.modules["paho.mqtt"] = paho_mqtt
        sys.modules["paho.mqtt.client"] = client_mod


def _load_bridge_module():
    spec = importlib.util.spec_from_file_location("bridge_under_test", BRIDGE_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load module from {BRIDGE_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


try:
    bridge = _load_bridge_module()
except ModuleNotFoundError:
    _install_dependency_stubs()
    bridge = _load_bridge_module()


class DummyRequest:
    def __init__(self, payload):
        self._payload = payload

    async def json(self):
        return self._payload


class DummyPublishInfo:
    def __init__(self, rc):
        self.rc = rc


class DummyMqttClient:
    def __init__(self, rc):
        self.rc = rc
        self.calls = []

    def publish(self, topic, payload=None, qos=0, retain=False):
        self.calls.append(
            {
                "topic": topic,
                "payload": payload,
                "qos": qos,
                "retain": retain,
            }
        )
        return DummyPublishInfo(self.rc)


class PublishCmdTests(unittest.TestCase):
    def setUp(self):
        bridge.state.last_cmd = ""
        bridge.state.last_cmd_at = 0.0

    def test_publish_cmd_returns_false_when_mqtt_publish_fails(self):
        fake_mqtt = DummyMqttClient(rc=1)
        with patch.object(bridge, "mqtt_client", fake_mqtt):
            with patch.object(bridge, "BRIDGE_CMD_TOKEN", "token"):
                with patch.object(bridge, "_encode_command_payload", lambda cmd: f"enc:{cmd}"):
                    ok = bridge.publish_cmd("lock door")

        self.assertFalse(ok)
        self.assertEqual("", bridge.state.last_cmd)
        self.assertEqual(0.0, bridge.state.last_cmd_at)
        self.assertEqual(1, len(fake_mqtt.calls))

    def test_publish_cmd_updates_state_when_mqtt_publish_succeeds(self):
        success_rc = int(getattr(bridge.mqtt, "MQTT_ERR_SUCCESS", 0))
        fake_mqtt = DummyMqttClient(rc=success_rc)
        with patch.object(bridge, "mqtt_client", fake_mqtt):
            with patch.object(bridge, "BRIDGE_CMD_TOKEN", "token"):
                with patch.object(bridge, "_encode_command_payload", lambda cmd: f"enc:{cmd}"):
                    ok = bridge.publish_cmd("unlock all")

        self.assertTrue(ok)
        self.assertEqual("unlock all", bridge.state.last_cmd)
        self.assertGreater(bridge.state.last_cmd_at, 0.0)
        self.assertEqual(1, len(fake_mqtt.calls))

    def test_publish_cmd_allows_status_without_token(self):
        success_rc = int(getattr(bridge.mqtt, "MQTT_ERR_SUCCESS", 0))
        fake_mqtt = DummyMqttClient(rc=success_rc)
        with patch.object(bridge, "mqtt_client", fake_mqtt):
            with patch.object(bridge, "BRIDGE_CMD_TOKEN", ""):
                ok = bridge.publish_cmd("status")

        self.assertTrue(ok)
        self.assertEqual("status", bridge.state.last_cmd)
        self.assertEqual(1, len(fake_mqtt.calls))
        self.assertEqual("status", fake_mqtt.calls[0]["payload"])


class HttpCmdTests(unittest.IsolatedAsyncioTestCase):
    async def test_http_cmd_rejects_when_auth_token_missing(self):
        req = DummyRequest({"cmd": "lock door"})
        with patch.object(bridge, "BRIDGE_CMD_TOKEN", ""):
            with self.assertRaises(bridge.HTTPException) as cm:
                await bridge.http_cmd(req)
        self.assertEqual(503, cm.exception.status_code)

    async def test_http_cmd_rejects_unsupported_command(self):
        req = DummyRequest({"cmd": "arm away"})
        with patch.object(bridge, "BRIDGE_CMD_TOKEN", "token"):
            with self.assertRaises(bridge.HTTPException) as cm:
                await bridge.http_cmd(req)
        self.assertEqual(400, cm.exception.status_code)

    async def test_http_cmd_rejects_when_publish_is_blocked(self):
        req = DummyRequest({"cmd": "lock all"})
        with patch.object(bridge, "BRIDGE_CMD_TOKEN", "token"):
            with patch.object(bridge, "publish_cmd", return_value=False):
                with self.assertRaises(bridge.HTTPException) as cm:
                    await bridge.http_cmd(req)
        self.assertEqual(503, cm.exception.status_code)

    async def test_http_cmd_accepts_supported_command(self):
        req = DummyRequest({"cmd": " lock door "})
        with patch.object(bridge, "BRIDGE_CMD_TOKEN", "token"):
            with patch.object(bridge, "publish_cmd", return_value=True):
                out = await bridge.http_cmd(req)
        self.assertEqual({"ok": True, "cmd": "lock door"}, out)

    async def test_http_cmd_accepts_status_without_token(self):
        req = DummyRequest({"cmd": "status"})
        with patch.object(bridge, "BRIDGE_CMD_TOKEN", ""):
            with patch.object(bridge, "publish_cmd", return_value=True):
                out = await bridge.http_cmd(req)
        self.assertEqual({"ok": True, "cmd": "status"}, out)


class SnapshotTests(unittest.TestCase):
    def test_on_message_status_updates_device_lock_snapshot(self):
        bridge.state.dev_door_locked = None
        bridge.state.dev_window_locked = None
        bridge.state.dev_door_open = None
        bridge.state.dev_window_open = None

        payload = (
            b'{"mode":"disarm","level":"off","door_locked":true,'
            b'"window_locked":false,"door_open":false,"window_open":true}'
        )
        msg = types.SimpleNamespace(topic=bridge.MQTT_TOPIC_STATUS, payload=payload)
        bridge.on_message(None, None, msg)

        self.assertTrue(bridge.state.dev_door_locked)
        self.assertFalse(bridge.state.dev_window_locked)
        self.assertFalse(bridge.state.dev_door_open)
        self.assertTrue(bridge.state.dev_window_open)


class MenuTests(unittest.TestCase):
    def test_flex_home_contains_status_postback(self):
        msg = bridge.flex_home()
        self.assertEqual("flex", msg.get("type"))
        body = msg.get("contents", {}).get("body", {})
        contents = body.get("contents", [])
        status_found = False
        for item in contents:
            if item.get("type") != "box":
                continue
            for sub in item.get("contents", []):
                action = sub.get("action", {}) if isinstance(sub, dict) else {}
                if action.get("data") == "cmd=status":
                    status_found = True
                    break
            if status_found:
                break
        self.assertTrue(status_found)


class SignatureTests(unittest.TestCase):
    def test_verify_line_signature_rejects_when_secret_missing(self):
        with patch.object(bridge, "LINE_CHANNEL_SECRET", ""):
            self.assertFalse(bridge.verify_line_signature(b"{}", "sig"))

    def test_verify_line_signature_accepts_valid_signature(self):
        body = b'{"events":[]}'
        secret = "supersecret"
        signature = base64.b64encode(
            hmac.new(secret.encode("utf-8"), body, hashlib.sha256).digest()
        ).decode("utf-8")
        with patch.object(bridge, "LINE_CHANNEL_SECRET", secret):
            self.assertTrue(bridge.verify_line_signature(body, signature))


if __name__ == "__main__":
    unittest.main()
