#!/usr/bin/env python3
"""Export TTF/OTF outlines into a C++ FontFace header for the large-panel clock.

The firmware never loads a font file. This script flattens the glyphs the
panel uses into line-segment contours. Add another face by pointing --font at
a different typeface and switching activeFontFace() to the new kFont… object.
"""

from __future__ import annotations

import argparse
import math
import re
import sys
from pathlib import Path

DEFAULT_CHARS = "0123456789ABCDEFGHKNORS:-"


def cpp_ident(name: str) -> str:
    parts = [part for part in re.split(r"[^A-Za-z0-9]+", name) if part]
    if not parts:
        raise SystemExit("font name must contain letters or digits")
    return "kFont" + "".join(part[0].upper() + part[1:] for part in parts)


def table_ident(name: str) -> str:
    ident = cpp_ident(name)
    return ident[5:] if ident.startswith("kFont") else ident


def load_fonttools():
    try:
        from fontTools.pens.basePen import BasePen
        from fontTools.ttLib import TTFont
    except ImportError as error:
        raise SystemExit(
            "fontTools is required to export a face. "
            "Install it with: python3 -m pip install fonttools"
        ) from error
    return TTFont, BasePen


class FlattenPen:
    def __init__(self, glyph_set, tolerance: float):
        self.glyph_set = glyph_set
        self.tolerance = tolerance
        self.contours: list[list[tuple[float, float]]] = []
        self.current: list[tuple[float, float]] = []

    def moveTo(self, pt):
        self._commit(False)
        self.current = [pt]

    def lineTo(self, pt):
        if not self.current:
            self.current = [pt]
            return
        self.current.append(pt)

    def qCurveTo(self, *points):
        if not points:
            return
        on_curve = points[-1]
        offs = points[:-1]
        if on_curve is None:
            if not self.current:
                return
            on_curve = self.current[0]
        start = self.current[-1] if self.current else offs[0]
        pts = [start, *offs, on_curve]
        index = 0
        while index < len(pts) - 2:
            p0 = pts[index]
            p1 = pts[index + 1]
            if index + 2 == len(pts) - 1:
                p2 = pts[index + 2]
                self._flatten_quad(p0, p1, p2)
                break
            implied = ((p1[0] + pts[index + 2][0]) * 0.5, (p1[1] + pts[index + 2][1]) * 0.5)
            self._flatten_quad(p0, p1, implied)
            pts[index + 1] = implied
            index += 1

    def curveTo(self, *points):
        if len(points) != 3 or not self.current:
            return
        self._flatten_cubic(self.current[-1], points[0], points[1], points[2])

    def closePath(self):
        self._commit(True)

    def endPath(self):
        self._commit(False)

    def addComponent(self, glyph_name, transformation):
        try:
            from fontTools.pens.transformPen import TransformPen
        except ImportError:
            return
        component = self.glyph_set[glyph_name]
        pen = TransformPen(self, transformation)
        component.draw(pen)

    def _flatten_quad(self, p0, p1, p2):
        if _quad_flat(p0, p1, p2, self.tolerance):
            self.current.append(p2)
            return
        q0 = _mid(p0, p1)
        q1 = _mid(p1, p2)
        mid = _mid(q0, q1)
        self._flatten_quad(p0, q0, mid)
        self._flatten_quad(mid, q1, p2)

    def _flatten_cubic(self, p0, p1, p2, p3):
        if _cubic_flat(p0, p1, p2, p3, self.tolerance):
            self.current.append(p3)
            return
        a0, a1, a2, mid, b1, b2, b3 = _split_cubic(p0, p1, p2, p3)
        self._flatten_cubic(a0, a1, a2, mid)
        self._flatten_cubic(mid, b1, b2, b3)

    def _commit(self, close: bool):
        points = _clean_contour(self.current, close)
        self.current = []
        if len(points) >= 3:
            self.contours.append(points)


