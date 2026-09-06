#!/usr/bin/env python3
"""
Generate the Pouch Mk1 cutting pattern.

    python3 pattern.py

Writes, next to itself:
    sheet1.svg, sheet2.svg   1:1 cutting sheets, print at 100% (no "fit to page")
    template.dxf             front-panel outline + aperture centres, for the
                             laser-cut acrylic marking template

EVERY PIECE IS CUT TO FINISHED SIZE. The pouch is bound-edge construction, so
there is no seam allowance anywhere - adding one is the classic way to end up
with a pouch 6mm too big in every direction.

Coordinates within a piece are millimetres from its bottom-left corner, x
across, y up. This matches the build sheet and the DXF.
"""
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))

# ---- the design ------------------------------------------------------
W, H_BODY, H_FLAP, R = 78.0, 62.0, 52.0, 14.0
H_FRONT = H_BODY + H_FLAP            # 114: front panel and flap are one piece
DEPTH = 24.0                         # gusset width = pouch depth

FOLD_1 = 62.0     # top of the body: the flap begins
FOLD_2 = 86.0     # 24mm on: the flap turns down the back

# Apertures: name, x, y, diameter. Solved for maximum mic spread on a blank
# panel - 46/37/37mm, against the printed medallion's 18.8/20.2/29.2 where the
# sculpt was in the way.
APERTURES = [
    ("mic A",   16.0, 44.0,  3.0),
    ("mic B",   62.0, 44.0,  3.0),
    ("mic C",   39.0, 15.0,  3.0),
    ("camera",  39.0, 33.0, 10.0),
    ("LED",     39.0, 53.0,  5.0),
    ("button",  62.0, 15.0, 12.5),
    ("snap",    39.0, 102.0, 4.0),
]

# Gusset length is the front panel's perimeter minus its top edge: down one
# side, round both bottom arcs, up the other. Not the full perimeter - the
# top is the opening.
GUSSET_RUN = 2*(H_BODY - R) + (W - 2*R) + 2*(math.pi*R/2)
GUSSET_CUT = 205.0                   # cut long, trim to fit at assembly

# label_at is in piece coordinates, chosen per piece to land in dead space;
# rot=90 for pieces too narrow to take the name across.
PIECES = [
    dict(id="front",  label="FRONT + FLAP",  mat="Veg-tan 4-5 oz", qty=1,
         w=W, h=H_FRONT, r=R, folds=[FOLD_1, FOLD_2], apertures=APERTURES,
         label_at=(7, 76)),
    dict(id="back",   label="BACK PANEL",    mat="Laminate",       qty=1,
         w=W, h=H_BODY,  r=R, folds=[], apertures=[("snap socket", 39.0, 46.0, 4.0)],
         label_at=(7, 18)),
    dict(id="gusset", label="GUSSET",        mat="Laminate",       qty=1,
         w=DEPTH, h=GUSSET_CUT, r=0, folds=[], apertures=[],
         notch=[GUSSET_CUT/2], label_at=(9, 150), rot=90),
    dict(id="sleeve", label="NAPE SLEEVE",   mat="Laminate",       qty=1,
         w=40.0, h=180.0, r=0, folds=[20.0], apertures=[],
         label_at=(13, 120), rot=90),
    dict(id="tab",    label="D-RING TAB",    mat="Veg-tan 4-5 oz", qty=2,
         w=20.0, h=60.0, r=0, folds=[30.0], apertures=[],
         label_at=(13, 44), rot=90),
    dict(id="patch",  label="SNAP PATCH",    mat="Veg-tan 4-5 oz", qty=1,
         w=30.0, h=30.0, r=6, folds=[], apertures=[("socket", 15.0, 15.0, 4.0)],
         label_at=(6, 6)),
]

# Two sheets, sized to the area common to A4 and US Letter (190 x 259mm).
SHEETS = [
    ("sheet1.svg", [("front", 12, 14), ("back", 104, 14)], (104, 92), (104, 160)),
    ("sheet2.svg", [("gusset", 12, 14), ("sleeve", 48, 14),
                    ("tab", 100, 14), ("tab", 128, 14), ("patch", 100, 84)],
     (100, 130), (100, 190)),
]
# 210 x 279 is the intersection of A4 and US Letter, so one file prints true
# on either without touching the scale.
PAGE_W, PAGE_H = 210.0, 279.0


