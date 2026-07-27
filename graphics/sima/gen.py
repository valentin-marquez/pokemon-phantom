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
#
# "elf.png" (la hoja grande de la que antes se recortaban a ojo los frames
# del jugador) YA NO esta aqui: el dueño del proyecto separo y nombro los
# frames de verdad (ver PLAYER_ANIM_SOURCES, mas abajo) precisamente para que
# el pipeline deje de adivinar posiciones sobre esa hoja. Si "elf.png" hiciera
# falta para algo mas en el futuro, hay que volver a añadirla aqui.
ASSETS = {
    "SGQ_Dungeon/grounds_and_walls/grounds.png": "grounds",
    "SGQ_Dungeon/grounds_and_walls/walls.png": "walls",
    "SGQ_Dungeon/props/props.png": "props",
    "SGQ_Dungeon/characters/enemies/rat.png": "rat",
    "SGQ_Dungeon/characters/enemies/bat.png": "bat",
    "SGQ_Dungeon/characters/enemies/slime.png": "slime",
    "SGQ_Dungeon/weapons_and_projectiles/weapons_animated.png": "weapons",
    "SGQ_Enemies/bosses/slime_king.png": "slime_king",
    "SGQ_ui/game_ui/hud.png": "hud",
}


def reindex(src_path, expected_size=None):
    """Reindexa una imagen RGBA contra la LUT fija de SIMA (paleta de 4
    colores + transparente, ver cabecera del archivo), devolviendo una
    imagen 'P' ya paletizada -- SIN grabarla a fichero (eso lo hace quien
    llama: convert(), para las hojas de origen completas; generate_player_anim(),
    para los frames nombrados que se concatenan antes de grabar). Aborta ante
    cualquier color fuera de la paleta o alfa parcial: preferible romper el
    build a colar un color aproximado en silencio.

    expected_size, si se da, fuerza (w, h) exactos -- para los ficheros de
    frames nombrados de PLAYER_ANIM_SOURCES, donde un ancho que no sea
    16*n_frames significa que el fichero de origen ya no trae el numero de
    frames que el codigo espera (mejor abortar que colar un frame de mas o
    de menos sin que nadie se entere)."""
    im = Image.open(src_path).convert("RGBA")
    w, h = im.size
    if expected_size is not None and (w, h) != expected_size:
        sys.exit(f"ERROR: {src_path} mide {w}x{h}, se esperaba "
                  f"{expected_size[0]}x{expected_size[1]}")
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

    return out


def convert(src_path, out_name):
    out = reindex(src_path)
    out.save(os.path.join(OUT, out_name + ".png"))
    print(f"{out_name}.png  ({out.width}x{out.height})")


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


# ---------------------------------------------------------------------
# Animaciones del jugador (Tarea de animacion): el dueño del proyecto separo
# y nombro los frames a mano -- ANTES de esta tarea se recortaban a ojo de un
# "elf.png" grande adivinando posiciones (generate_player_walk(), eliminada
# aqui). Es exactamente el motivo por el que los separo: que el pipeline deje
# de adivinar.
#
# Los 5 ficheros de origen (SGQ_Dungeon/characters/main/, ver PLAYER_ANIM_DIR)
# son tiras horizontales de celdas de 16x16 MIRANDO A LA DERECHA, todas de 4
# colores y sin alfa parcial (medido, igual que el resto de hojas de SIMA).
# Las variantes "-left" NO se cargan: son el espejo EXACTO frame a frame de
# las "-right" (verificado comparando cada celda de 16x16 por separado -- no
# la tira entera, que invertiria tambien el ORDEN de los frames -- contra su
# version "-left": 0 pixeles de diferencia en las 4 hojas que traen ambas
# variantes). El juego ya voltea por OAM para mirar a la izquierda
# (sPlayerFacing/hFlip en src/sima_actors.c), asi que cargar tambien la
# izquierda gastaria el doble de VRAM sin aportar nada nuevo.
#
# (fichero, cuantos frames de 16x16 trae, etiqueta para logs/errores). El
# ORDEN de esta lista es el orden en que generate_player_anim() concatena los
# frames en player_anim.png -- src/sima_actors.c deriva sus FRAME_*_BASE de
# este mismo orden (3+4+5+3+6 = 21 frames), no lo cambies sin actualizar alli
# tambien.
PLAYER_ANIM_DIR = "SGQ_Dungeon/characters/main"
PLAYER_ANIM_SOURCES = [
    ("elf-idle-look-right.png", 3, "idle"),
    ("elf-move-right.png", 4, "move"),
    ("elf-take-damage-look-right.png", 5, "damage"),
    ("elf-dead-look-right.png", 3, "dead"),
    ("elf-teleport-disapear.png", 6, "teleport"),
]


