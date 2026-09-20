#!/usr/bin/env python3
"""Derives a playfield's unlit picture from its lit one.

    hd_unlit.py <lit.png> <unlit.png> <folder with the table's original pictures>

The game draws a lamp that is off at half brightness, so the unlit picture is the lit one
with every lamp dimmed by half. Which pixels belong to a lamp comes from the original
playfield: the ones that differ between `playfield_lights_on.png` and
`playfield_lights_off.png`, as written by `pfr-extract`. Holes inside a lamp (a monster's
eyes, the middle of a letter) count as part of it, and single stray pixels of a lamp colour
are left out.

A redrawn playfield puts the same shapes in slightly different places, so the mask is fitted
to the picture's own edges before it is used; otherwise the dimmed area cuts across the new
drawing and leaves bright rims and dark bites.

Needs Pillow, from the tools virtual environment, which the script switches to by itself.
"""
import os
import sys
from collections import deque
from pathlib import Path

try:
    from PIL import Image
except ModuleNotFoundError:
    # Not running inside the tools virtual environment: restart with its Python.
    venv = Path(__file__).resolve().parent / ".venv"
    venv_python = venv / "bin" / "python"
    if venv_python.exists() and Path(sys.prefix).resolve() != venv.resolve():
        os.execv(str(venv_python), [str(venv_python), *sys.argv])
    sys.exit("Pillow is missing. Set up the tools environment:\n"
             "  python3 -m venv tools/.venv && tools/.venv/bin/pip install -r tools/requirements.txt")

MIN_LAMP_PIXELS = 4   # smaller islands of a lamp colour are not lamps of their own
DIM = 0.5             # what the engine draws an unlit lamp at
RADIUS = 8            # how far the mask may follow an edge of the picture, in its pixels
SOFTNESS = 0.35       # how much of a blend is left at a lamp's edge


def lamp_mask(table_dir):
    """The original playfield's lamp pixels: a list of rows of bools."""
    on = Image.open(Path(table_dir) / "playfield_lights_on.png").convert("RGB")
    off = Image.open(Path(table_dir) / "playfield_lights_off.png").convert("RGB")
    if on.size != off.size:
        sys.exit("the lit and unlit originals differ in size")
    w, h = on.size
    a, b = on.load(), off.load()
    mask = [[a[x, y] != b[x, y] for x in range(w)] for y in range(h)]
    fill_holes(mask)
    drop_specks(mask)
    return mask


def neighbours(x, y, w, h):
    if x > 0:
        yield x - 1, y
    if x + 1 < w:
        yield x + 1, y
    if y > 0:
        yield x, y - 1
    if y + 1 < h:
        yield x, y + 1


def fill_holes(mask):
    """Marks everything enclosed by a lamp's outline, by flooding the outside instead."""
    h, w = len(mask), len(mask[0])
    outside = [[False] * w for _ in range(h)]
    queue = deque()
    for x in range(w):
        for y in (0, h - 1):
            if not mask[y][x] and not outside[y][x]:
                outside[y][x] = True
                queue.append((x, y))
    for y in range(h):
        for x in (0, w - 1):
            if not mask[y][x] and not outside[y][x]:
                outside[y][x] = True
                queue.append((x, y))
    while queue:
        x, y = queue.popleft()
        for nx, ny in neighbours(x, y, w, h):
            if not mask[ny][nx] and not outside[ny][nx]:
                outside[ny][nx] = True
                queue.append((nx, ny))
    for y in range(h):
        for x in range(w):
            if not outside[y][x]:
                mask[y][x] = True


def drop_specks(mask):
    """Clears lamp islands smaller than MIN_LAMP_PIXELS."""
    h, w = len(mask), len(mask[0])
    seen = [[False] * w for _ in range(h)]
    for sy in range(h):
        for sx in range(w):
            if not mask[sy][sx] or seen[sy][sx]:
                continue
            island, queue = [], deque([(sx, sy)])
            seen[sy][sx] = True
            while queue:
                x, y = queue.popleft()
                island.append((x, y))
                for nx, ny in neighbours(x, y, w, h):
                    if mask[ny][nx] and not seen[ny][nx]:
                        seen[ny][nx] = True
                        queue.append((nx, ny))
            if len(island) < MIN_LAMP_PIXELS:
                for x, y in island:
                    mask[y][x] = False


def box_blur(values, w, h, r):
    """Mean over a (2r+1) square, edges clamped, as two passes over a flat list."""
    out = [0.0] * (w * h)
    row = [0.0] * (w * h)
    for y in range(h):
        base = y * w
        total = sum(values[base + min(max(i, 0), w - 1)] for i in range(-r, r + 1))
        n = 2 * r + 1
        for x in range(w):
            row[base + x] = total / n
            total += values[base + min(x + r + 1, w - 1)] - values[base + max(x - r, 0)]
    for x in range(w):
        total = sum(row[min(max(i, 0), h - 1) * w + x] for i in range(-r, r + 1))
        n = 2 * r + 1
        for y in range(h):
            out[y * w + x] = total / n
            total += row[min(y + r + 1, h - 1) * w + x] - row[max(y - r, 0) * w + x]
    return out


def fit_to_edges(mask, guide, w, h, r=RADIUS, eps=1e-3):
    """Guided filter: smooths `mask` but keeps to the edges of `guide` (a grey picture)."""
    mean_g = box_blur(guide, w, h, r)
    mean_m = box_blur(mask, w, h, r)
    mean_gm = box_blur([g * m for g, m in zip(guide, mask)], w, h, r)
    mean_gg = box_blur([g * g for g in guide], w, h, r)
    a = [(gm - g * m) / (gg - g * g + eps) for gm, g, m, gg in zip(mean_gm, mean_g, mean_m, mean_gg)]
    b = [m - ai * g for m, ai, g in zip(mean_m, a, mean_g)]
    mean_a, mean_b = box_blur(a, w, h, r), box_blur(b, w, h, r)
    return [ai * g + bi for ai, g, bi in zip(mean_a, guide, mean_b)]


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    lit_path, out_path, table_dir = sys.argv[1:4]
    lit = Image.open(lit_path)
    flat = Image.new("RGBA", lit.size, (0, 0, 0, 255))
    flat.alpha_composite(lit.convert("RGBA"))
    lit = flat.convert("RGB")
    w, h = lit.size

    mask = lamp_mask(table_dir)
    scaled = Image.frombytes("L", (len(mask[0]), len(mask)),
                             bytes(255 if v else 0 for row in mask for v in row)).resize((w, h), Image.NEAREST)
    values = [v / 255.0 for v in scaled.tobytes()]
    guide = [v / 255.0 for v in lit.convert("L").tobytes()]
    alpha = fit_to_edges(values, guide, w, h)
    # Pull the soft edge back towards a decision, leaving a pixel or two of blend.
    alpha = [min(max((v - 0.5) / SOFTNESS + 0.5, 0.0), 1.0) for v in alpha]

    rgb = lit.tobytes()
    dimmed = bytes(round(rgb[i * 3 + c] * (1 - DIM * a)) for i, a in enumerate(alpha) for c in range(3))
    Image.frombytes("RGB", (w, h), dimmed).save(out_path)
    lamps = sum(sum(row) for row in mask) / (len(mask) * len(mask[0]))
    print(f"{out_path}: lamps dimmed to {DIM:.0%}, {lamps:.1%} of the playfield")


if __name__ == "__main__":
    main()
