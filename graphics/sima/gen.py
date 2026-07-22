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
#
# El 4o campo (over_floor) marca las celdas que tienen pixeles con indice 0
# (transparente) y por eso deben componerse sobre el suelo antes de escribirse
# en tiles.png. Motivo: indice 0 es transparente de verdad en un OBJ (sprite),
# pero estas tres celdas se pintan en un BG (fondo de tilemap), y ahi el
# indice 0 no es "hueco": el hardware le pinta el color de backdrop, que es
# justo la paleta[0] = TRANSPARENT = rojo (255,0,0). El suelo y el muro son
# opacos al 100% (medido), asi que nunca enseñan ese rojo; la escalera si
# tiene huecos (el hueco entre peldaños) y por eso salia con un recuadro rojo
# alrededor. Si mañana se añade otra celda de BG con transparencia, marcarla
# aqui con True en vez de reintroducir el mismo bug.
TILE_CELLS = [
    ("grounds", 12, 12, False),  # crema (212,210,155) al 97%, con motas
    ("walls", 5, 1, False),      # marron oscuro (88,68,34) con vetas verdes
    ("props", 3, 0, True),       # escalera de mano -- tiene huecos transparentes
]

# Indice de TILE_CELLS que hace de fondo al componer las celdas over_floor.
# Es el mismo suelo que ya pinta PlaceCell debajo de cualquier prop en el
# tilemap de la sala, asi que componer contra el es fiel a como se ve en juego.
FLOOR_CELL_INDEX = 0


def generate_tiles():
    """Recorta TILE_CELLS de las hojas ya convertidas en OUT y las pega en
    fila en tiles.png (48x16): 3 celdas de 16x16, una por indice de
    SimaTile. Las hojas fuente comparten paleta (verificado en la Tarea 1),
    asi que el recorte es una simple copia de indices, sin recuantizar.

    Las celdas marcadas over_floor en TILE_CELLS se componen sobre la celda
    de suelo (FLOOR_CELL_INDEX): donde tengan indice 0 (transparente) se usa
    el pixel de suelo en su lugar, para que el indice 0 no llegue nunca a
    tiles.png fuera de la celda de suelo/muro. Ver el comentario en
    TILE_CELLS para el porque (indice 0 = backdrop visible en un BG, no
    transparencia real como en un OBJ)."""
    out = Image.new("P", (16 * len(TILE_CELLS), 16), 0)
    pal = list(TRANSPARENT)
    for rgb in COLORS:
        pal += list(rgb)
    pal += [0, 0, 0] * (16 - 1 - len(COLORS))
    out.putpalette(pal)

    cells = []
    for sheet, cellX, cellY, over_floor in TILE_CELLS:
        sheet_path = os.path.join(OUT, sheet + ".png")
        src = Image.open(sheet_path)
        cell = src.crop((cellX * 16, cellY * 16, cellX * 16 + 16, cellY * 16 + 16))
        cells.append(cell)

    floor = cells[FLOOR_CELL_INDEX]
    floor_px = floor.load()

    for i, (_, _, _, over_floor) in enumerate(TILE_CELLS):
        cell = cells[i]
        if over_floor:
            cell = cell.copy()
            px = cell.load()
            for y in range(16):
                for x in range(16):
                    if px[x, y] == 0:
                        px[x, y] = floor_px[x, y]
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


# Corazones del HUD (Tarea 6): igual que generate_tiles() para las celdas de
# sala, se recortan solo las 2 celdas de hud.png que el HUD usa de verdad
# (corazon lleno y corazon vacio, columnas 0 y 2 de la fila 0 de la hoja de
# 8x9 celdas) en vez de cargar la hoja completa -- esa serian 288 tiles de
# hardware (9 KB) contra los 8 que hacen falta, y BG1 comparte char block
# con los mapblocks de BG0/BG1 (ver el aviso de VRAM en src/sima.c).
HUD_HEART_CELLS = [(0, 0), (2, 0)]  # (col, row) en celdas de 16x16: lleno, vacio


def generate_hud_hearts():
    src_path = os.path.join(OUT, "hud.png")
    src = Image.open(src_path)
    out = Image.new("P", (16 * len(HUD_HEART_CELLS), 16), 0)
    out.putpalette(src.getpalette())
    for i, (col, row) in enumerate(HUD_HEART_CELLS):
        cell = src.crop((col * 16, row * 16, col * 16 + 16, row * 16 + 16))
        out.paste(cell, (i * 16, 0))
    out.save(os.path.join(OUT, "hud_hearts.png"))
    print(f"hud_hearts.png  ({out.width}x{out.height})")


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAULT
    missing = [p for p in ASSETS if not os.path.exists(os.path.join(root, p))]
    if missing:
        sys.exit("ERROR: no encontrados en " + root + ":\n  " + "\n  ".join(missing))
    for rel, name in ASSETS.items():
        convert(os.path.join(root, rel), name)
    generate_tiles()
    generate_player_walk()
    generate_hud_hearts()


if __name__ == "__main__":
    main()