def generate_player_anim(root):
    """Reindexa (reindex(), que aborta ante cualquier color o alfa fuera de
    la paleta de SIMA, o ante un ancho que no cuadre con el numero de frames
    esperado) cada fichero de PLAYER_ANIM_SOURCES y los concatena en una
    unica tira horizontal, en orden: 3+4+5+3+6 = 21 celdas de 16x16 ->
    336x16. Igual que generate_player_walk() hacia antes, graphics_file_rules.mk
    convierte player_anim.png con -mwidth 2 -mheight 2 para que cada celda
    quede en 4 tiles de hardware CONTIGUOS (formato OBJ, SPRITE_SIZE(16x16))."""
    cells = []
    for filename, count, label in PLAYER_ANIM_SOURCES:
        src_path = os.path.join(root, PLAYER_ANIM_DIR, filename)
        cell = reindex(src_path, expected_size=(16 * count, 16))
        cells.append(cell)
        print(f"  {label}: {filename} ({count} frames)")

    total_frames = sum(count for _, count, _ in PLAYER_ANIM_SOURCES)
    out = Image.new("P", (16 * total_frames, 16), 0)
    pal = list(TRANSPARENT)
    for rgb in COLORS:
        pal += list(rgb)
    pal += [0, 0, 0] * (16 - 1 - len(COLORS))
    out.putpalette(pal)   # misma paleta que reindex() usa para cada celda -- pegar es una simple copia de indices

    x = 0
    for cell in cells:
        out.paste(cell, (x, 0))
        x += cell.width

    out.save(os.path.join(OUT, "player_anim.png"))
    print(f"player_anim.png  ({out.width}x{out.height})")


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


# Arma del jugador (espada, reemplazo del arco de tajo): CINCO frames de 16x32
# de la ESPADA dibujados a mano por el dueño (weapon_left_0..4.png), en vez de
# los cuatro arcos de media luna de antes (ver git log de esta funcion para esa
# version). Motivo del cambio: el arma principal del prota ES una espada, y con
# la vista de perfil pura (el prota solo mira izq/derecha, ver el comentario
# largo en src/sima_actors.c) una espada en horizontal ya se lee sin la
# ambiguedad direccional que en su dia obligo a pasar al arco.
#
# Los frames vienen MIRANDO A LA IZQUIERDA (el juego voltea por OAM para la
# derecha, hFlip en UpdateAttack -- misma economia de VRAM que player_anim, que
# solo carga "-right"). Son 16x32 -- el doble de alto que el jugador (16x16) --
# porque describen un mandoble descendente: la mitad superior del canvas es la
# espada ALZADA y la inferior el reposo, y el sprite se ancla con su BASE sobre
# la fila del jugador (top-left = sPlayerY-16, ver UpdateAttack). La secuencia
# es: 0 = alzada (windup), 1,2,3 = arco de tajo barriendo hacia abajo (impacto),
# 4 = reposo / follow-through. Todos de 4 colores y sin alfa parcial (medido,
# como el resto de hojas de SIMA), asi que reindexan limpio en la paleta
# compartida -- no necesitan paleta propia.
WEAPON_DIR = "SGQ_Dungeon/weapons_and_projectiles"
WEAPON_FRAMES = 5          # weapon_left_0..4.png
WEAPON_FRAME_W = 16
WEAPON_FRAME_H = 32


def generate_weapon(root):
    """Reindexa (reindex(), que aborta ante cualquier color o alfa fuera de la
    paleta de SIMA, o ante un tamaño que no sea 16x32) los 5 frames de la espada
    y los apila VERTICALMENTE en weapon.png (16x160, frame 0 arriba -> frame 4
    abajo). graphics_file_rules.mk lo convierte con -mwidth 2 -mheight 4 para que
    cada frame de 16x32 quede en 8 tiles de hardware CONTIGUOS (formato OBJ,
    SPRITE_SIZE(16x32)) -- el orden de tiles OBJ 1D dentro de un frame coincide
    con el orden de lectura de gbagfx sobre una tira de 16px de ancho."""
    out = Image.new("P", (WEAPON_FRAME_W, WEAPON_FRAME_H * WEAPON_FRAMES), 0)
    pal = list(TRANSPARENT)
    for rgb in COLORS:
        pal += list(rgb)
    pal += [0, 0, 0] * (16 - 1 - len(COLORS))
    out.putpalette(pal)   # misma paleta que reindex() -- pegar es copiar indices
    for i in range(WEAPON_FRAMES):
        src_path = os.path.join(root, WEAPON_DIR, f"weapon_left_{i}.png")
        cell = reindex(src_path, expected_size=(WEAPON_FRAME_W, WEAPON_FRAME_H))
        out.paste(cell, (0, i * WEAPON_FRAME_H))
    out.save(os.path.join(OUT, "weapon.png"))
    print(f"weapon.png  ({out.width}x{out.height})")


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAULT
    missing = [p for p in ASSETS if not os.path.exists(os.path.join(root, p))]
    missing += [os.path.join(PLAYER_ANIM_DIR, filename)
                for filename, _, _ in PLAYER_ANIM_SOURCES
                if not os.path.exists(os.path.join(root, PLAYER_ANIM_DIR, filename))]
    missing += [os.path.join(WEAPON_DIR, f"weapon_left_{i}.png")
                for i in range(WEAPON_FRAMES)
                if not os.path.exists(os.path.join(root, WEAPON_DIR, f"weapon_left_{i}.png"))]
    if missing:
        sys.exit("ERROR: no encontrados en " + root + ":\n  " + "\n  ".join(missing))
    for rel, name in ASSETS.items():
        convert(os.path.join(root, rel), name)
    # NO llamar a generate_tiles() aqui: graphics/sima/tiles.png lo DUEÑA
    # rooms.py desde que se importo la sala del editor visual -- es el atlas de
    # celdas COMPUESTAS (base+objeto) que indexa sima_rooms_data.h, no las 3
    # celdas suelo/muro/escalera de generate_tiles(). Llamarlo aqui pisaba ese
    # atlas y dejaba la sala apuntando a tiles inexistentes (suelo rojo). La
    # funcion se conserva abajo solo como referencia historica.
    generate_player_anim(root)
    generate_hud_hearts()
    generate_weapon(root)


if __name__ == "__main__":
    main()
