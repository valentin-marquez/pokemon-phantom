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

# Grietas radiales desde el centro, con ramas -- densas y largas para que el
# impacto se lea como un golpe real y no un motivo chico: llegan casi al
# borde del sprite 64x64 (algunas se recortan al tocar el borde, lo cual luce
# bien: imita el vidrio partiéndose hasta el marco). Este mismo motivo se usa
# en phantom_intro.c en un CLUSTER de copias (mismo tile, distintas
# posiciones) para el efecto "dramático a pantalla casi completa" -- que cada
# tile individual llegue bien al borde ayuda a que las copias se lean como
# una sola red de grietas en vez de 5 rombos sueltos.
NUM_RAYS = 16
for i in range(NUM_RAYS):
    ang = i * (2 * math.pi / NUM_RAYS) + random.uniform(-0.2, 0.2)
    length = random.randint(29, 34)
    ex = int(CX + math.cos(ang) * length)
    ey = int(CY + math.sin(ang) * length)
    line(CX, CY, ex, ey, 1)
    # engrosar el tercio interior del rayo (más creíble como impacto real)
    perp = ang + math.pi / 2
    ox, oy = int(math.cos(perp)), int(math.sin(perp))
    mx0 = int(CX + math.cos(ang) * length * 0.35)
    my0 = int(CY + math.sin(ang) * length * 0.35)
    line(CX + ox, CY + oy, mx0 + ox, my0 + oy, 2)

    # rama primaria
    bang = ang + random.uniform(-0.7, 0.7)
    blen = random.randint(8, 14)
    mx = int(CX + math.cos(ang) * length * 0.55)
    my = int(CY + math.sin(ang) * length * 0.55)
    bex = int(mx + math.cos(bang) * blen)
    bey = int(my + math.sin(bang) * blen)
    line(mx, my, bex, bey, 2)

    # rama secundaria, más fina, brotando de la primera (más densidad cerca del borde)
    b2ang = bang + random.uniform(-0.8, 0.8)
    b2len = random.randint(4, 9)
    line(bex, bey, int(bex + math.cos(b2ang) * b2len), int(bey + math.sin(b2ang) * b2len), 3)

# Anillos concéntricos (grietas circulares), incluido uno cerca del borde
# para reforzar la sensación de impacto que cubre casi todo el sprite.
for r in (10, 18, 26):
    for a in range(0, 360, 5):
        x = int(CX + math.cos(math.radians(a)) * r)
        y = int(CY + math.sin(math.radians(a)) * r)
        if 0 <= x < W and 0 <= y < H and random.random() > 0.35:
            px[x, y] = 3

img.save("graphics/phantom_intro/crack.png")
print("crack.png generado")

# --- menu.png: "NUEVA PARTIDA" / "CONTINUAR" + cursor, fuente 5x7 embebida ---
# Layout en memoria: pila VERTICAL de 4 bloques, cada uno del ANCHO COMPLETO de
# la hoja (64px). Esto importa: gbagfx parte la imagen en tiles de 8x8 en orden
# row-major sobre TODA la hoja, no por sprite. Si dos sprites de 64 de ancho
# fueran lado a lado (p. ej. una hoja de 128x32), sus filas de tiles quedarían
# intercaladas y el sprite de 1D-mapping saldría con la gráfica descompuesta.
# Apilando en vertical con el mismo ancho que cada sprite, cada bloque cae en
# un rango de tiles contiguo y en el orden que un sprite 1D-mapped espera.
#
# "NUEVA PARTIDA" (~78px a 6px/char) no entra en un solo sprite de 64px de
# ancho (máximo GBA), así que la línea se arma con DOS sprites adyacentes en
# pantalla: bloque 0 = "NUEVA", bloque 1 = "PARTIDA". Ambos textos arrancan
# con el mismo margen izquierdo (TEXT_MARGIN) dentro de su bloque, y es
# phantom_intro.c el que los posiciona pegados en X para que se lean como una
# sola línea continua. El bloque de "CONTINUAR" usa el mismo margen izquierdo
# que el bloque 0, para que ambas opciones queden alineadas a la izquierda
# (con el cursor a la izquierda de todo, per diseño).
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

TEXT_MARGIN = 2  # margen izquierdo dentro de cada bloque (texto en línea, no centrado)
TEXT_Y = 2       # fila donde arranca el texto, cerca del borde superior del bloque
                 # (deja el resto del bloque 64x32 transparente -- pegamos el
                 # texto arriba para poder acercar el sprite al logo sin mover
                 # más "aire" del que hace falta)

menu = Image.new("P", (MENU_W, BLOCK_H * 3 + CURSOR_H), 0)
menu.putpalette(palette)  # reutiliza la paleta de crack (1 = blanco)

# Bloque 0 (tiles 0..31): "NUEVA" -- mitad izquierda de la línea "NUEVA PARTIDA".
text(menu, "NUEVA", TEXT_MARGIN, TEXT_Y, 1)
# Bloque 1 (tiles 32..63): "PARTIDA" -- mitad derecha de la misma línea.
text(menu, "PARTIDA", TEXT_MARGIN, BLOCK_H + TEXT_Y, 1)
# Bloque 2 (tiles 64..95): "CONTINUAR", mismo margen izquierdo que el bloque 0.
text(menu, "CONTINUAR", TEXT_MARGIN, BLOCK_H * 2 + TEXT_Y, 1)
# Bloque 3 (tile 96): cursor '>' suelto, en la esquina superior izquierda de su
# propio tile 8x8 (se recorta como sprite aparte y se reposiciona en vivo).
mp = menu.load()
for row in range(7):
    bits = FONT['>'][row]
    for col in range(5):
        if bits & (1 << (4 - col)):
            mp[1 + col, BLOCK_H * 3 + row] = 1

menu.save("graphics/phantom_intro/menu.png")
print("menu.png generado")
