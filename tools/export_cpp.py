#!/usr/bin/env python3
"""Превращает snare_pixelart.png (пиксели увеличены в SCALE раз) в C++ массивы:
палитра + индекс цвета на каждый пиксель арта.
    python3 tools/export_cpp.py assets/snare_pixelart.png src/SnareImageData.cpp --scale 7
"""
import argparse
import numpy as np
from PIL import Image

ap = argparse.ArgumentParser()
ap.add_argument("png"); ap.add_argument("out")
ap.add_argument("--scale", type=int, default=7)
a = ap.parse_args()

im = np.array(Image.open(a.png).convert("RGB"))
s = a.scale
h, w = im.shape[0] // s, im.shape[1] // s
grid = im[s // 2::s, s // 2::s][:h, :w]                  # центр каждой клетки
pal, idx = np.unique(grid.reshape(-1, 3), axis=0, return_inverse=True)
idx = idx.reshape(h, w)

with open(a.out, "w", encoding="utf-8") as f:
    f.write('// Сгенерировано tools/export_cpp.py. Не править вручную.\n#include "SnareImage.h"\n\nnamespace snareimg {\n\n')
    f.write(f"static_assert(kWidth == {w} && kHeight == {h} && kPaletteSize == {len(pal)}, \"размеры не совпадают с SnareImage.h\");\n\n")
    f.write("const uint8_t kPalette[kPaletteSize][3] = {\n")
    for r, g, b in pal:
        f.write(f"    {{{r}, {g}, {b}}},\n")
    f.write("};\n\nconst uint8_t kPixels[kWidth * kHeight] = {\n")
    for y in range(h):
        f.write("    " + ",".join(str(int(v)) for v in idx[y]) + ",\n")
    f.write("};\n\n} // namespace snareimg\n")
print(f"{w}x{h}, палитра {len(pal)} -> {a.out}")
