#!/usr/bin/env python3
"""Reindexa los assets de SIMA (packs Super Gameboy Quest de Toadzilla) a PNG
indexados, que es lo unico que gbagfx acepta (tools/gbagfx/convert_png.c:146).

Determinista y sin cuantizacion: los 32 PNG de origen comparten UNA paleta de 4
colores y no tienen un solo pixel semitransparente (medido). Asi que esto es una
tabla de conversion fija, no un proceso con criterio. Si aparece un color que no
esta en la tabla, ABORTA -- es preferible romper el build a colar un color
aproximado en silencio.

Uso:  python3 graphics/sima/gen.py [dir_de_packs]
"""
import os
import sys
from PIL import Image

SRC_DEFAULT = os.path.expanduser("~/Descargas")
OUT = os.path.dirname(os.path.abspath(__file__))

# indice 0 = transparente. El rojo es la convencion que ya usan los assets
# del minijuego anterior (graphics/minigame_spaceship/), se mantiene.
TRANSPARENT = (255, 0, 0)
# 1..4, de mas oscuro a mas claro.
COLORS = [
    (88, 68, 34),      # marron oscuro
    (94, 133, 73),     # verde medio
    (120, 164, 106),   # verde claro
    (212, 210, 155),   # crema
]
LUT = {rgb: i + 1 for i, rgb in enumerate(COLORS)}

# (ruta relativa dentro del dir de packs) -> nombre de salida
ASSETS = {
    "SGQ_Dungeon/grounds_and_walls/grounds.png": "grounds",
    "SGQ_Dungeon/grounds_and_walls/walls.png": "walls",
    "SGQ_Dungeon/props/props.png": "props",
    "SGQ_Dungeon/characters/main/elf.png": "player",
    "SGQ_Dungeon/characters/enemies/rat.png": "rat",
    "SGQ_Dungeon/characters/enemies/bat.png": "bat",
    "SGQ_Dungeon/characters/enemies/slime.png": "slime",
    "SGQ_Dungeon/weapons_and_projectiles/weapons_animated.png": "weapons",
    "SGQ_Enemies/bosses/slime_king.png": "slime_king",
    "SGQ_ui/game_ui/hud.png": "hud",
}


def convert(src_path, out_name):
    im = Image.open(src_path).convert("RGBA")
    w, h = im.size
    if w % 8 or h % 8:
        sys.exit(f"ERROR: {src_path} mide {w}x{h}; ambos lados deben ser multiplo de 8")

    out = Image.new("P", (w, h), 0)
    pal = list(TRANSPARENT)
    for rgb in COLORS:
        pal += list(rgb)
    pal += [0, 0, 0] * (16 - 1 - len(COLORS))   # relleno hasta 16 entradas
    out.putpalette(pal)

    src = im.load()
    dst = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b, a = src[x, y]
            if a == 0:
                dst[x, y] = 0
                continue
            if a != 255:
                sys.exit(f"ERROR: {src_path} tiene alfa parcial en ({x},{y}); "
                         "la GBA no soporta alfa por pixel")
            idx = LUT.get((r, g, b))
            if idx is None:
                sys.exit(f"ERROR: {src_path} usa el color {(r, g, b)} en ({x},{y}), "
                         "que no esta en la paleta de SIMA")
            dst[x, y] = idx

    out.save(os.path.join(OUT, out_name + ".png"))
    print(f"{out_name}.png  ({w}x{h})")


# Celdas de 16x16 que usa de verdad el crawler de SIMA (Tarea 3), recortadas
# de las hojas ya convertidas (mismo indexado, misma paleta unica) en vez de
# cargar grounds/walls enteros: 832+896 = 1728 tiles de hardware no caben en
# el campo de 10 bits (1024) de una entrada de tilemap. Coordenadas en celdas
# de 16x16, elegidas por uniformidad/contraste -- no tocar sin remedir.
# indice 0 = suelo, 1 = muro, 2 = escalera (ver include/sima_rooms.h).
TILE_CELLS = [
    ("grounds", 12, 12),  # crema (212,210,155) al 97%, con motas
    ("walls", 5, 1),      # marron oscuro (88,68,34) con vetas verdes
    ("props", 3, 0),      # escalera de mano
]


