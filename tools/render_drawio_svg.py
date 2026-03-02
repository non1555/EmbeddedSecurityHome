from __future__ import annotations

import html
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


STYLE_RE = re.compile(r"([^=;]+)=([^;]+)")


def parse_style(style: str) -> dict[str, str]:
    return {key: value for key, value in STYLE_RE.findall(style or "")}


def rect_side_point(src: dict[str, float], dst: dict[str, float]) -> tuple[float, float]:
    ax, ay, aw, ah = src["x"], src["y"], src["w"], src["h"]
    bx, by, bw, bh = dst["x"], dst["y"], dst["w"], dst["h"]
    acx, acy = ax + aw / 2, ay + ah / 2
    bcx, bcy = bx + bw / 2, by + bh / 2
    dx, dy = bcx - acx, bcy - acy
    if abs(dx) >= abs(dy):
        return (ax + aw, acy) if dx >= 0 else (ax, acy)
    return (acx, ay + ah) if dy >= 0 else (acx, ay)


def orthogonalize(points: list[tuple[float, float]]) -> list[tuple[float, float]]:
    if not points:
        return points
    out = [points[0]]
    for x2, y2 in points[1:]:
        x1, y1 = out[-1]
        if x1 == x2 and y1 == y2:
            continue
        if x1 != x2 and y1 != y2:
            if abs(x2 - x1) >= abs(y2 - y1):
                out.append((x2, y1))
            else:
                out.append((x1, y2))
        out.append((x2, y2))

    deduped = [out[0]]
    for point in out[1:]:
        if point != deduped[-1]:
            deduped.append(point)
    return deduped


def route_points(
    source_vertex: dict[str, float],
    target_vertex: dict[str, float],
    mids: list[tuple[float, float]],
) -> list[tuple[float, float]]:
    start = rect_side_point(source_vertex, target_vertex)
    end = rect_side_point(target_vertex, source_vertex)
    if mids:
        return orthogonalize([start, *mids, end])
    sx, sy = start
    ex, ey = end
    if sx == ex or sy == ey:
        return [start, end]
    midx = (sx + ex) / 2
    return orthogonalize([start, (midx, sy), (midx, ey), end])


def path_d(points: list[tuple[float, float]]) -> str:
    return " ".join(
        (("M" if index == 0 else "L") + f"{x:.1f},{y:.1f}")
        for index, (x, y) in enumerate(points)
    )


def center_of_poly(points: list[tuple[float, float]]) -> tuple[float, float]:
    if len(points) < 2:
        return points[0]
    segments: list[tuple[tuple[float, float], tuple[float, float], float]] = []
    total = 0.0
    for a, b in zip(points, points[1:]):
        dist = ((b[0] - a[0]) ** 2 + (b[1] - a[1]) ** 2) ** 0.5
        segments.append((a, b, dist))
        total += dist
    target = total / 2
    acc = 0.0
    for a, b, dist in segments:
        if acc + dist >= target and dist > 0:
            t = (target - acc) / dist
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
        acc += dist
    return points[len(points) // 2]


def draw_text_block(lines: list[str], x: float, y: float, w: float, h: float, font_size: int) -> str:
    line_gap = font_size * 1.25
    total_h = line_gap * (len(lines) - 1)
    base_y = y + h / 2 - total_h / 2 + font_size * 0.35
    tx = x + w / 2
    parts = [
        f'<text x="{tx:.1f}" y="{base_y:.1f}" text-anchor="middle" '
        f'font-family="Times New Roman" font-size="{font_size}">'
    ]
    for index, line in enumerate(lines):
        dy = 0 if index == 0 else line_gap
        parts.append(f'<tspan x="{tx:.1f}" dy="{dy:.1f}">{html.escape(line)}</tspan>')
    parts.append("</text>")
    return "".join(parts)


def render_drawio_svg(src: Path, out: Path) -> None:
    root = ET.parse(src).getroot()
    model = root.find(".//mxGraphModel")
    if model is None:
        raise RuntimeError("mxGraphModel not found")

    page_w = int(model.get("pageWidth", "2400"))
    page_h = int(model.get("pageHeight", "1400"))

    cells: dict[str, ET.Element] = {}
    for cell in root.iter("mxCell"):
        cid = cell.get("id")
        if cid:
            cells[cid] = cell

    vertices: dict[str, dict[str, float | str]] = {}
    edges: list[dict[str, object]] = []
    for cid, cell in cells.items():
        geo = cell.find("mxGeometry")
        if cell.get("vertex") == "1" and geo is not None:
            vertices[cid] = {
                "id": cid,
                "value": cell.get("value") or "",
                "style": cell.get("style") or "",
                "x": float(geo.get("x", 0)),
                "y": float(geo.get("y", 0)),
                "w": float(geo.get("width", 0)),
                "h": float(geo.get("height", 0)),
            }
        elif cell.get("edge") == "1":
            pts: list[tuple[float, float]] = []
            if geo is not None:
                arr = geo.find("Array[@as='points']")
                if arr is not None:
                    for point in arr.findall("mxPoint"):
                        pts.append((float(point.get("x", 0)), float(point.get("y", 0))))
            edges.append(
                {
                    "id": cid,
                    "value": cell.get("value") or "",
                    "style": cell.get("style") or "",
                    "source": cell.get("source"),
                    "target": cell.get("target"),
                    "points": pts,
                }
            )

    svg: list[str] = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{page_w}" height="{page_h}" '
        f'viewBox="0 0 {page_w} {page_h}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
    ]

    for edge in edges:
        source = vertices.get(edge["source"])  # type: ignore[index]
        target = vertices.get(edge["target"])  # type: ignore[index]
        if not source or not target:
            continue
        style = parse_style(edge["style"])  # type: ignore[arg-type]
        pts = route_points(source, target, edge["points"])  # type: ignore[arg-type]
        stroke = style.get("strokeColor", "#424242")
        dashed = ' stroke-dasharray="6 4"' if style.get("dashed") == "1" else ""
        svg.append(f'<path d="{path_d(pts)}" fill="none" stroke="{stroke}" stroke-width="1.2"{dashed}/>')
        label = edge["value"]  # type: ignore[index]
        if label:
            lx, ly = center_of_poly(pts)
            svg.append(
                f'<text x="{lx:.1f}" y="{ly - 4:.1f}" text-anchor="middle" '
                f'font-family="Times New Roman" font-size="12" fill="#212121">{html.escape(str(label))}</text>'
            )

    for vid in sorted(vertices, key=lambda item: int(item)):
        vertex = vertices[vid]
        style = parse_style(str(vertex["style"]))
        x, y = float(vertex["x"]), float(vertex["y"])
        w, h = float(vertex["w"]), float(vertex["h"])
        fill = style.get("fillColor", "#ffffff")
        stroke = style.get("strokeColor", "#424242")
        rx = 12 if style.get("rounded") == "1" else 0
        svg.append(
            f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" '
            f'rx="{rx}" ry="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="1.2"/>'
        )
        lines = [re.sub(r"<[^>]+>", "", part) for part in re.split(r"<br\s*/?>", str(vertex["value"]))]
        font_size = 16 if vid == "3" else 14 if int(vid) >= 23 else 12
        svg.append(draw_text_block(lines, x, y, w, h, font_size))

    svg.append("</svg>")
    out.write_text("\n".join(svg), encoding="utf-8")


def main() -> int:
    if len(sys.argv) != 3:
        print("Usage: python tools/render_drawio_svg.py <input.drawio> <output.svg>")
        return 1
    render_drawio_svg(Path(sys.argv[1]), Path(sys.argv[2]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
