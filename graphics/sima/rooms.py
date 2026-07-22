#!/usr/bin/env python3
"""Importa las salas de SIMA del editor visual a datos consumibles por
src/sima_rooms.c.

Las salas de SIMA ya NO se escriben a mano como arte ASCII en el .c (ver
git log de src/sima_rooms.c): se dibujan en tools/sima-editor/index.html y
se exportan a tools/sima-editor/salas.json. Este script lee ese JSON y
genera dos artefactos:

  1. graphics/sima/tiles.png -- el atlas de celdas de 16x16 realmente usadas,
     una por cada combinacion DISTINTA de (celda de fondo, celda de objeto)
     que aparece en algun piso con contenido. Hace falta componerlas de
     antemano porque una capa de fondo (BG) de la GBA no apila dos tiles: si
     el piso quiere un barril sobre tablones, hace falta un UNICO tile que ya
     tenga el barril pintado sobre los tablones.

  2. src/sima_rooms_data.h -- las tablas de C (tile grafico compuesto, solido,
     spawn, escalera, enemigos) que src/sima_rooms.c consume. GENERADO, no
     editar a mano: se pisa entero en cada ejecucion.

Ademas re-escribe el numero de SIMA_FLOOR_COUNT en include/sima_rooms.h para
que coincida con el numero de pisos CON CONTENIDO del JSON (spawn != null).
Los pisos en blanco (todavia en stand-by mientras se disenan) NO entran en
la ROM: caminar sobre una sala en blanco seria un bug, no una funcionalidad.

Determinista: misma entrada (mismo salas.json + mismos PNG de origen) ->
mismos bytes de salida, siempre. No hay cuantizacion ni aleatoriedad; solo
copia de indices de paleta.

Uso (repetible, un solo comando -- tambien vale para cuando se pinten los
pisos 2 y 3 con el editor):

    python3 graphics/sima/rooms.py

Requiere que graphics/sima/gen.py se haya corrido antes al menos una vez
(grounds.png, walls.png, props.png deben existir ya re-indexados con la
paleta fija de SIMA: indice 0 = TRANSPARENT = (255,0,0), 1..4 = tonos).
"""
import json
import os
import re
import sys

from PIL import Image

OUT = os.path.dirname(os.path.abspath(__file__))              # graphics/sima
ROOT = os.path.dirname(os.path.dirname(OUT))                  # raiz del repo
SALAS_JSON = os.path.join(ROOT, "tools", "sima-editor", "salas.json")
DATA_HEADER = os.path.join(ROOT, "src", "sima_rooms_data.h")
PUBLIC_HEADER = os.path.join(ROOT, "include", "sima_rooms.h")

ROOM_W = 15
ROOM_H = 10
CELL_PX = 16

# Misma paleta fija que graphics/sima/gen.py: indice 0 = TRANSPARENT
# (255,0,0), 1..4 = tonos, relleno con negro hasta 16 entradas. No se lee de
# los PNG de origen (PIL podria devolver una paleta truncada o reordenada);
# se fija aqui explicitamente para que la salida no dependa de un detalle de
# implementacion de Pillow.
TRANSPARENT_IDX = 0
_COLORS = [
    (88, 68, 34),
    (94, 133, 73),
    (120, 164, 106),
    (212, 210, 155),
]
_PALETTE = [255, 0, 0]
for rgb in _COLORS:
    _PALETTE += list(rgb)
_PALETTE += [0, 0, 0] * (16 - 1 - len(_COLORS))


def load_sheets(cells_by_id):
    """Abre cada hoja PNG (grounds/walls/props) referenciada por alguna
    celda, una sola vez."""
    names = {c["sheet"] for c in cells_by_id.values()}
    sheets = {}
    for name in names:
        path = os.path.join(OUT, name + ".png")
        if not os.path.exists(path):
            sys.exit(f"ERROR: falta {path}. Corre antes: python3 graphics/sima/gen.py")
        sheets[name] = Image.open(path)
    return sheets