def rrect_path(x, y, w, h, r):
    if r <= 0:
        return f"M{x},{y} h{w} v{h} h{-w} Z"
    return (f"M{x+r},{y} h{w-2*r} a{r},{r} 0 0 1 {r},{r} "
            f"v{h-2*r} a{r},{r} 0 0 1 {-r},{r} h{-(w-2*r)} "
            f"a{r},{r} 0 0 1 {-r},{-r} v{-(h-2*r)} a{r},{r} 0 0 1 {r},{-r} Z")


def draw_piece(p, ox, oy):
    """SVG for one piece. SVG y runs down, piece y runs up, so y is flipped
    about the piece's own height."""
    def py(v):
        return oy + p["h"] - v
    g = [f'<g id="{p["id"]}">']
    g.append(f'<path d="{rrect_path(ox, oy, p["w"], p["h"], p["r"])}" '
             f'fill="none" stroke="#111" stroke-width="0.5"/>')

    for f in p.get("folds", []):
        g.append(f'<line x1="{ox}" y1="{py(f):.2f}" x2="{ox+p["w"]}" y2="{py(f):.2f}" '
                 f'stroke="#111" stroke-width="0.35" stroke-dasharray="5 3"/>')
        g.append(f'<text x="{ox+p["w"]-2:.1f}" y="{py(f)-1.6:.2f}" font-size="3" '
                 f'font-family="monospace" fill="#111" text-anchor="end">fold {f:g}</text>')

    for n in p.get("notch", []):
        g.append(f'<line x1="{ox}" y1="{py(n):.2f}" x2="{ox+3}" y2="{py(n):.2f}" '
                 f'stroke="#111" stroke-width="0.5"/>')
        g.append(f'<line x1="{ox+p["w"]-3}" y1="{py(n):.2f}" x2="{ox+p["w"]}" y2="{py(n):.2f}" '
                 f'stroke="#111" stroke-width="0.5"/>')

    for name, ax, ay, d in p.get("apertures", []):
        cx, cy = ox + ax, py(ay)
        g.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{d/2:.2f}" '
                 f'fill="none" stroke="#111" stroke-width="0.45"/>')
        g.append(f'<path d="M{cx-d/2-2.2:.2f},{cy:.2f} h{d+4.4:.2f} '
                 f'M{cx:.2f},{cy-d/2-2.2:.2f} v{d+4.4:.2f}" '
                 f'stroke="#111" stroke-width="0.2"/>')
        g.append(f'<text x="{cx:.2f}" y="{cy-d/2-3.4:.2f}" font-size="2.7" '
                 f'font-family="monospace" fill="#111" text-anchor="middle">'
                 f'{name} &#8709;{d:g}</text>')

    qty = f' &#215;{p["qty"]}' if p["qty"] > 1 else ""
    lx, ly = p["label_at"]
    tx, ty = ox + lx, py(ly)
    rot = f' transform="rotate(-90 {tx:.2f} {ty:.2f})"' if p.get("rot") == 90 else ""
    g.append(f'<text x="{tx:.2f}" y="{ty:.2f}" font-size="4.2" font-family="sans-serif" '
             f'font-weight="bold" fill="#111"{rot}>{p["label"]}{qty}</text>')
    g.append("</g>")
    return "\n".join(g)


