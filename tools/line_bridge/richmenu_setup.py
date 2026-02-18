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
    img = Image.new("RGB", (W, H), (242, 246, 252))
    d = ImageDraw.Draw(img)

    # Panels
    pad = 70
    mid = W // 2
    top = 240
    bottom = H - 260

    def rr(x0, y0, x1, y1, r, fill, outline):
        d.rounded_rectangle([x0, y0, x1, y1], radius=r, fill=fill, outline=outline, width=6)

    rr(pad, top, mid - 25, bottom, 90, (255, 255, 255), (52, 66, 84))
    rr(mid + 25, top, W - pad, bottom, 90, (255, 255, 255), (52, 66, 84))

    # Header bar
    rr(pad, 60, W - pad, 190, 60, (36, 54, 76), (36, 54, 76))

    # Fonts
    try:
        font_title = ImageFont.truetype("arial.ttf", 80)
        font_big = ImageFont.truetype("arial.ttf", 210)
        font_small = ImageFont.truetype("arial.ttf", 62)
    except Exception:
        font_title = ImageFont.load_default()
        font_big = ImageFont.load_default()
        font_small = ImageFont.load_default()

    d.text((pad + 60, 90), "EmbeddedSecurity", fill=(255, 255, 255), font=font_title)
    d.text((pad + 60, 200), "Tap a button", fill=(52, 66, 84), font=font_small)

    # Left: NOTIFY (monitoring view)
    d.text((pad + 120, top + 190), "NOTIFY", fill=(36, 54, 76), font=font_big)
    d.text((pad + 140, top + 450), "Event / Status / ACK", fill=(90, 100, 115), font=font_small)
    x = pad + 250
    y = bottom - 470
    c = (36, 54, 76)
    s = 150
    gap = 24
    d.rounded_rectangle([x, y, x + s, y + s], radius=22, fill=c)
    d.rounded_rectangle([x + s + gap, y, x + 2 * s + gap, y + s], radius=22, fill=c)
    d.rounded_rectangle([x, y + s + gap, x + s, y + 2 * s + gap], radius=22, fill=c)
    d.rounded_rectangle([x + s + gap, y + s + gap, x + 2 * s + gap, y + 2 * s + gap], radius=22, fill=c)

    # Right: LOCK (window + door + lock icons)
    rx = mid + 140
    d.text((rx, top + 190), "LOCK", fill=(36, 54, 76), font=font_big)
    d.text((rx + 10, top + 450), "Door / Window / All", fill=(90, 100, 115), font=font_small)

    # Window icon
    wx = rx + 30
    wy = bottom - 500
    ww = 250
    wh = 250
    d.rounded_rectangle([wx, wy, wx + ww, wy + wh], radius=30, outline=(36, 54, 76), width=26)
    d.line([wx + ww // 2, wy + 22, wx + ww // 2, wy + wh - 22], fill=(36, 54, 76), width=18)
    d.line([wx + 22, wy + wh // 2, wx + ww - 22, wy + wh // 2], fill=(36, 54, 76), width=18)

    # Door icon
    dx = wx + 320
    dy = wy
    dw = 190
    dh = 280
    d.rounded_rectangle([dx, dy, dx + dw, dy + dh], radius=24, outline=(36, 54, 76), width=24)
    d.ellipse([dx + dw - 44, dy + dh // 2 - 12, dx + dw - 20, dy + dh // 2 + 12], fill=(36, 54, 76))

    # Padlock icon
    lx = rx + 40
    ly = bottom - 180
    d.rounded_rectangle([lx, ly, lx + 470, ly + 250], radius=52, fill=(36, 54, 76))
    d.arc([lx + 90, ly - 190, lx + 380, ly + 70], start=205, end=-25, fill=(36, 54, 76), width=64)
    d.ellipse([lx + 225, ly + 100, lx + 265, ly + 140], fill=(242, 246, 252))

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
                "action": {"type": "postback", "data": "ui=mode", "displayText": "Notify"},
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
    ap.add_argument("--chatbar", default="Notify / Lock")
    ap.add_argument("--image", default=str(ROOT / "richmenu.png"))
    ap.add_argument("--delete-all", action="store_true", help="Delete all existing rich menus first")
    ap.add_argument("--use-existing-image", action="store_true", help="Do not regenerate image; use --image as-is")
    args = ap.parse_args()

    if args.delete_all:
        for rm in list_richmenus():
            rid = str(rm.get("richMenuId") or "")
            if rid:
                delete_richmenu(rid)

    img_path = Path(args.image)
    if args.use_existing_image:
        if not img_path.exists():
            raise SystemExit(f"Image not found: {img_path}")
    else:
        generate_image(img_path)

    rid = create_richmenu(args.name, args.chatbar)
    upload_image(rid, img_path)
    set_default(rid)
    print(f"OK richMenuId={rid}")
    print("Try: open LINE chat -> you should see the rich menu bar at bottom.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
