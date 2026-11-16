#!/usr/bin/env python3
"""
Восстанавливает «настоящую» пиксельную сетку у картинки в стиле пиксель-арт
(ИИ-картинки обычно имеют дробный размер пикселя, например 6.78 px) и
перерисовывает её кодом: чистый PNG + SVG из прямоугольников.

Запуск:
    python3 pixelart_to_svg.py картинка.png
    python3 pixelart_to_svg.py картинка.png --colors 28 --scale 8 --out drum

Нужны: pip install pillow numpy
"""
import argparse
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("src")
ap.add_argument("--colors", type=int, default=32, help="размер палитры (0 = не квантовать)")
ap.add_argument("--scale", type=int, default=8, help="во сколько раз увеличить PNG")
ap.add_argument("--period", type=float, default=0, help="размер пикселя в px (0 = найти автоматически)")
ap.add_argument("--out", default="snare_pixelart")
args = ap.parse_args()

img = Image.open(args.src).convert("RGB")
a = np.array(img).astype(np.float32)
H, W, _ = a.shape


# ---------- 1. поиск периода и фазы сетки по силе границ ----------
def edge_profile(axis):
    return np.abs(np.diff(a, axis=axis)).sum(2).sum(axis=1 - axis)


def find_grid(e, p_range=(4.0, 12.0), step=0.01):
    n = len(e)
    best = (-1, 0, 0)
    for p in np.arange(*p_range, step):
        for ph in np.arange(0, p, 0.25):
            pos = np.round(np.arange(ph, n, p)).astype(int)
            pos = pos[pos < n]
            score = e[pos].mean()
            if score > best[0]:
                best = (score, p, ph)
    return best[1], best[2]


ex, ey = edge_profile(1), edge_profile(0)   # ex: границы по x, ey: по y
if args.period:
    px = py = args.period
    phx = phy = 0.0
else:
    px, phx = find_grid(ex)
    py, phy = find_grid(ey)
    p = (px + py) / 2                      # пиксель квадратный
    px = py = round(p, 2)
print(f"Размер пикселя ≈ {px} px, сдвиг сетки x={phx:.2f}, y={phy:.2f}")

nx = int((W - phx) // px)
ny = int((H - phy) // py)

# ---------- 2. цвет каждой клетки = медиана её центральной части ----------
grid = np.zeros((ny, nx, 3), np.uint8)
for j in range(ny):
    y0, y1 = phy + j * py, phy + (j + 1) * py
    ya, yb = int(round(y0 + py * 0.25)), max(int(round(y1 - py * 0.25)), int(round(y0 + py * 0.25)) + 1)
    for i in range(nx):
        x0, x1 = phx + i * px, phx + (i + 1) * px
        xa, xb = int(round(x0 + px * 0.25)), max(int(round(x1 - px * 0.25)), int(round(x0 + px * 0.25)) + 1)
        grid[j, i] = np.median(a[ya:yb, xa:xb].reshape(-1, 3), axis=0)

# ---------- 3. палитра (без дизеринга — он уже есть в самой картинке) ----------
if args.colors > 0:
    q = Image.fromarray(grid).quantize(colors=args.colors, method=Image.Quantize.MEDIANCUT,
                                       dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[: args.colors * 3], np.uint8).reshape(-1, 3)
    idx = np.array(q)
    grid = pal[idx]
else:
    pal, inv = np.unique(grid.reshape(-1, 3), axis=0, return_inverse=True)
    idx = inv.reshape(ny, nx)

# ---------- 4. PNG ----------
Image.fromarray(grid).resize((nx * args.scale, ny * args.scale), Image.NEAREST).save(f"{args.out}.png")

# ---------- 5. SVG: отрезки одного цвета по строкам, по одному <path> на цвет ----------
paths = {}
for y in range(ny):
    x = 0
    while x < nx:
        c = int(idx[y, x])
        x2 = x
        while x2 + 1 < nx and idx[y, x2 + 1] == c:
            x2 += 1
        w = x2 - x + 1
        paths.setdefault(c, []).append(f"M{x} {y}h{w}v1h-{w}z")
        x = x2 + 1

bg = pal[int(idx[0, 0])]
with open(f"{args.out}.svg", "w", encoding="utf-8") as f:
    f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {nx} {ny}" '
            f'width="{nx * args.scale}" height="{ny * args.scale}" shape-rendering="crispEdges">\n')
    f.write(f'<rect width="{nx}" height="{ny}" fill="rgb({bg[0]},{bg[1]},{bg[2]})"/>\n')
    for c, segs in paths.items():
        r, g, b = (int(v) for v in pal[c])
        f.write(f'<path fill="rgb({r},{g},{b})" d="{"".join(segs)}"/>\n')
    f.write("</svg>\n")

print(f"Сетка {nx}x{ny}, цветов: {len(pal)} -> {args.out}.png, {args.out}.svg")
