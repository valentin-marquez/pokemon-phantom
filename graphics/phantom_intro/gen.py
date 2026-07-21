#!/usr/bin/env python3
"""Genera crack.png: impacto de vidrio (grietas radiales) para el front-end.
Programático, sin arte a mano. Paleta indexada de 16 (índice 0 = transparente)."""
import math, random
from PIL import Image

W = H = 64
CX = CY = 32
random.seed(7)  # determinista (reproducible en el repo)

# Paleta: 0 transparente (magenta convención gbagfx), 1 blanco, 2 gris claro, 3 gris.
palette = [255, 0, 255,  248, 248, 248,  200, 200, 208,  120, 120, 130]
palette += [0, 0, 0] * (16 - len(palette) // 3)

img = Image.new("P", (W, H), 0)
img.putpalette(palette)
px = img.load()

def line(x0, y0, x1, y1, color):
    dx, dy = abs(x1 - x0), abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx - dy
    while True:
        if 0 <= x0 < W and 0 <= y0 < H:
            px[x0, y0] = color
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 > -dy: err -= dy; x0 += sx
        if e2 < dx:  err += dx; y0 += sy

# Grietas radiales desde el centro, con ramas.
for i in range(9):
    ang = i * (2 * math.pi / 9) + random.uniform(-0.2, 0.2)
    length = random.randint(22, 30)
    ex = int(CX + math.cos(ang) * length)
    ey = int(CY + math.sin(ang) * length)
    line(CX, CY, ex, ey, 1)
    # rama
    bang = ang + random.uniform(-0.7, 0.7)
    blen = random.randint(6, 12)
    mx = int(CX + math.cos(ang) * length * 0.6)
    my = int(CY + math.sin(ang) * length * 0.6)
    line(mx, my, int(mx + math.cos(bang) * blen), int(my + math.sin(bang) * blen), 2)

# Anillos concéntricos tenues (grietas circulares).
for r in (10, 18):
    for a in range(0, 360, 6):
        x = int(CX + math.cos(math.radians(a)) * r)
        y = int(CY + math.sin(math.radians(a)) * r)
        if 0 <= x < W and 0 <= y < H and random.random() > 0.35:
            px[x, y] = 3

img.save("graphics/phantom_intro/crack.png")
print("crack.png generado")
