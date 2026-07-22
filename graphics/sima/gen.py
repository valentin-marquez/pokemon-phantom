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


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else SRC_DEFAULT
    missing = [p for p in ASSETS if not os.path.exists(os.path.join(root, p))]
    if missing:
        sys.exit("ERROR: no encontrados en " + root + ":\n  " + "\n  ".join(missing))
    for rel, name in ASSETS.items():
        convert(os.path.join(root, rel), name)


if __name__ == "__main__":
    main()
