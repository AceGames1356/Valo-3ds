#!/usr/bin/env python3
"""Generates the original icon / banner / silent banner audio used for packaging.
Requires Pillow. The generated files are committed, so this is only needed to regenerate."""
import wave, struct
from PIL import Image, ImageDraw, ImageFont

def v_mark(d, ox, oy, s, col):
    d.polygon([(ox, oy), (ox + 0.28*s, oy), (ox + 0.5*s, oy + 0.62*s), (ox + 0.72*s, oy), (ox + s, oy),
               (ox + 0.62*s, oy + s), (ox + 0.38*s, oy + s)], fill=col)

# 48x48 icon
ic = Image.new("RGB", (48, 48), (14, 17, 26)); d = ImageDraw.Draw(ic)
d.rectangle([0, 0, 47, 47], outline=(236, 72, 86), width=2)
v_mark(d, 8, 8, 32, (236, 72, 86))
d.rectangle([4, 42, 43, 44], fill=(255, 210, 90))
ic.save("assets/icon.png")

# 256x128 banner
bn = Image.new("RGB", (256, 128), (14, 17, 26)); d = ImageDraw.Draw(bn)
for y in range(128):
    c = int(20 + 30 * y / 128); d.line([(0, y), (256, y)], fill=(c // 2, c // 2 + 4, c))
v_mark(d, 20, 24, 80, (236, 72, 86))
try: f = ImageFont.load_default(size=30)
except TypeError: f = ImageFont.load_default()
d.text((112, 36), "VALORANT", fill=(240, 244, 250), font=f)
d.text((112, 70), "3DS", fill=(255, 210, 90), font=f)
bn.save("assets/banner.png")

# 1 second of silence for the banner jingle
w = wave.open("assets/banner.wav", "wb"); w.setnchannels(2); w.setsampwidth(2); w.setframerate(32728)
w.writeframes(struct.pack("<hh", 0, 0) * 32728); w.close()
print("assets generated")
