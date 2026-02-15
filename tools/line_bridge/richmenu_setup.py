import argparse
import os
from pathlib import Path

import requests


ROOT = Path(__file__).resolve().parent


def load_env_file(path: Path) -> None:
    if not path.exists():
        return
    for raw in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        k = k.strip()
        v = v.strip()
        if k and k not in os.environ:
            os.environ[k] = v


def env(name: str, default: str = "") -> str:
    return os.environ.get(name, default)


def line_headers() -> dict:
    tok = env("LINE_CHANNEL_ACCESS_TOKEN")
    if not tok:
        raise SystemExit("Missing LINE_CHANNEL_ACCESS_TOKEN in tools/line_bridge/.env")
    return {"Authorization": f"Bearer {tok}"}


def line_json_headers() -> dict:
    h = line_headers()
    h["Content-Type"] = "application/json"
    return h


def api(method: str, path: str, **kwargs):
    url = f"https://api.line.me{path}"
    r = requests.request(method, url, timeout=15, **kwargs)
    if r.status_code >= 400:
        raise SystemExit(f"{method} {path} failed: {r.status_code}\n{r.text}")
    if r.text:
        try:
            return r.json()
        except Exception:
            return r.text
    return None


def generate_image(out_path: Path) -> None:
    try:
        from PIL import Image, ImageDraw, ImageFont  # type: ignore
    except Exception:
        raise SystemExit(
            "Pillow is required to generate the rich menu image.\n"
            "Install:\n"
            "  Windows: tools\\line_bridge\\.venv\\Scripts\\python.exe -m pip install pillow\n"
            "  Linux:   tools/line_bridge/.venv/bin/python3 -m pip install pillow\n"
        )

    W, H = 2500, 1686
    img = Image.new("RGB", (W, H), (245, 247, 250))
    d = ImageDraw.Draw(img)

    # Panels
    pad = 70
    mid = W // 2
    top = 240
    bottom = H - 260

    def rr(x0, y0, x1, y1, r, fill, outline):
        d.rounded_rectangle([x0, y0, x1, y1], radius=r, fill=fill, outline=outline, width=6)

    rr(pad, top, mid - 25, bottom, 90, (255, 255, 255), (40, 40, 40))
    rr(mid + 25, top, W - pad, bottom, 90, (255, 255, 255), (40, 40, 40))

    # Header bar
    rr(pad, 60, W - pad, 190, 60, (40, 40, 40), (40, 40, 40))

    # Fonts
    try:
        font_title = ImageFont.truetype("arial.ttf", 80)
        font_big = ImageFont.truetype("arial.ttf", 220)
        font_small = ImageFont.truetype("arial.ttf", 70)
    except Exception:
        font_title = ImageFont.load_default()
        font_big = ImageFont.load_default()
        font_small = ImageFont.load_default()

    d.text((pad + 60, 90), "EmbeddedSecurity", fill=(255, 255, 255), font=font_title)
    d.text((pad + 60, 200), "Tap a button", fill=(40, 40, 40), font=font_small)

    # Left: MODE
    d.text((pad + 200, top + 200), "MODE", fill=(40, 40, 40), font=font_big)
    d.text((pad + 210, top + 470), "Disarm / Night / Away", fill=(80, 80, 80), font=font_small)
    # Simple icon: three bars
    x = pad + 200
    y = bottom - 380
    for i, w in enumerate([520, 420, 300]):
        d.rectangle([x, y + i * 110, x + w, y + i * 110 + 60], fill=(40, 40, 40))

    # Right: LOCK
    rx = mid + 140
    d.text((rx, top + 200), "LOCK", fill=(40, 40, 40), font=font_big)
    d.text((rx + 10, top + 470), "Door / Window", fill=(80, 80, 80), font=font_small)
    # Simple icon: padlock
    cx = W - pad - 560
    cy = bottom - 330
    d.rounded_rectangle([cx, cy, cx + 520, cy + 340], radius=60, fill=(40, 40, 40))
    d.arc([cx + 90, cy - 220, cx + 430, cy + 90], start=200, end=-20, fill=(40, 40, 40), width=80)
    d.ellipse([cx + 240, cy + 130, cx + 290, cy + 180], fill=(245, 247, 250))

    out_path.parent.mkdir(parents=True, exist_ok=True)
    img.save(out_path, format="PNG")


def create_richmenu(name: str, chat_bar_text: str) -> str:
    body = {
        "size": {"width": 2500, "height": 1686},
        "selected": True,
        "name": name,
        "chatBarText": chat_bar_text,
        "areas": [
            {
                "bounds": {"x": 0, "y": 0, "width": 1250, "height": 1686},
                # LINE API currently requires non-empty displayText for postback actions.
                "action": {"type": "postback", "data": "ui=mode", "displayText": "Mode"},
            },
            {
                "bounds": {"x": 1250, "y": 0, "width": 1250, "height": 1686},
                "action": {"type": "postback", "data": "ui=lock", "displayText": "Lock"},
            },
        ],
    }
    j = api("POST", "/v2/bot/richmenu", headers=line_json_headers(), json=body)
    rid = str(j.get("richMenuId") or "")
    if not rid:
        raise SystemExit(f"Unexpected response: {j}")
    return rid


def upload_image(richmenu_id: str, image_path: Path) -> None:
    data = image_path.read_bytes()
    # Rich menu content upload uses api-data.
    url = f"https://api-data.line.me/v2/bot/richmenu/{richmenu_id}/content"
    headers = {**line_headers(), "Content-Type": "image/png"}

    # LINE occasionally returns 404 immediately after creating a richmenu.
    for attempt in range(1, 6):
        r = requests.post(url, headers=headers, data=data, timeout=20)
        if r.status_code < 400:
            return
        if r.status_code == 404 and attempt < 6:
            # Give LINE API a moment to propagate.
            import time

            time.sleep(1.2)
            continue
        raise SystemExit(f"POST /v2/bot/richmenu/{richmenu_id}/content failed: {r.status_code}\n{r.text}")


def set_default(richmenu_id: str) -> None:
    api("POST", f"/v2/bot/user/all/richmenu/{richmenu_id}", headers=line_headers())


def list_richmenus() -> list[dict]:
    j = api("GET", "/v2/bot/richmenu/list", headers=line_headers())
    return list(j.get("richmenus") or [])


def delete_richmenu(richmenu_id: str) -> None:
    api("DELETE", f"/v2/bot/richmenu/{richmenu_id}", headers=line_headers())


def main() -> int:
    load_env_file(ROOT / ".env")

    ap = argparse.ArgumentParser()
    ap.add_argument("--name", default="EmbeddedSecurityHome")
    ap.add_argument("--chatbar", default="Mode / Lock")
    ap.add_argument("--image", default=str(ROOT / "richmenu.png"))
    ap.add_argument("--delete-all", action="store_true", help="Delete all existing rich menus first")
    args = ap.parse_args()

    if args.delete_all:
        for rm in list_richmenus():
            rid = str(rm.get("richMenuId") or "")
            if rid:
                delete_richmenu(rid)

    img_path = Path(args.image)
    if not img_path.exists():
        generate_image(img_path)

    rid = create_richmenu(args.name, args.chatbar)
    upload_image(rid, img_path)
    set_default(rid)
    print(f"OK richMenuId={rid}")
    print("Try: open LINE chat -> you should see the rich menu bar at bottom.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
