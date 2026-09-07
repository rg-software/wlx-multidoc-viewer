#!/usr/bin/env python3
"""Generate original synthetic sample images for wlx-multidoc-viewer examples/.

Produces: sample1.jpg, sample2.png, sample3.bmp, sample4.webp, sample5.tiff,
sample-animated.gif. All content is generated programmatically (gradients,
shapes, frame sequences) so no third-party copyrights apply.
"""
from PIL import Image, ImageDraw, ImageFont
import math
import os

OUT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "examples"))
os.makedirs(OUT, exist_ok=True)


def _font(size):
    try:
        return ImageFont.truetype("arial.ttf", size)
    except Exception:
        return ImageFont.load_default()


def lerp(a, b, t):
    return int(a + (b - a) * t)


def gradient(w, h, corners):
    """Bilinear gradient from an (tl, tr, bl, br) 2x2 corner map.

    ``corners`` is a tuple of 4 RGB tuples: top-left, top-right, bottom-left,
    bottom-right. Built from an L ramp so no per-pixel Python loops run.
    """
    tl, tr, bl, br = corners
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        ty = y / max(1, h - 1)
        for x in range(w):
            tx = x / max(1, w - 1)
            top = (lerp(tl[0], tr[0], tx), lerp(tl[1], tr[1], tx), lerp(tl[2], tr[2], tx))
            bot = (lerp(bl[0], br[0], tx), lerp(bl[1], br[1], tx), lerp(bl[2], br[2], tx))
            px[x, y] = (lerp(top[0], bot[0], ty), lerp(top[1], bot[1], ty), lerp(top[2], bot[2], ty))
    return img


def label(draw, title, subtitle=""):
    draw.text((16, 14), title, fill=(255, 255, 255), font=_font(26))
    if subtitle:
        draw.text((16, 52), subtitle, fill=(225, 225, 225), font=_font(16))


# --- JPEG: warm horizontal gradient (`load()` works here) + ellipse ---
img = gradient(640, 400, ((180, 60, 20), (30, 90, 200), (210, 110, 40), (50, 120, 220)))
d = ImageDraw.Draw(img)
d.ellipse([460, 240, 600, 360], fill=(250, 220, 120), outline=(200, 120, 20), width=4)
label(d, "Sample JPEG", "wxm sample, gradient + shape")
img.save(os.path.join(OUT, "sample1.jpg"), "JPEG", quality=92)
print("wrote sample1.jpg")

# --- PNG: checkerboard + circle ---
img = Image.new("RGB", (480, 360), (248, 248, 244))
d = ImageDraw.Draw(img)
for iy in range(0, 360, 24):
    for ix in range(0, 480, 24):
        if (ix // 24 + iy // 24) % 2:
            d.rectangle([ix, iy, ix + 23, iy + 23], fill=(70, 70, 92))
d.ellipse([160, 120, 320, 280], fill=(40, 160, 90), outline=(20, 90, 50), width=5)
label(d, "Sample PNG", "wxm sample, checkerboard")
img.save(os.path.join(OUT, "sample2.png"), "PNG")
print("wrote sample2.png")

# --- BMP: cool vertical gradient + rect ---
img = gradient(360, 360, ((10, 80, 180), (10, 120, 200), (70, 200, 240), (90, 230, 255)))
d = ImageDraw.Draw(img)
d.rectangle([60, 60, 300, 300], fill=(240, 240, 220), outline=(0, 0, 0), width=3)
d.line([60, 60, 300, 300], fill=(200, 30, 30), width=8)
label(d, "Sample BMP", "wxm sample, vertical gradient")
img.save(os.path.join(OUT, "sample3.bmp"))
print("wrote sample3.bmp")

# --- WEBP: diagonal stripe ---
img = Image.new("RGB", (512, 320), (245, 245, 235))
d = ImageDraw.Draw(img)
for i in range(-320, 512, 48):
    d.line([i, 0, i + 320, 320], fill=(120, 40, 160), width=16)
label(d, "Sample WEBP", "wxm sample, diagonal stripes")
img.save(os.path.join(OUT, "sample4.webp"), "WEBP", quality=88)
print("wrote sample4.webp")

# --- TIFF (single-frame) ---
img = gradient(420, 300, ((20, 120, 20), (100, 190, 90), (160, 220, 60), (220, 220, 60)))
d = ImageDraw.Draw(img)
d.polygon([(210, 40), (120, 260), (300, 260)], fill=(30, 60, 120), outline=(255, 255, 255))
label(d, "Sample TIFF", "wxm sample, triangle")
img.save(os.path.join(OUT, "sample5.tiff"), "TIFF")
print("wrote sample5.tiff")

# --- Animated GIF: 12 frames, moving dot + counter ---
W, H = 320, 240
frames = []
for i in range(12):
    f = gradient(W, H, ((30, 80, 160), (90, 140, 200), (120, 170, 130), (160, 200, 60)))
    d = ImageDraw.Draw(f)
    cx = 40 + i * 22
    cy = H // 2 + int(40 * math.sin(i * 0.7))
    d.ellipse([cx - 28, cy - 28, cx + 28, cy + 28],
              fill=(230, 60, 40), outline=(40, 20, 60), width=3)
    d.text((10, 12), "%d / 12" % (i + 1), fill=(255, 255, 255))
    frames.append(f.convert("RGB"))
frames[0].save(
    os.path.join(OUT, "sample-animated.gif"),
    save_all=True, append_images=frames[1:], duration=120, loop=0,
)
print("wrote sample-animated.gif (12 frames, 120ms)")
print("done ->", OUT)