def crop_cell(sheets, cell):
    sheet = sheets[cell["sheet"]]
    x, y = cell["x"] * CELL_PX, cell["y"] * CELL_PX
    return sheet.crop((x, y, x + CELL_PX, y + CELL_PX))


def compose(base_img, over_img):
    """Pega over_img encima de base_img respetando la transparencia real de
    estos PNG, que NO es alfa: es el indice de paleta 0 (color (255,0,0),
    ver la constante TRANSPARENT_IDX arriba). Un pixel de over_img con
    indice 0 deja ver el pixel de base_img; cualquier otro indice lo tapa.
    Tratar el indice 0 como un color opaco mas produciria un recuadro rojo
    alrededor del objeto -- ya paso dos veces en este proyecto (ver
    graphics/sima/gen.py, TILE_CELLS)."""
    if over_img is None:
        return base_img.copy()
    out = base_img.copy()
    src = over_img.load()
    dst = out.load()
    for y in range(CELL_PX):
        for x in range(CELL_PX):
            idx = src[x, y]
            if idx != TRANSPARENT_IDX:
                dst[x, y] = idx
    return out


def load_salas():
    with open(SALAS_JSON, encoding="utf-8") as f:
        data = json.load(f)
    if data.get("format") != "sima-rooms/2":
        sys.exit(f"ERROR: formato inesperado {data.get('format')!r} en {SALAS_JSON} "
                  "(se esperaba sima-rooms/2)")
    grid = data.get("grid", {})
    if grid.get("w") != ROOM_W or grid.get("h") != ROOM_H or grid.get("cell") != CELL_PX:
        sys.exit(f"ERROR: rejilla inesperada {grid} en {SALAS_JSON} "
                  f"(se esperaba {ROOM_W}x{ROOM_H} celdas de {CELL_PX}px)")
    return data


def build_rooms(data, sheets):
    """Recorre los pisos CON CONTENIDO (spawn != null) en el orden del JSON
    y compone, una sola vez por combinacion distinta, cada celda (base,
    over) que aparece de verdad. Devuelve las tablas ya listas para volcar a
    C mas la lista de imagenes compuestas para el atlas."""
    cells_by_id = {c["id"]: c for c in data["cells"]}

    tile_index = {}    # (base_id, over_id_o_None) -> indice de tile compuesto
    tile_images = []   # imagenes PIL compuestas, en el mismo orden que los indices

    def get_tile_index(base_id, over_id):
        key = (base_id, over_id)
        idx = tile_index.get(key)
        if idx is not None:
            return idx
        base_img = crop_cell(sheets, cells_by_id[base_id])
        over_img = crop_cell(sheets, cells_by_id[over_id]) if over_id is not None else None
        composed = compose(base_img, over_img)
        idx = len(tile_images)
        tile_index[key] = idx
        tile_images.append(composed)
        return idx

    floors = []
    for floor_i, fl in enumerate(data["floors"]):
        if fl.get("spawn") is None:
            continue  # piso en blanco / stand-by: no entra en la ROM

        base = fl["base"]
        over = fl["over"]
        solid = fl["solid"]
        n = ROOM_W * ROOM_H
        if len(base) != n or len(over) != n or len(solid) != n:
            sys.exit(f"ERROR: piso {floor_i} tiene tablas de longitud distinta de {n}")

        stairs = fl.get("stairs")
        if stairs is None:
            sys.exit(f"ERROR: piso {floor_i} tiene spawn pero no escalera -- "
                      "SimaRoom_IsStairs nunca seria verdad en el")

        gfx_row = [get_tile_index(base[i], over[i]) for i in range(n)]
        solid_row = [bool(s) for s in solid]
        spawn = (fl["spawn"]["x"], fl["spawn"]["y"])
        stairs_xy = (stairs["x"], stairs["y"])
        enemies = [(e["x"], e["y"]) for e in fl.get("enemies", [])]

        floors.append({
            "gfx": gfx_row,
            "solid": solid_row,
            "spawn": spawn,
            "stairs": stairs_xy,
            "enemies": enemies,
        })

    if not floors:
        sys.exit(f"ERROR: ningun piso de {SALAS_JSON} tiene spawn -- nada que importar")

    n_tiles = len(tile_images)
    hw_tiles = n_tiles * 4  # cada celda de 16x16 = 2x2 tiles de hardware de 8x8
    if hw_tiles > 1024:
        sys.exit(f"ERROR: {n_tiles} celdas compuestas -> {hw_tiles} tiles de hardware, "
                 "supera el limite de 1024 (campo de 10 bits de una entrada de tilemap)")

    return floors, tile_images, n_tiles