def _mid(a, b):
    return ((a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5)


def _quad_flat(p0, p1, p2, tolerance: float) -> bool:
    mx, my = (p0[0] + p2[0]) * 0.5, (p0[1] + p2[1]) * 0.5
    dx, dy = p1[0] - mx, p1[1] - my
    return dx * dx + dy * dy <= tolerance * tolerance


def _cubic_flat(p0, p1, p2, p3, tolerance: float) -> bool:
    ux, uy = p3[0] - p0[0], p3[1] - p0[1]
    length = math.hypot(ux, uy)
    if length < 1e-6:
        return math.hypot(p1[0] - p0[0], p1[1] - p0[1]) <= tolerance and math.hypot(
            p2[0] - p3[0], p2[1] - p3[1]
        ) <= tolerance
    def dist(point):
        return abs((point[0] - p0[0]) * uy - (point[1] - p0[1]) * ux) / length

    return dist(p1) <= tolerance and dist(p2) <= tolerance


def _split_cubic(p0, p1, p2, p3):
    q0 = _mid(p0, p1)
    q1 = _mid(p1, p2)
    q2 = _mid(p2, p3)
    r0 = _mid(q0, q1)
    r1 = _mid(q1, q2)
    mid = _mid(r0, r1)
    return p0, q0, r0, mid, r1, q2, p3


def _clean_contour(points, close: bool):
    cleaned: list[tuple[float, float]] = []
    for point in points:
        if cleaned and math.hypot(point[0] - cleaned[-1][0], point[1] - cleaned[-1][1]) < 0.01:
            continue
        cleaned.append(point)
    if close and len(cleaned) >= 2 and math.hypot(
        cleaned[0][0] - cleaned[-1][0], cleaned[0][1] - cleaned[-1][1]
    ) < 0.01:
        cleaned.pop()
    return cleaned


def _cmap(font):
    table = font.getBestCmap() or {}
    return table


def _cap_height(font, cmap, glyph_set) -> int:
    os2 = font["OS/2"] if "OS/2" in font else None
    if os2 is not None and getattr(os2, "sCapHeight", 0):
        return int(os2.sCapHeight)
    name = cmap.get(ord("H"))
    if name and name in glyph_set:
        bounds = glyph_set[name].getBounds(glyph_set) if hasattr(glyph_set[name], "getBounds") else None
        if bounds is not None:
            return max(1, int(round(bounds[3])))
    return int(round(font["head"].unitsPerEm * 0.72))


def export_face(font_path: Path, name: str, characters: str, tolerance: float):
    TTFont, _ = load_fonttools()
    font = TTFont(font_path)
    glyph_set = font.getGlyphSet()
    cmap = _cmap(font)
    units = int(font["head"].unitsPerEm)
    cap = _cap_height(font, cmap, glyph_set)
    source_name = _full_name(font) or font_path.stem

    points: list[tuple[int, int]] = []
    contours: list[tuple[int, int]] = []
    glyphs = []
    missing = []

    for character in characters:
        glyph_name = cmap.get(ord(character))
        if glyph_name is None or glyph_name not in glyph_set:
            missing.append(character)
            continue
        pen = FlattenPen(glyph_set, tolerance)
        glyph_set[glyph_name].draw(pen)
        if not pen.contours:
            missing.append(character)
            continue
        xs = [pt[0] for contour in pen.contours for pt in contour]
        ys = [pt[1] for contour in pen.contours for pt in contour]
        contour0 = len(contours)
        for contour in pen.contours:
            first = len(points)
            for x, y in contour:
                points.append((int(round(x)), int(round(y))))
            contours.append((first, len(contour)))
        glyphs.append(
            {
                "code": ord(character),
                "contour0": contour0,
                "contours": len(pen.contours),
                "minX": int(math.floor(min(xs))),
                "minY": int(math.floor(min(ys))),
                "maxX": int(math.ceil(max(xs))),
                "maxY": int(math.ceil(max(ys))),
            }
        )

    font.close()
    if missing:
        print("missing glyphs: " + " ".join(repr(item) for item in missing), file=sys.stderr)
    if not glyphs:
        raise SystemExit("no glyphs were exported")
    return {
        "name": name,
        "source": source_name,
        "unitsPerEm": units,
        "capHeight": cap,
        "points": points,
        "contours": contours,
        "glyphs": glyphs,
    }


def _full_name(font) -> str:
    if "name" not in font:
        return ""
    for record in font["name"].names:
        if record.nameID == 4:
            try:
                return record.toUnicode().strip()
            except UnicodeDecodeError:
                continue
    return ""


def render_header(face: dict) -> str:
    ident = cpp_ident(face["name"])
    stem = table_ident(face["name"])
    lines = [
        "// Generated by software/tools/font_to_glyphs.py. Do not edit.",
        f"// Source: {face['source']}.",
        f"// Regenerate: python3 software/tools/font_to_glyphs.py --font <ttf> --name {face['name']} --out <this file>",
        "#pragma once",
        "",
        '#include "../font_face.h"',
        "",
        "namespace DisplayLogic {",
        "namespace {",
        "",
        f"const GlyphPoint k{stem}Points[] = {{",
    ]
    for x, y in face["points"]:
        lines.append(f"    {{{x}, {y}}},")
    lines.append("};")
    lines.append("")
    lines.append(f"const GlyphContour k{stem}Contours[] = {{")
    for first, count in face["contours"]:
        lines.append(f"    {{{first}, {count}}},")
    lines.append("};")
    lines.append("")
    lines.append(f"const FontGlyph k{stem}Glyphs[] = {{")
    for glyph in face["glyphs"]:
        lines.append(
            "    {{{code}, {contour0}, {contours}, {minX}, {minY}, {maxX}, {maxY}}},".format(
                **glyph
            )
        )
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace")
    lines.append("")
    lines.append(f"const FontFace {ident} = {{")
    lines.append(f'    "{face["name"]}",')
    lines.append(f'    "{face["source"]}",')
    lines.append(f'    {face["unitsPerEm"]},')
    lines.append(f'    {face["capHeight"]},')
    lines.append(f"    k{stem}Points,")
    lines.append(f"    {len(face['points'])},")
    lines.append(f"    k{stem}Contours,")
    lines.append(f"    {len(face['contours'])},")
    lines.append(f"    k{stem}Glyphs,")
    lines.append(f"    {len(face['glyphs'])},")
    lines.append("};")
    lines.append("")
    lines.append("}  // namespace DisplayLogic")
    lines.append("")
    return "\n".join(lines)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font", required=True, help="TTF or OTF to export")
    parser.add_argument("--name", required=True, help="face id, e.g. sans_serif")
    parser.add_argument("--out", required=True, help="C++ header to write")
    parser.add_argument(
        "--chars",
        default=DEFAULT_CHARS,
        help=f"characters to export (default: {DEFAULT_CHARS})",
    )
    parser.add_argument(
        "--tolerance",
        type=float,
        default=8.0,
        help="flatten tolerance in font units (default: 8)",
    )
    args = parser.parse_args(argv)

    font_path = Path(args.font)
    if not font_path.is_file():
        raise SystemExit(f"font not found: {font_path}")

    face = export_face(font_path, args.name, args.chars, args.tolerance)
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render_header(face))
    print(
        f"wrote {out} ({len(face['glyphs'])} glyphs, "
        f"{len(face['points'])} points, {len(face['contours'])} contours) "
        f"from {face['source']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
