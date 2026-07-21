#!/usr/bin/env python3
"""Genera crack.png: UN impacto de vidrio a pantalla completa (rejilla de
sprites) para el front-end. Programático, sin arte a mano. Paleta indexada
de 16 (índice 0 = transparente)."""
import math, random
from PIL import Image

random.seed(7)  # determinista (reproducible en el repo)

# Paleta: 0 transparente (magenta convención gbagfx), 1 blanco, 2 gris claro, 3 gris.
palette = [255, 0, 255,  248, 248, 248,  200, 200, 208,  120, 120, 130]
palette += [0, 0, 0] * (16 - len(palette) // 3)

def line(px, w, h, x0, y0, x1, y1, color):
    dx, dy = abs(x1 - x0), abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx - dy
    while True:
        if 0 <= x0 < w and 0 <= y0 < h:
            px[x0, y0] = color
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 > -dy: err -= dy; x0 += sx
        if e2 < dx:  err += dx; y0 += sy

# --- crack.png: UNA sola grieta de impacto (telaraña) que cubre los 240x160
# de pantalla completa, cortada en una rejilla de sprites de 64x64.
#
# GBA no tiene sprites más grandes de 64x64, así que una grieta continua a
# pantalla completa se pinta en un lienzo full-screen FS (240x160) y LUEGO se
# corta en las 12 celdas de una rejilla de 4 columnas x 3 filas -- las mismas
# coordenadas de centro de celda están hardcodeadas en phantom_intro.c
# (sCrackCellPos), deben coincidir con GRID_COLS/GRID_ROWS de abajo.
#
# Cada celda K se escribe como un bloque VERTICAL de 64x64 en la hoja de
# salida (64 de ancho, 64*12=768 de alto). Igual razón que menu.png más abajo:
# gbagfx recorre tiles row-major sobre TODA la hoja, y con mapping 1D de OBJ
# cada sprite de 64x64 necesita 64 tiles contiguos -- un bloque vertical de
# alto 64 en una hoja de ancho 64 da exactamente esos 64 tiles seguidos. Si
# las celdas fueran lado a lado (rejilla horizontal en la hoja), sus filas de
# tiles quedarían intercaladas y cada sprite saldría con la gráfica rota.
FS_W, FS_H = 240, 160
IMPACT_X, IMPACT_Y = 120, 80   # centro real de pantalla: origen ÚNICO del impacto

fs = Image.new("P", (FS_W, FS_H), 0)
fs.putpalette(palette)
fpx = fs.load()

# Grietas radiales desde el ÚNICO punto de impacto: largas (llegan a los
# bordes/esquinas de la pantalla -- la distancia máxima del centro a una
# esquina es ~144px, así que un largo de 150-175 garantiza que cada rayo
# efectivamente toque el borde antes de agotarse), cada una con 1-2 ramas.
# Una sola red conectada, no varios impactos sueltos.
NUM_RAYS = 18
for i in range(NUM_RAYS):
    ang = i * (2 * math.pi / NUM_RAYS) + random.uniform(-0.15, 0.15)
    length = random.randint(150, 175)
    ex = int(IMPACT_X + math.cos(ang) * length)
    ey = int(IMPACT_Y + math.sin(ang) * length)
    line(fpx, FS_W, FS_H, IMPACT_X, IMPACT_Y, ex, ey, 1)

    # engrosar el tramo interior del rayo (más creíble como impacto real)
    perp = ang + math.pi / 2
    ox, oy = int(math.cos(perp)), int(math.sin(perp))
    mx0 = int(IMPACT_X + math.cos(ang) * length * 0.25)
    my0 = int(IMPACT_Y + math.sin(ang) * length * 0.25)
    line(fpx, FS_W, FS_H, IMPACT_X + ox, IMPACT_Y + oy, mx0 + ox, my0 + oy, 2)

    # rama primaria, a mitad de camino del rayo
    bang = ang + random.uniform(-0.7, 0.7)
    blen = random.randint(30, 55)
    mx = int(IMPACT_X + math.cos(ang) * length * 0.5)
    my = int(IMPACT_Y + math.sin(ang) * length * 0.5)
    bex = int(mx + math.cos(bang) * blen)
    bey = int(my + math.sin(bang) * blen)
    line(fpx, FS_W, FS_H, mx, my, bex, bey, 2)

    # 1-2 ramas secundarias, más finas, brotando de la primera
    for _ in range(random.randint(1, 2)):
        b2ang = bang + random.uniform(-0.8, 0.8)
        b2len = random.randint(15, 35)
        line(fpx, FS_W, FS_H, bex, bey,
             int(bex + math.cos(b2ang) * b2len), int(bey + math.sin(b2ang) * b2len), 3)

# Fragmentos de anillos concéntricos cerca del centro (refuerzan la lectura
# de impacto puntual sin cubrir toda la pantalla de círculos).
for r in (14, 24, 36):
    for a in range(0, 360, 5):
        x = int(IMPACT_X + math.cos(math.radians(a)) * r)
        y = int(IMPACT_Y + math.sin(math.radians(a)) * r)
        if 0 <= x < FS_W and 0 <= y < FS_H and random.random() > 0.35:
            fpx[x, y] = 3

# Rejilla de 4 columnas x 3 filas de celdas 64x64 (coordenadas de CENTRO,
# igual convención que CreateSprite en phantom_intro.c): cubre los 240x160
# completos. Los bordes derecho/inferior solapan un poco entre celdas
# vecinas (64px no divide exacto ni 240 ni 160) -- no genera artefacto porque
# ambas celdas se cortan de la MISMA imagen FS, así que el contenido
# coincide píxel a píxel en la zona compartida.
GRID_COLS = (32, 96, 160, 208)
GRID_ROWS = (32, 96, 144)

sheet = Image.new("P", (64, 64 * len(GRID_COLS) * len(GRID_ROWS)), 0)
sheet.putpalette(palette)

k = 0
for cy in GRID_ROWS:
    for cx in GRID_COLS:
        box = (cx - 32, cy - 32, cx + 32, cy + 32)
        cell = fs.crop(box)   # fuera de límites de FS -> PIL rellena con índice 0 (transparente)
        sheet.paste(cell, (0, k * 64))
        k += 1

sheet.save("graphics/phantom_intro/crack.png")
print("crack.png generado (%d celdas de 64x64)" % k)

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
