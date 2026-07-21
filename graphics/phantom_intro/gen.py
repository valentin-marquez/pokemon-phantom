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

# --- menu.png: "NUEVA PARTIDA" / "CONTINUAR" + cursor, fuente 5x7 embebida ---
# Layout en memoria: pila VERTICAL de 3 bloques, cada uno del ANCHO COMPLETO de
# la hoja (64px). Esto importa: gbagfx parte la imagen en tiles de 8x8 en orden
# row-major sobre TODA la hoja, no por sprite. Si dos sprites de 64 de ancho
# fueran lado a lado (p. ej. una hoja de 128x32), sus filas de tiles quedarían
# intercaladas y el sprite de 1D-mapping saldría con la gráfica descompuesta.
# Apilando en vertical con el mismo ancho que cada sprite, cada bloque cae en
# un rango de tiles contiguo y en el orden que un sprite 1D-mapped espera.
FONT = {  # 5x7, filas de bits (1 = pixel encendido)
    'A': [0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11], 'C': [0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E],
    'D': [0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E],
    'E': [0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F], 'I': [0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E],
    'N': [0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11], 'O': [0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
    'P': [0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10], 'R': [0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11],
    'T': [0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04], 'U': [0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E],
    'V': [0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04], '>': [0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08],
    ' ': [0, 0, 0, 0, 0, 0, 0],
}


def text(img, s, x, y, color):
    p = img.load()
    for ch in s:
        g = FONT.get(ch, FONT[' '])
        for row in range(7):
            bits = g[row]
            for col in range(5):
                if bits & (1 << (4 - col)):
                    p[x + col, y + row] = color
        x += 6
    return x


MENU_W = 64      # ancho de un sprite SPRITE_SIZE(64x32)
BLOCK_H = 32     # alto de cada bloque de etiqueta (64x32 = 32 tiles)
CURSOR_H = 8     # alto del bloque del cursor (8x8 = 1 tile)

menu = Image.new("P", (MENU_W, BLOCK_H * 2 + CURSOR_H), 0)
menu.putpalette(palette)  # reutiliza la paleta de crack (1 = blanco)

# Bloque 0 (tiles 0..31): "NUEVA" / "PARTIDA" en dos líneas, centradas.
text(menu, "NUEVA", 17, 4, 1)
text(menu, "PARTIDA", 11, 16, 1)
# Bloque 1 (tiles 32..63): "CONTINUAR", centrada.
text(menu, "CONTINUAR", 5, BLOCK_H + 12, 1)
# Bloque 2 (tile 64): cursor '>' suelto, en la esquina superior izquierda de su
# propio tile 8x8 (se recorta como sprite aparte y se reposiciona en vivo).
mp = menu.load()
for row in range(7):
    bits = FONT['>'][row]
    for col in range(5):
        if bits & (1 << (4 - col)):
            mp[1 + col, BLOCK_H * 2 + row] = 1

menu.save("graphics/phantom_intro/menu.png")
print("menu.png generado")