def write_atlas(tile_images):
    atlas = Image.new("P", (CELL_PX * len(tile_images), CELL_PX), 0)
    atlas.putpalette(_PALETTE)
    for i, img in enumerate(tile_images):
        atlas.paste(img, (i * CELL_PX, 0))
    path = os.path.join(OUT, "tiles.png")
    atlas.save(path)
    print(f"tiles.png  ({atlas.width}x{atlas.height}, {len(tile_images)} celdas compuestas)")


def _c_grid(values, per_row=ROOM_W, fmt="{}"):
    """Formatea una tabla de ROOM_W*ROOM_H valores como ROOM_H filas de
    ROOM_W, para que el .h generado se pueda hojear como una rejilla (igual
    que el arte ASCII que sustituye), aunque sea codigo generado."""
    lines = []
    for row in range(0, len(values), per_row):
        chunk = values[row:row + per_row]
        lines.append("        " + ", ".join(fmt.format(v) for v in chunk) + ",")
    return "\n".join(lines)


def write_header(floors, n_tiles):
    floor_count = len(floors)
    max_enemies = max((len(fl["enemies"]) for fl in floors), default=0)

    lines = []
    lines.append("// GENERADO por graphics/sima/rooms.py -- NO EDITAR A MANO.")
    lines.append("// Fuente: tools/sima-editor/salas.json (formato sima-rooms/2), pintado")
    lines.append("// con el editor visual tools/sima-editor/index.html.")
    lines.append("// Para regenerar (tambien tras anadir contenido a los pisos 2/3):")
    lines.append("//     python3 graphics/sima/rooms.py")
    lines.append("")
    lines.append("#ifndef GUARD_SIMA_ROOMS_DATA_H")
    lines.append("#define GUARD_SIMA_ROOMS_DATA_H")
    lines.append("")
    lines.append(f"// Numero de celdas compuestas distintas en graphics/sima/tiles.png (una")
    lines.append(f"// fila de SIMA_ROOMS_TILE_COUNT celdas de 16x16 = SIMA_ROOMS_TILE_COUNT*2")
    lines.append(f"// tiles de hardware de ancho). src/sima.c la usa como sheetTilesWide.")
    lines.append(f"#define SIMA_ROOMS_TILE_COUNT {n_tiles}")
    lines.append("")
    lines.append(f"#define SIMA_ROOM_MAX_ENEMIES {max_enemies}")
    lines.append("")

    # Tile grafico (indice en graphics/sima/tiles.png) por casilla.
    lines.append("// Indice de la celda compuesta (graphics/sima/tiles.png) por casilla,")
    lines.append("// en orden raster (fila a fila). SimaRoom_GetTileGfx lo expone a src/sima.c.")
    lines.append(f"static const u16 sRoomTileGfx[SIMA_FLOOR_COUNT][SIMA_ROOM_W * SIMA_ROOM_H] = {{")
    for fl in floors:
        lines.append("    {")
        lines.append(_c_grid(fl["gfx"]))
        lines.append("    },")
    lines.append("};")
    lines.append("")

    # Solido por casilla.
    lines.append("// TRUE si la casilla bloquea el paso, en orden raster.")
    lines.append(f"static const bool8 sRoomSolid[SIMA_FLOOR_COUNT][SIMA_ROOM_W * SIMA_ROOM_H] = {{")
    for fl in floors:
        lines.append("    {")
        lines.append(_c_grid(["TRUE" if s else "FALSE" for s in fl["solid"]], fmt="{}"))
        lines.append("    },")
    lines.append("};")
    lines.append("")

    # Spawn.
    lines.append("// Casilla de spawn del jugador, {x, y}, por piso.")
    lines.append("static const s8 sRoomSpawn[SIMA_FLOOR_COUNT][2] = {")
    for fl in floors:
        lines.append(f"    {{{fl['spawn'][0]}, {fl['spawn'][1]}}},")
    lines.append("};")
    lines.append("")

    # Escalera.
    lines.append("// Casilla de la escalera, {x, y}, por piso.")
    lines.append("static const s8 sRoomStairs[SIMA_FLOOR_COUNT][2] = {")
    for fl in floors:
        lines.append(f"    {{{fl['stairs'][0]}, {fl['stairs'][1]}}},")
    lines.append("};")
    lines.append("")

    # Enemigos.
    lines.append("// Numero de enemigos por piso (<= SIMA_ROOM_MAX_ENEMIES).")
    lines.append("static const u8 sRoomEnemyCount[SIMA_FLOOR_COUNT] = {")
    lines.append("    " + ", ".join(str(len(fl["enemies"])) for fl in floors) + ",")
    lines.append("};")
    lines.append("")
    lines.append("// Casillas de spawn de enemigo, {x, y}, por piso. Las entradas sobrantes")
    lines.append("// (por encima de sRoomEnemyCount[piso]) quedan a {0, 0} y no se leen.")
    lines.append("static const s8 sRoomEnemies[SIMA_FLOOR_COUNT][SIMA_ROOM_MAX_ENEMIES][2] = {")
    for fl in floors:
        lines.append("    {")
        enemies = fl["enemies"] + [(0, 0)] * (max_enemies - len(fl["enemies"]))
        for ex, ey in enemies:
            lines.append(f"        {{{ex}, {ey}}},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("#endif // GUARD_SIMA_ROOMS_DATA_H")
    lines.append("")

    with open(DATA_HEADER, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"{DATA_HEADER}  ({floor_count} piso(s) con contenido, {n_tiles} tiles, "
          f"{max_enemies} enemigo(s) maximo)")


def patch_floor_count(floor_count):
    """Reescribe el numero de SIMA_FLOOR_COUNT en el header PUBLICO
    (include/sima_rooms.h) para que coincida con los pisos con contenido de
    verdad. Solo toca esa linea (#define SIMA_FLOOR_COUNT N); el resto del
    header (comentarios, firmas publicas) no se toca -- es el unico valor
    que depende de salas.json."""
    with open(PUBLIC_HEADER, encoding="utf-8") as f:
        text = f.read()

    pattern = re.compile(r"^#define SIMA_FLOOR_COUNT \d+$", re.MULTILINE)
    if not pattern.search(text):
        sys.exit(f"ERROR: no se encontro '#define SIMA_FLOOR_COUNT <N>' en {PUBLIC_HEADER}")

    new_text = pattern.sub(f"#define SIMA_FLOOR_COUNT {floor_count}", text, count=1)
    if new_text != text:
        with open(PUBLIC_HEADER, "w", encoding="utf-8") as f:
            f.write(new_text)
        print(f"{PUBLIC_HEADER}  (SIMA_FLOOR_COUNT -> {floor_count})")
    else:
        print(f"{PUBLIC_HEADER}  (SIMA_FLOOR_COUNT ya era {floor_count}, sin cambios)")


def main():
    data = load_salas()
    cells_by_id = {c["id"]: c for c in data["cells"]}
    sheets = load_sheets(cells_by_id)

    floors, tile_images, n_tiles = build_rooms(data, sheets)

    write_atlas(tile_images)
    write_header(floors, n_tiles)
    patch_floor_count(len(floors))


if __name__ == "__main__":
    main()
