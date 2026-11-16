#!/usr/bin/env python3
"""Макет окна плагина-триггера: сверху настройки, снизу пиксельный барабан.
    python3 tools/make_trigger_mockup.py assets/snare_pixelart.png trigger_mockup.png
Нужны: pillow, numpy. Размер пикселя арта в PNG — 7 px (как делает pixelart_to_svg.py --scale 7)."""
import sys, math
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ART, OUT = (sys.argv[1] if len(sys.argv) > 1 else "assets/snare_pixelart.png"), (sys.argv[2] if len(sys.argv) > 2 else "trigger_mockup.png")
np.random.seed(7)
S = 2
W, TOP, BOT = 1070, 346, 400
H = TOP + BOT
img = Image.new("RGB", (W*S, H*S), (80, 81, 86))
d = ImageDraw.Draw(img)

def F(sz, bold=True):
    return ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans%s.ttf" % ("-Bold" if bold else ""), int(sz*S))
def T(x, y, t, sz, fill=(255, 255, 255), bold=True, anchor="ma"):
    d.text((x*S, y*S), t, font=F(sz, bold), fill=fill, anchor=anchor)
def R(b, r=4, fill=None, outline=None, w=1):
    d.rounded_rectangle([v*S for v in b], r*S, fill=fill, outline=outline, width=int(w*S))

for y in range(TOP*S):                                   # фон верхней части
    c = int(98 - 30*y/(TOP*S))
    d.line([(0, y), (W*S, y)], fill=(c, c+1, c+4))

def panel(x0, x1, y0=8, y1=338):
    R((x0, y0, x1, y1), 5, fill=(62, 63, 68), outline=(36, 37, 41), w=2)

def knob(cx, cy, r, val, ring=(206, 84, 66)):
    cx, cy, r = cx*S, cy*S, r*S
    d.ellipse([cx-r-3*S, cy-r-3*S, cx+r+3*S, cy+r+3*S], fill=(34, 35, 38))
    d.ellipse([cx-r, cy-r, cx+r, cy+r], fill=ring)
    d.ellipse([cx-r*.78, cy-r*.78, cx+r*.78, cy+r*.78], fill=(74, 72, 76))
    d.ellipse([cx-r*.6, cy-r*.6, cx+r*.6, cy+r*.6], fill=(120, 112, 112))
    a = math.radians(135 + 270*val)
    d.line([cx+math.cos(a)*r*.15, cy+math.sin(a)*r*.15, cx+math.cos(a)*r*.85, cy+math.sin(a)*r*.85], fill=(250, 250, 250), width=3*S)

def lcd(x0, y0, x1, y1, txt):
    R((x0, y0, x1, y1), 3, fill=(247, 247, 247), outline=(30, 30, 33), w=1)
    T((x0+x1)/2, (y0+y1)/2-6, txt, 11, fill=(30, 30, 34))

def meter(x0, y0, w, h, lit):
    R((x0, y0, x0+w, y0+h), 3, fill=(40, 41, 45), outline=(26, 27, 30))
    for i, lab in enumerate(["-0", "-3", "-6", "-12", "-24", "-48"]):
        yy = y0 + 8 + i*(h-16)/5
        T(x0+w/2, yy-6, lab, 8, fill=(230, 232, 236))
        for xx in (x0+7, x0+w-15):
            R((xx, yy-3, xx+8, yy+3), 1, fill=(60, 140, 255) if i >= 6-lit else (64, 70, 82))

BLUE = (60, 120, 220)
# GATE
panel(2, 90); T(46, 14, "GATE", 13)
R((30, 40, 62, 62), 5, fill=(60, 130, 235), outline=(180, 215, 255), w=2); T(46, 44, "On", 11)
for yy, lab, v, val in ((76, "ATTACK", "0.5 ms", .12), (160, "HOLD", "4 ms", .22), (246, "RELEASE", "450 ms", .55)):
    T(46, yy, lab, 12); knob(46, yy+34, 15, val); lcd(10, yy+56, 82, yy+74, v)
# INPUT
panel(96, 182); T(139, 14, "INPUT", 13); meter(108, 40, 62, 112, 1)
T(139, 166, "VOLUME", 12); knob(139, 200, 14, .5, BLUE); lcd(103, 222, 175, 240, "0.00 dB")
# фильтры
T(194, 22, "LOWCUT", 13, anchor="la"); knob(208, 62, 13, 0, BLUE); lcd(232, 50, 300, 72, "0 hz")
T(194, 94, "HIGHCUT", 13, anchor="la"); knob(208, 132, 13, 1, BLUE); lcd(232, 120, 300, 142, "20000 hz")
T(194, 264, "SUPPRESS", 13, anchor="la"); knob(208, 300, 15, 0); lcd(232, 288, 300, 312, "0%")

# волна
R((310, 8, 830, 338), 4, fill=(11, 12, 16), outline=(30, 31, 34), w=2)
T(318, 14, "Snare take 01", 12, fill=(120, 124, 132), anchor="la")
cy, nx, AMP = 175, 518, 125
hits = [(70, .95), (178, .8), (275, .55), (316, .7), (344, .62), (382, .9), (470, .75)]
noise = np.abs(np.random.randn(nx + 200))
sig = np.zeros(nx)
for x0, a in hits:
    for i in range(0, 100):
        if x0 + i < nx:
            env = a * math.exp(-i/16) * (1 if i > 1 else .35)
            sig[x0 + i] = max(sig[x0 + i], env * (0.5 + 0.5*min(noise[x0 + i], 1.6)/1.6))