def sheet(filename, layout, index, legend_at, calib_at):
    by_id = {p["id"]: p for p in PIECES}
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{PAGE_W}mm" height="{PAGE_H}mm" '
        f'viewBox="0 0 {PAGE_W} {PAGE_H}">',
        f'<rect width="{PAGE_W}" height="{PAGE_H}" fill="#fff"/>',
        f'<text x="12" y="8" font-size="4" font-family="sans-serif" font-weight="bold" '
        f'fill="#111">PENDANT POUCH Mk1 &#183; cutting pattern &#183; sheet {index} of {len(SHEETS)}</text>',
        f'<text x="12" y="{PAGE_H-6:.1f}" font-size="3" font-family="monospace" fill="#444">'
        f'1:1 &#183; print at 100%, not "fit to page" &#183; cut to finished size, '
        f'edges are bound so no seam allowance is included</text>',
    ]
    seen = []
    for pid, ox, oy in layout:
        parts.append(draw_piece(by_id[pid], ox, oy))
        if pid not in seen:
            seen.append(pid)

    # legend: material and cut size live here, not crammed inside the piece
    lx, ly = legend_at
    parts.append(f'<text x="{lx}" y="{ly}" font-size="3.6" font-family="sans-serif" '
                 f'font-weight="bold" fill="#111">CUT LIST &#183; THIS SHEET</text>')
    for i, pid in enumerate(seen):
        p = by_id[pid]
        rr = f" R{int(p['r'])}" if p["r"] else ""
        q = f" &#215;{p['qty']}" if p["qty"] > 1 else ""
        parts.append(
            f'<text x="{lx}" y="{ly+7+i*5.2:.1f}" font-size="3" font-family="monospace" '
            f'fill="#333">{p["label"]}{q} &#183; {p["mat"]} &#183; '
            f'{p["w"]:g}&#215;{p["h"]:g}{rr}</text>')
    if any(pid == "gusset" for pid, _, _ in layout):
        parts.append(f'<text x="{lx}" y="{ly+7+len(seen)*5.2:.1f}" font-size="3" '
                     f'font-family="monospace" fill="#333">'
                     f'gusset fits {GUSSET_RUN:.0f}mm &#183; cut long, trim at assembly</text>')

    # calibration square - the only way to know the print came out true
    cx, cy = calib_at
    parts.append(f'<rect x="{cx}" y="{cy}" width="50" height="50" fill="none" '
                 f'stroke="#111" stroke-width="0.5"/>')
    parts.append(f'<text x="{cx+25}" y="{cy+27}" font-size="4" font-family="monospace" '
                 f'fill="#111" text-anchor="middle">50 mm</text>')
    parts.append(f'<text x="{cx+25}" y="{cy+33}" font-size="2.8" font-family="monospace" '
                 f'fill="#444" text-anchor="middle">measure before cutting</text>')
    parts.append("</svg>")
    path = os.path.join(HERE, filename)
    with open(path, "w") as fh:
        fh.write("\n".join(parts))
    return path


# ---- DXF (R12 ASCII: universally accepted by laser cutters) -----------
def dxf_template(path):
    e = []

    def rec(code, val):
        e.append(f"{code}\n{val}")

    def line(x1, y1, x2, y2, layer):
        rec(0, "LINE"); rec(8, layer)
        rec(10, f"{x1:.4f}"); rec(20, f"{y1:.4f}")
        rec(11, f"{x2:.4f}"); rec(21, f"{y2:.4f}")

    def arc(cx, cy, r, a0, a1, layer):
        rec(0, "ARC"); rec(8, layer)
        rec(10, f"{cx:.4f}"); rec(20, f"{cy:.4f}"); rec(40, f"{r:.4f}")
        rec(50, f"{a0:.4f}"); rec(51, f"{a1:.4f}")

    def circle(cx, cy, r, layer):
        rec(0, "CIRCLE"); rec(8, layer)
        rec(10, f"{cx:.4f}"); rec(20, f"{cy:.4f}"); rec(40, f"{r:.4f}")

    rec(0, "SECTION"); rec(2, "ENTITIES")
    L = "OUTLINE"
    line(R, 0, W-R, 0, L)
    line(W, R, W, H_FRONT-R, L)
    line(W-R, H_FRONT, R, H_FRONT, L)
    line(0, H_FRONT-R, 0, R, L)
    arc(W-R, R,          R, 270, 360, L)
    arc(W-R, H_FRONT-R,  R,   0,  90, L)
    arc(R,   H_FRONT-R,  R,  90, 180, L)
    arc(R,   R,          R, 180, 270, L)

    # 2mm centre marks to scribe through, plus the real apertures on their own
    # layer so you can cut either - the template only needs the centres.
    for _, ax, ay, d in APERTURES:
        circle(ax, ay, 1.0, "CENTRES")
        circle(ax, ay, d/2, "APERTURES")
    for f in (FOLD_1, FOLD_2):
        line(-4, f, -1, f, "FOLDS")
        line(W+1, f, W+4, f, "FOLDS")

    rec(0, "ENDSEC"); rec(0, "EOF")
    with open(path, "w") as fh:
        fh.write("\n".join(e) + "\n")
    return path


if __name__ == "__main__":
    print(f"gusset run {GUSSET_RUN:.1f}mm (cut {GUSSET_CUT:g}, trim at assembly)")
    total = 0.0
    for i, (name, layout, leg, cal) in enumerate(SHEETS, 1):
        p = sheet(name, layout, i, leg, cal)
        print(f"  {os.path.basename(p)}")
    for p in PIECES:
        total += p["w"] * p["h"] * p["qty"] / 100.0
    print(f"template.dxf -> {os.path.basename(dxf_template(os.path.join(HERE,'template.dxf')))}")
    print(f"total flat area {total:.0f} cm2 per unit (before strap)")