def generate_tiles():
    """Recorta TILE_CELLS de las hojas ya convertidas en OUT y las pega en
    fila en tiles.png (48x16): 3 celdas de 16x16, una por indice de
    SimaTile. Las hojas fuente comparten paleta (verificado en la Tarea 1),
    asi que el recorte es una simple copia de indices, sin recuantizar."""
    out = Image.new("P", (16 * len(TILE_CELLS), 16), 0)
    pal = list(TRANSPARENT)
    for rgb in COLORS:
        pal += list(rgb)
    pal += [0, 0, 0] * (16 - 1 - len(COLORS))
    out.putpalette(pal)

    for i, (sheet, cellX, cellY) in enumerate(TILE_CELLS):
        sheet_path = os.path.join(OUT, sheet + ".png")
        src = Image.open(sheet_path)
        cell = src.crop((cellX * 16, cellY * 16, cellX * 16 + 16, cellY * 16 + 16))
        out.paste(cell, (i * 16, 0))

    out.save(os.path.join(OUT, "tiles.png"))
    print(f"tiles.png  ({out.width}x{out.height})")


# Celdas de caminata que el jugador de la Tarea 4 usa de player.png (filas
# 0/1/2: abajo, arriba, perfil -- ver el mapa de frames del brief de la
# Tarea 4). Izquierda reutiliza las celdas de perfil volteadas por OAM
# (oam.hFlip en src/sima_actors.c), no hace falta arte propio. (fila, col)
# en celdas de 16x16 de player.png.
PLAYER_WALK_CELLS = [
    (0, 0), (0, 1), (0, 2),   # abajo:  quieto, paso A, paso B
    (1, 0), (1, 1), (1, 2),   # arriba: quieto, paso A, paso B
    (2, 0), (2, 1), (2, 2), (2, 3),  # perfil: quieto + 3 de ciclo de paso
]


def generate_player_walk():
    """Recorta de player.png (ya reindexado) las 10 celdas de caminata que
    usa SimaActors (Tarea 4) y las empaqueta en una tira horizontal de
    160x16. A diferencia de generate_tiles() (que arma un BG, pintado por
    PlaceCell con barrido raster de la hoja completa), esto es para un OBJ:
    graphics_file_rules.mk convierte player_walk.png con -mwidth 2 -mheight 2
    para que cada celda de 16x16 quede como 4 tiles de hardware CONTIGUOS,
    que es el formato que necesita un sprite SPRITE_SIZE(16x16). Las filas
    3-8 de player.png (ataque/muerte) quedan fuera: no son de esta tarea."""
    src_path = os.path.join(OUT, "player.png")
    src = Image.open(src_path)
    out = Image.new("P", (16 * len(PLAYER_WALK_CELLS), 16), 0)
    out.putpalette(src.getpalette())
    for i, (row, col) in enumerate(PLAYER_WALK_CELLS):
        cell = src.crop((col * 16, row * 16, col * 16 + 16, row * 16 + 16))
        out.paste(cell, (i * 16, 0))
    out.save(os.path.join(OUT, "player_walk.png"))
    print(f"player_walk.png  ({out.width}x{out.height})")


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAULT
    missing = [p for p in ASSETS if not os.path.exists(os.path.join(root, p))]
    if missing:
        sys.exit("ERROR: no encontrados en " + root + ":\n  " + "\n  ".join(missing))
    for rel, name in ASSETS.items():
        convert(os.path.join(root, rel), name)
    generate_tiles()
    generate_player_walk()


if __name__ == "__main__":
    main()