sig = np.maximum(sig, 0.035 * noise[:nx] / 1.5)
for x in range(nx):
    h = min(sig[x], 1.0) * AMP
    d.line([((311+x)*S, (cy-h)*S), ((311+x)*S, (cy+h)*S)], fill=(52, 160, 255), width=S)
for x0, a in hits:
    xx = 311 + x0
    d.line([(xx*S, 12*S), (xx*S, 334*S)], fill=(95, 98, 108), width=S)
    h = a * AMP
    d.polygon([((xx-4)*S, (cy-h-5)*S), ((xx+4)*S, (cy-h-5)*S), (xx*S, (cy-h+2)*S)], fill=(255, 140, 50))
    d.polygon([((xx-4)*S, (cy+h+5)*S), ((xx+4)*S, (cy+h+5)*S), (xx*S, (cy+h-2)*S)], fill=(255, 140, 50))
for yy in (cy-23, cy+23):
    d.line([(311*S, yy*S), (829*S, yy*S)], fill=(225, 228, 232), width=2*S)

# справа
for yy, lab, v, val, big in ((58, "SENSITIVITY", "51", .55, 20), (146, "RETRIGGER", "16 ms", .18, 18), (233, "DETAIL", "14", .12, 20)):
    T(905, yy-12, lab, 16); knob(860, yy+36, big, val); lcd(893, yy+18, 965, yy+52, v)
panel(976, 1068); T(1022, 14, "OUTPUT", 13); meter(992, 40, 62, 112, 1)
T(1022, 160, "VOLUME", 12); knob(1022, 196, 13, .5, BLUE); lcd(988, 216, 1056, 234, "0.00 dB")
T(1022, 244, "MIX", 12); knob(1022, 276, 13, 1, BLUE); lcd(988, 300, 1056, 322, "100%")

# низ: пиксельный барабан
d.rectangle([0, TOP*S, W*S, H*S], fill=(188, 198, 203))
d.rectangle([0, TOP*S, W*S, (TOP+3)*S], fill=(30, 31, 35))
a = np.array(Image.open(ART).convert("RGB"))
grid = a[3::7, 3::7][:124, :186]
sc = 3
img.paste(Image.fromarray(grid).resize((186*sc*S, 124*sc*S), Image.NEAREST),
          (((W - 186*sc)//2)*S, (TOP + (BOT - 124*sc)//2 + 4)*S))

def card(x0, y0, x1, y1, title):
    R((x0, y0, x1, y1), 6, fill=(52, 53, 58), outline=(30, 31, 34), w=2)
    T(x0+12, y0+8, title, 11, fill=(235, 237, 240), anchor="la")
cx0, cy0 = 18, TOP+22
card(cx0, cy0, 236, cy0+176, "SAMPLES")
for i, (n, rng) in enumerate((("snare_soft.wav", "1-60"), ("snare_mid.wav", "61-100"), ("snare_hard.wav", "101-127"))):
    yy = cy0+34+i*34
    R((cx0+10, yy, 226, yy+26), 3, fill=(18, 19, 22), outline=(70, 72, 78))
    d.ellipse([(cx0+18)*S, (yy+9)*S, (cx0+26)*S, (yy+17)*S], fill=(60, 140, 255))
    T(cx0+32, yy+6, n, 10, fill=(225, 228, 232), bold=False, anchor="la")
    T(220, yy+6, rng, 10, fill=(140, 146, 156), bold=False, anchor="ra")
R((cx0+10, cy0+140, 226, cy0+166), 3, outline=(120, 124, 132))
T((cx0+236)/2, cy0+147, "+  Drop WAV here", 10, fill=(190, 194, 202), bold=False)
rx0 = W-18-218
card(rx0, cy0, W-18, cy0+176, "REPLACE")
T(rx0+12, cy0+34, "Mode", 10, fill=(150, 155, 164), bold=False, anchor="la")
for i, (t, on) in enumerate((("Replace", True), ("Blend", False))):
    x0 = rx0+60+i*74
    R((x0, cy0+30, x0+68, cy0+52), 4, fill=(60, 130, 235) if on else (36, 37, 41), outline=(26, 27, 30))
    T(x0+34, cy0+34, t, 10, fill=(255, 255, 255) if on else (170, 175, 184))
T(rx0+12, cy0+70, "Velocity", 10, fill=(150, 155, 164), bold=False, anchor="la"); lcd(rx0+90, cy0+66, rx0+206, cy0+88, "Dynamic")
T(rx0+12, cy0+102, "Round robin", 10, fill=(150, 155, 164), bold=False, anchor="la"); lcd(rx0+120, cy0+98, rx0+206, cy0+120, "3")
R((rx0+12, cy0+134, rx0+206, cy0+162), 5, fill=(36, 37, 41), outline=(26, 27, 30))
T(rx0+109, cy0+141, "▶  Audition", 12, fill=(235, 237, 240))

img.resize((W, H), Image.LANCZOS).save(OUT)
print("OK", OUT, W, H)
