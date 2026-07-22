# SIMA — el crawler del prólogo · Plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sustituir el minijuego shmup del prólogo por **SIMA**, un dungeon crawler monocromo de Game Boy que termina en un jefe invencible, un marcador con el récord de Carlos y la entrada del nombre del jugador.

**Architecture:** Un modo nuevo (`CB2_InitSima`) al que `minigame_pre.c` entrega el control tras la narración del camarote. Cada piso es **una pantalla fija de 15×10 tiles de 16px** dibujada en un tilemap estático, sin scroll ni cámara. La lógica pura (colisión, progresión, daño) vive separada del renderizado en `sima_rooms.c` para que el harness in-ROM pueda ejercitarla sin inyección de inputs.

**Tech Stack:** C (gnu89 vía `arm-none-eabi-gcc`), pokeemerald vanilla, `gbagfx` para conversión de gráficos, Python 3 + Pillow para el reindexado determinista, harness in-ROM (`PHANTOM_TEST=1` + `mgba-rom-test`).

## Global Constraints

- **Spec de referencia:** `docs/superpowers/specs/2026-07-21-prologo-consola-design.md`. Los límites de hardware están verificados en `docs/design/limites-graficos-gba.md`.
- **Sin framework de tests.** La verificación es: build limpio + `./test/smoke.sh` (`SMOKE: OK`) + revisión visual en mGBA. Los tests unitarios se añaden a `src/phantom_test.c` con `PHANTOM_ASSERT(cond, "nombre")`.
- **El harness NO inyecta inputs** (documentado en `src/phantom_test.c`). Solo se puede testear lógica invocable directamente. Todo lo que dependa de pulsaciones se verifica visualmente.
- **Commits sin trailers de atribución.** Nada de `Co-Authored-By` ni `Claude-Session`.
- **No hacer `git push`.** El remoto real es `git.nozz.skin`; `origin` apunta a GitHub y está vetado.
- **Paleta única, verificada sobre los 32 PNG de origen:** índice 0 transparente + `(88,68,34)`, `(94,133,73)`, `(120,164,106)`, `(212,210,155)`. Cero píxeles semitransparentes en todo el conjunto.
- **Tamaños de sprite legales en GBA:** 8×8, 16×16, 32×32, 64×64, 16×8, 32×8, 32×16, 64×32, 8×16, 8×32, 16×32, 32×64. **48×48 no existe** — el Slime King va en un 64×64 con relleno.
- **`MAX_SPRITES = 64`** (límite de pokeemerald, no del hardware).
- **Los assets de origen están en** `/home/valentin/Descargas/SGQ_Dungeon`, `SGQ_Enemies`, `SGQ_ui`. Son de Toadzilla y **exigen atribución** en `CREDITS.md`.

---

## Estructura de ficheros

| Fichero | Responsabilidad |
|---|---|
| `graphics/sima/gen.py` | Reindexa los PNG RGBA de origen a PNG indexados. Determinista, falla ruidosamente ante un color inesperado. |
| `graphics/sima/*.png` | Assets indexados generados (se commitean, como en `graphics/phantom_intro/`). |
| `include/sima.h` | API pública del modo: `CB2_InitSima`, y las funciones puras que el harness ejercita. |
| `src/sima.c` | Modo: callbacks, BGs, paletas, task principal, flujo de pisos. |
| `include/sima_rooms.h` / `src/sima_rooms.c` | Datos de las salas + consultas **puras** (colisión, escalera, spawns). Es la parte testeable. |
| `src/sima_actors.c` | Jugador, enemigos y jefe: movimiento, daño, ataque. |
| `src/sima_score.c` | Marcador y entrada de nombre. |

---

## Task 1: Script de reindexado y assets convertidos

Nada compila hasta que esto exista: `gbagfx` exige PNG **indexado** (`tools/gbagfx/convert_png.c:146`) y los 32 ficheros de origen son RGBA.

**Files:**
- Create: `graphics/sima/gen.py`
- Create: `graphics/sima/*.png` (salida generada, se commitea)

**Interfaces:**
- Produces: PNG indexados de 4bpp con la paleta fija, listos para `INCBIN_U32("graphics/sima/<n>.4bpp")`.

- [ ] **Step 1: Escribir el script**

Crear `graphics/sima/gen.py`:

```python
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
```

- [ ] **Step 2: Ejecutarlo y comprobar que falla ruidosamente ante rutas malas**

Run: `python3 graphics/sima/gen.py /ruta/que/no/existe`
Expected: `ERROR: no encontrados en /ruta/que/no/existe:` seguido de la lista. Exit != 0.

- [ ] **Step 3: Ejecutarlo de verdad**

Run: `python3 graphics/sima/gen.py`
Expected: una línea por asset con su tamaño, sin errores.

- [ ] **Step 4: Verificar que la salida es indexada y de 4 colores**

Run:
```bash
python3 -c "
from PIL import Image; import glob
for p in sorted(glob.glob('graphics/sima/*.png')):
    im = Image.open(p)
    used = {i for i in im.getdata()}
    print(f'{p:34s} modo={im.mode} indices={sorted(used)}')
"
```
Expected: `modo=P` en todos, e índices dentro de `[0, 1, 2, 3, 4]`.

- [ ] **Step 5: Commit**

```bash
git add graphics/sima/gen.py graphics/sima/*.png
git commit -m "feat(sima): script de reindexado de assets + PNG convertidos

gbagfx solo acepta PNG indexado y los 32 ficheros de origen son RGBA. El
script es una tabla de conversion fija de 4 entradas, no una cuantizacion:
los assets comparten una unica paleta y no tienen alfa parcial. Aborta ante
cualquier color fuera de tabla en vez de aproximarlo en silencio."
```

---

## Task 2: Esqueleto del modo SIMA

Un modo que arranca, pinta una sala vacía y devuelve el control. Sin jugador todavía.

**Files:**
- Create: `include/sima.h`, `src/sima.c`
- Modify: `src/minigame_pre.c` (`Task_TransitionToMainGame`)

**Interfaces:**
- Consumes: los PNG de Task 1.
- Produces: `void CB2_InitSima(void);`

- [ ] **Step 1: Crear el header**

`include/sima.h`:
```c
#ifndef GUARD_SIMA_H
#define GUARD_SIMA_H

// SIMA: el dungeon crawler ficticio que el protagonista juega en la consola
// durante el prologo. Ver docs/superpowers/specs/2026-07-21-prologo-consola-design.md
void CB2_InitSima(void);

#endif // GUARD_SIMA_H
```

- [ ] **Step 2: Crear `src/sima.c` con el modo mínimo**

Debe: resetear GPU/tasks/sprites, montar **BG0 (sala)** y **BG1 (HUD/texto)** en modo 0, cargar `grounds`/`walls` en el bloque de tiles y la paleta única en `BG_PLTT_ID(0)`, pintar un tilemap de 15×10 celdas de 16px a partir de una sala de prueba rellena de suelo, y entrar con `BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK)`.

Seguir el patrón de `src/minigame_pre.c` para el callback principal, **incluyendo `RunTextPrinters()`** (hará falta en Task 9 y olvidarlo es el bug que ya costó una sesión):
```c
static void CB2_SimaMain(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    RunTextPrinters();
    UpdatePaletteFade();
}
```

- [ ] **Step 3: Enganchar desde el prólogo**

En `src/minigame_pre.c`, sustituir el destino:
```c
static void Task_TransitionToMainGame(u8 taskId)
{
    SetMainCallback2(CB2_InitSima);   // era CB2_InitMinigameSpaceship
    DestroyTask(taskId);
}
```
y cambiar el `#include "minigame_spaceship.h"` por `#include "sima.h"`.

- [ ] **Step 4: Build**

Run: `make modern -j$(nproc)`
Expected: compila y linka limpio.

- [ ] **Step 5: Smoke**

Run: `./test/smoke.sh`
Expected: `SMOKE: OK`

- [ ] **Step 6: Verificación visual**

Run: `flatpak run io.mgba.mGBA pokeemerald_modern.gba`
Comprobar: START → NUEVA PARTIDA → vidrio → camarote → tras la narración aparece **una sala de mazmorra en verde de dos tonos**, sin scroll y ocupando la pantalla entera.

- [ ] **Step 7: Commit**

```bash
git add include/sima.h src/sima.c src/minigame_pre.c
git commit -m "feat(sima): esqueleto del modo con una sala fija de 15x10

240/16 = 15 y 160/16 = 10, asi que una pantalla es exactamente una rejilla
de salas de 16px: ni scroll ni camara. El prologo entrega aqui el control
tras la narracion del camarote."
```

---

## Task 3: Datos de sala y colisión (la parte testeable)

Lógica pura, sin renderizado, para que el harness pueda ejercitarla sin inputs.

**Files:**
- Create: `include/sima_rooms.h`, `src/sima_rooms.c`
- Modify: `src/phantom_test.c`, `include/phantom_test.h` (si hace falta exponer algo)

- [ ] **Step 0: Recortar SOLO las celdas que SIMA usa (bloqueante)**

La Tarea 2 destapó que `grounds` (832 tiles) + `walls` (896) suman **1728 tiles de
hardware**, y el campo de índice de una entrada de tilemap son **10 bits: 1024
máximo**. Cargar las hojas enteras es imposible *y* innecesario — el crawler usa
tres celdas.

Extender `graphics/sima/gen.py` con una función que recorte celdas de 16×16 por
coordenada y las pegue en un `tiles.png` de 3 celdas (48×16). Celdas elegidas por
medición de uniformidad y contraste:

| Índice de celda | Origen | Celda | Color dominante |
|---|---|---|---|
| 0 = suelo | `grounds.png` | (12, 12) | crema `(212,210,155)` al 97 %, con motas |
| 1 = muro | `walls.png` | (5, 1) | marrón oscuro `(88,68,34)` con vetas verdes |
| 2 = escalera | `props.png` | (3, 0) | escalera de mano |

Resultado: **12 tiles de hardware** en vez de 1728. `src/sima.c` pasa a cargar
`tiles.4bpp` en lugar de `grounds`/`walls` enteros, y los `INCBIN` de esas dos
hojas se retiran de `sima.c` (los PNG se quedan en el repo: los usa `gen.py`).

**Interfaces:**
- Produces:
  - `#define SIMA_ROOM_W 15` / `#define SIMA_ROOM_H 10`
  - `#define SIMA_FLOOR_COUNT 3`
  - `enum SimaTile { SIMA_TILE_FLOOR, SIMA_TILE_WALL, SIMA_TILE_STAIRS };`
  - `u8 SimaRoom_GetTile(u8 floor, s8 x, s8 y);` — devuelve `SIMA_TILE_WALL` fuera de rango
  - `bool8 SimaRoom_IsSolid(u8 floor, s8 x, s8 y);`
  - `bool8 SimaRoom_IsStairs(u8 floor, s8 x, s8 y);`
  - `void SimaRoom_GetSpawn(u8 floor, s8 *outX, s8 *outY);`

- [ ] **Step 1: Escribir el test que falla**

Añadir a `src/phantom_test.c` (antes de `PhantomTest_Run`):
```c
// Test 7 (SIMA): las salas son validas por construccion -- cerradas por muros,
// con exactamente una escalera y un spawn transitable. Un fallo aqui es una
// sala mal dibujada, y sin esta comprobacion solo se detectaria jugando.
static void Test_SimaRoomsValid(void)
{
    u8 floor;
    bool8 allEnclosed = TRUE;
    bool8 allHaveOneStairs = TRUE;
    bool8 allSpawnsWalkable = TRUE;

    for (floor = 0; floor < SIMA_FLOOR_COUNT; floor++)
    {
        s8 x, y;
        u32 stairs = 0;
        for (x = 0; x < SIMA_ROOM_W; x++)
        {
            if (!SimaRoom_IsSolid(floor, x, 0) || !SimaRoom_IsSolid(floor, x, SIMA_ROOM_H - 1))
                allEnclosed = FALSE;
        }
        for (y = 0; y < SIMA_ROOM_H; y++)
        {
            if (!SimaRoom_IsSolid(floor, 0, y) || !SimaRoom_IsSolid(floor, SIMA_ROOM_W - 1, y))
                allEnclosed = FALSE;
        }
        for (y = 0; y < SIMA_ROOM_H; y++)
            for (x = 0; x < SIMA_ROOM_W; x++)
                if (SimaRoom_IsStairs(floor, x, y))
                    stairs++;
        if (stairs != 1)
            allHaveOneStairs = FALSE;
        {
            s8 sx, sy;
            SimaRoom_GetSpawn(floor, &sx, &sy);
            if (SimaRoom_IsSolid(floor, sx, sy))
                allSpawnsWalkable = FALSE;
        }
    }

    PHANTOM_ASSERT(allEnclosed, "sima-rooms-enclosed");
    PHANTOM_ASSERT(allHaveOneStairs, "sima-rooms-one-stairs");
    PHANTOM_ASSERT(allSpawnsWalkable, "sima-spawns-walkable");
    // Fuera de rango debe ser solido, o el jugador se sale de la sala.
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, -1, 5), "sima-oob-solid-left");
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, SIMA_ROOM_W, 5), "sima-oob-solid-right");
}
```
y llamarlo desde `PhantomTest_Run()` antes de `PHANTOM_CHECKPOINT("suite-end")`. Añadir `#include "sima_rooms.h"` arriba.

- [ ] **Step 2: Ejecutar el test y ver que NO compila (RED)**

Run: `./test/smoke.sh`
Expected: falla la compilación con `sima_rooms.h: No such file or directory`. Es el RED correcto: el test existe antes que el código.

- [ ] **Step 3: Escribir `include/sima_rooms.h` y `src/sima_rooms.c`**

Las salas se declaran como arrays de cadenas de 15 caracteres × 10 filas, que se leen a ojo en el propio código:
```c
// '#' muro, '.' suelo, '>' escalera, '@' spawn del jugador.
static const char *const sFloor0[SIMA_ROOM_H] = {
    "###############",
    "#@...........*#",
    "#.####.#####..#",
    "#.#.........#.#",
    "#.#.#######.#.#",
    "#.#.......#.#.#",
    "#.#######.#.#.#",
    "#.........#..>#",
    "#.............#",
    "###############",
};
```
(`*` reservado para spawns de enemigos en Task 6; de momento tratarlo como suelo.)

`SimaRoom_GetTile` devuelve `SIMA_TILE_WALL` si `x`/`y` caen fuera de `[0, SIMA_ROOM_W)` × `[0, SIMA_ROOM_H)`. `SimaRoom_GetSpawn` busca el `'@'`.

- [ ] **Step 4: Ejecutar el test (GREEN)**

Run: `./test/smoke.sh`
Expected: `SMOKE: OK`, con `:P PASS sima-rooms-enclosed`, `sima-rooms-one-stairs`, `sima-spawns-walkable`, `sima-oob-solid-left`, `sima-oob-solid-right`.

- [ ] **Step 5: Pintar las salas reales desde `sima.c`**

Sustituir la sala de prueba de Task 2 por `SimaRoom_GetTile(floor, x, y)`.

- [ ] **Step 6: Build + verificación visual**

Run: `make modern -j$(nproc)` y abrir en mGBA.
Comprobar: la sala del piso 0 se ve con sus muros y su escalera.

- [ ] **Step 7: Commit**

```bash
git add include/sima_rooms.h src/sima_rooms.c src/sima.c src/phantom_test.c
git commit -m "feat(sima): datos de sala y colision, con validacion en el harness

Las salas se declaran como arte ASCII de 15x10 legible en el propio codigo.
La logica va separada del renderizado porque el harness in-ROM no inyecta
inputs: asi se puede verificar sin jugar que las tres salas estan cerradas
por muros, tienen exactamente una escalera y un spawn transitable."
```

---

## Task 4: Jugador — movimiento y colisión

**Files:**
- Create: `src/sima_actors.c`
- Modify: `src/sima.c`, `include/sima.h`

**Interfaces:**
- Consumes: `SimaRoom_IsSolid`, `SimaRoom_GetSpawn`.
- Produces: `void SimaActors_InitPlayer(u8 floor);`, `void SimaActors_UpdatePlayer(void);`, `void SimaActors_GetPlayerTile(s8 *x, s8 *y);`

- [ ] **Step 1: Crear el sprite del jugador**

`player.png` es de 112×144 en celdas de 16×16. Usar `SPRITE_SIZE(16x16)` y una hoja con los frames de caminar en las 4 direcciones.

- [ ] **Step 2: Movimiento libre en píxeles con colisión por casilla**

El jugador se mueve en píxeles (no por casillas: el spec pide tiempo real). Antes de aplicar el desplazamiento, comprobar la casilla de destino con `SimaRoom_IsSolid(floor, nx / 16, ny / 16)` para las cuatro esquinas de su caja de colisión.

- [ ] **Step 3: Build**

Run: `make modern -j$(nproc)`
Expected: limpio.

- [ ] **Step 4: Smoke**

Run: `./test/smoke.sh`
Expected: `SMOKE: OK` (los tests de Task 3 siguen pasando).

- [ ] **Step 5: Verificación visual**

Comprobar en mGBA: el elfo aparece en el `'@'`, se mueve en 4 direcciones, **no atraviesa muros** y **no se sale de la pantalla**.

- [ ] **Step 6: Commit**

```bash
git add src/sima_actors.c src/sima.c include/sima.h
git commit -m "feat(sima): jugador con movimiento libre y colision por casilla"
```

---

## Task 5: Escalera y progresión de pisos

**Files:**
- Modify: `src/sima.c`, `src/sima_rooms.c`, `include/sima_rooms.h`, `src/phantom_test.c`

**Interfaces:**
- Produces: `u8 SimaRoom_NextFloor(u8 floor);` — devuelve `floor + 1`, saturando en `SIMA_FLOOR_COUNT - 1`.

- [ ] **Step 1: Escribir el test que falla**

En `src/phantom_test.c`:
```c
// Test 8 (SIMA): la progresion de pisos satura en el ultimo. Si desbordara,
// SimaRoom_GetTile leeria fuera de la tabla de salas.
static void Test_SimaFloorProgression(void)
{
    PHANTOM_ASSERT(SimaRoom_NextFloor(0) == 1, "sima-floor-0-to-1");
    PHANTOM_ASSERT(SimaRoom_NextFloor(1) == 2, "sima-floor-1-to-2");
    PHANTOM_ASSERT(SimaRoom_NextFloor(SIMA_FLOOR_COUNT - 1) == SIMA_FLOOR_COUNT - 1,
                   "sima-floor-saturates");
}
```
Llamarlo desde `PhantomTest_Run()`.

- [ ] **Step 2: Ejecutar (RED)**

Run: `./test/smoke.sh`
Expected: falla a compilar por `SimaRoom_NextFloor` no declarada.

- [ ] **Step 3: Implementar `SimaRoom_NextFloor` y el cambio de piso**

Al pisar la escalera: fundido a negro, `floor = SimaRoom_NextFloor(floor)`, repintar el tilemap, recolocar al jugador en el spawn del piso nuevo, fundido de vuelta.

- [ ] **Step 4: Ejecutar (GREEN)**

Run: `./test/smoke.sh`
Expected: `SMOKE: OK` con `:P PASS sima-floor-0-to-1`, `sima-floor-1-to-2`, `sima-floor-saturates`.

- [ ] **Step 5: Verificación visual**

Comprobar: pisar la escalera funde a negro y aparece el piso siguiente con el jugador en su sitio.

- [ ] **Step 6: Commit**

```bash
git add src/sima.c src/sima_rooms.c include/sima_rooms.h src/phantom_test.c
git commit -m "feat(sima): escalera y progresion de pisos, saturando en el ultimo"
```

---

## Task 6: Enemigos, daño por contacto y HUD de vida

**Files:**
- Modify: `src/sima_actors.c`, `src/sima.c`, `src/sima_rooms.c`, `src/phantom_test.c`

**Interfaces:**
- Produces:
  - `#define SIMA_PLAYER_MAX_HP 3`
  - `u8 SimaActors_ApplyDamage(u8 hp, u8 amount);` — resta saturando en 0
  - `bool8 SimaActors_IsPlayerDead(void);`

- [ ] **Step 1: Escribir el test que falla**

```c
// Test 9 (SIMA): el daño satura en 0. Un underflow en u8 daria 255 de vida
// y el jugador seria inmortal justo cuando deberia morir.
static void Test_SimaDamage(void)
{
    PHANTOM_ASSERT(SimaActors_ApplyDamage(3, 1) == 2, "sima-damage-normal");
    PHANTOM_ASSERT(SimaActors_ApplyDamage(1, 1) == 0, "sima-damage-to-zero");
    PHANTOM_ASSERT(SimaActors_ApplyDamage(1, 5) == 0, "sima-damage-saturates");
    PHANTOM_ASSERT(SimaActors_ApplyDamage(0, 1) == 0, "sima-damage-from-zero");
}
```

- [ ] **Step 2: Ejecutar (RED)** — Run: `./test/smoke.sh`, falla al compilar.

- [ ] **Step 3: Implementar**

```c
u8 SimaActors_ApplyDamage(u8 hp, u8 amount)
{
    if (amount >= hp)
        return 0;
    return hp - amount;
}
```
Enemigos (rata, murciélago, slime) en los `'*'` de la sala, con movimiento simple hacia el jugador. Contacto → daño + invulnerabilidad breve. HUD de corazones desde `hud.png` en BG1.

- [ ] **Step 4: Ejecutar (GREEN)** — Run: `./test/smoke.sh`, Expected: `SMOKE: OK`.

- [ ] **Step 5: Verificación visual** — los enemigos se mueven, quitan vida al tocarte, el HUD baja y a 0 corazones mueres.

- [ ] **Step 6: Commit**

```bash
git add src/sima_actors.c src/sima.c src/sima_rooms.c src/phantom_test.c
git commit -m "feat(sima): enemigos, daño por contacto y HUD de vida"
```

---

## Task 7: Ataque del jugador

**Files:**
- Modify: `src/sima_actors.c`

- [ ] **Step 1: Sprite de arma** — usar `weapons.png` (Task 1), animación breve delante del jugador según su dirección.
- [ ] **Step 2: Colisión arma-enemigo** — un enemigo tocado reproduce su animación de muerte y desaparece.
- [ ] **Step 3: Build + smoke** — `make modern -j$(nproc)` y `./test/smoke.sh` → `SMOKE: OK`.
- [ ] **Step 4: Verificación visual** — atacar con A mata ratas, murciélagos y slimes.
- [ ] **Step 5: Commit**

```bash
git add src/sima_actors.c
git commit -m "feat(sima): ataque del jugador y muerte de enemigos"
```

---

## Task 8: El Slime King invencible

**Files:**
- Modify: `src/sima_actors.c`, `src/sima.c`, `src/phantom_test.c`

**Interfaces:**
- Produces: `bool8 SimaActors_BossTakesDamage(void);` — devuelve **siempre** `FALSE`.

- [ ] **Step 1: Escribir el test que falla**

```c
// Test 10 (SIMA): el jefe es invencible POR CONTRATO, no por tener mucha vida.
// Es la regla que el spec general fija para los combates de la isla ("el juego
// nunca te debe una pelea justa") y lo que garantiza que ningun jugador pueda
// romper el guion superando el piso 7 de Carlos.
static void Test_SimaBossInvincible(void)
{
    u32 i;
    bool8 everDamaged = FALSE;
    for (i = 0; i < 100; i++)
        if (SimaActors_BossTakesDamage())
            everDamaged = TRUE;
    PHANTOM_ASSERT(everDamaged == FALSE, "sima-boss-invincible");
}
```

- [ ] **Step 2: Ejecutar (RED)** — `./test/smoke.sh` falla al compilar.

- [ ] **Step 3: Implementar el jefe**

```c
// Invencible a proposito: golpearlo solo dispara el parpadeo. Ver §4 del spec.
bool8 SimaActors_BossTakesDamage(void)
{
    return FALSE;
}
```
El sprite va en **`SPRITE_SIZE(64x64)` con el arte de 48×48 centrado y relleno transparente** — 48×48 no es un tamaño legal de GBA. Golpearlo reproduce los frames claros de "golpeado" del pack. Su contacto mata.

- [ ] **Step 4: Ejecutar (GREEN)** — `SMOKE: OK` con `:P PASS sima-boss-invincible`.

- [ ] **Step 5: Verificación visual** — en el piso 3 aparece el jefe; golpearlo lo hace parpadear pero no muere; te mata.

- [ ] **Step 6: Commit**

```bash
git add src/sima_actors.c src/sima.c src/phantom_test.c
git commit -m "feat(sima): el Slime King es invencible por contrato

No es dificultad inflada: es la primera vez que el juego enuncia la regla
que rige Sombraluna, y es lo que impide que un jugador bueno rompa el guion
superando el piso 7 de Carlos. El test lo fija como contrato, no como
un valor de vida alto que alguien podria bajar sin darse cuenta.

El arte es de 48x48, que NO es un tamaño legal de sprite en GBA, asi que va
centrado en uno de 64x64 con relleno transparente."
```

---

## Task 9: Marcador y entrada del nombre

**Files:**
- Create: `src/sima_score.c`
- Modify: `src/sima.c`, `include/sima.h`, `src/phantom_test.c`

**Interfaces:**
- Produces:
  - `void SimaScore_Start(u8 floorReached, MainCallback nextCB);`
  - `void SimaScore_CommitName(const u8 *name);` — copia a `gSaveBlock2Ptr->playerName`

- [ ] **Step 1: Escribir el test que falla**

```c
// Test 11 (SIMA): el nombre escrito en el marcador ES el nombre de la partida.
// Cierra la tension del andamiaje de New Game, que fijaba el nombre a "?".
//
// OJO con el literal: tiene que pasar por _() para quedar en la codificacion
// de charmap.txt. Un "VAL" en C crudo son bytes ASCII, que NO es lo que el
// juego guarda en playerName -- el test pasaria y el nombre saldria basura.
static const u8 sTestName[] = _("VAL");

static void Test_SimaNameWritesPlayerName(void)
{
    gSaveBlock2Ptr->playerName[0] = 0xFF;   // EOS: nombre vacio
    SimaScore_CommitName(sTestName);
    PHANTOM_ASSERT(gSaveBlock2Ptr->playerName[0] == sTestName[0], "sima-name-written");
    PHANTOM_ASSERT(gSaveBlock2Ptr->playerName[3] == 0xFF, "sima-name-terminated");
}
```

- [ ] **Step 2: Ejecutar (RED)** — falla al compilar.

- [ ] **Step 3: Implementar el marcador**

Maquetación (§7 del spec), todo en la paleta de 2 tonos:
```
     ------- SIMA -------
        PISO ALCANZADO
              03
        NOMBRE: ______
        A B C D E F G H I J
        K L M N Ñ O P Q R S
        T U V W X Y Z  <-  OK
        RECORD
        PISO 07   CAR
```
Rejilla de letras propia con cursor; **no** usar `DoNamingScreen` (es la pantalla colorida de Pokémon y rompería la ficción de Game Boy justo en el momento clave). `SimaScore_CommitName` copia a `gSaveBlock2Ptr->playerName` respetando `PLAYER_NAME_LENGTH` (7) y terminando en `EOS`.

- [ ] **Step 4: Ejecutar (GREEN)** — `SMOKE: OK` con `:P PASS sima-name-written`.

- [ ] **Step 5: Verificación visual** — al morir sale el marcador con el piso alcanzado y `PISO 07 CAR`; se escribe el nombre y se confirma.

- [ ] **Step 6: Commit**

```bash
git add src/sima_score.c src/sima.c include/sima.h src/phantom_test.c
git commit -m "feat(sima): marcador con el record de Carlos y entrada del nombre

El nombre se escribe dentro de la ficcion de la consola, junto a la marca
del hermano -- ese paralelismo es el diseño, no un tramite. Y ese nombre es
el de la partida, lo que cierra la tension del andamiaje fijado a \"?\"."
```

---

## Task 10: El capitán y salida al overworld

**Files:**
- Modify: `src/sima.c`, `src/minigame_pre.c`

- [ ] **Step 1: Escena del capitán** — tras confirmar el nombre, fundido a negro y una caja de narración en el mismo registro de la apertura (segunda persona, presente, sin adjetivos de afecto): el capitán anuncia la isla. Reutilizar el patrón de `minigame_pre.c` (colores explícitos con fondo transparente, `GetPlayerTextSpeedDelay()`, `\p` al final, `RunTextPrinters()` en el callback).
- [ ] **Step 2: Salida** — `SetMainCallback2(CB2_NewGame)` para entrar al overworld.
- [ ] **Step 3: Build + smoke** — `SMOKE: OK`.
- [ ] **Step 4: Verificación visual de la cadena completa** — título → vidrio → camarote → SIMA → jefe → marcador → nombre → capitán → overworld, sin cuelgues.
- [ ] **Step 5: Commit**

```bash
git add src/sima.c src/minigame_pre.c
git commit -m "feat(sima): el capitan anuncia la isla y entrega al overworld"
```

---

## Task 11: Retirar el shmup y actualizar créditos

Se hace **al final**, cuando el crawler ya funciona, no antes.

**Files:**
- Delete: `src/minigame_spaceship.c`, `src/minigame_countdown.c`, `include/minigame_spaceship.h`, `include/minigame_ship.h`, `graphics/minigame_spaceship/bg_*.*`, `graphics/minigame_spaceship/player_ship.*`
- Keep: `graphics/minigame_spaceship/boy.*` (es el camarote y sigue en uso) — moverlo a `graphics/phantom_intro/`
- Modify: `CREDITS.md`

- [ ] **Step 1: Mover el asset del camarote** y actualizar los `INCBIN` de `src/minigame_pre.c`.
- [ ] **Step 2: Borrar los ficheros del shmup.**
- [ ] **Step 3: Actualizar `CREDITS.md`**

```markdown
## Graphics Credits

### Sprite Assets
- **Super Gameboy Quest — Dungeons Pack** by Toadzilla
  - https://toadzillart.itch.io/dungeons-pack
- **Super Gameboy Quest — Monster Pack** by Toadzilla
  - https://toadzillart.itch.io/monster-pack
- **Super Gameboy Quest — UI Pack** by Toadzilla
  - https://toadzillart.itch.io/ui-pack
```
(Retirar la entrada de *Classic Shmups* solo si ya no queda ningún asset suyo en el repo.)

- [ ] **Step 4: Build de los tres targets**

Run:
```bash
make modern -j$(nproc)
make PHANTOM_TEST=1 modern -j$(nproc)
make PHANTOM_DEBUG_BOOT=1 modern -j$(nproc)
```
Expected: los tres compilan y linkan limpio.

- [ ] **Step 5: Smoke** — `./test/smoke.sh` → `SMOKE: OK`.

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "chore(sima): retirar el shmup y acreditar los packs de Toadzilla

El camarote (boy.*) se conserva y se muda a graphics/phantom_intro/, que es
donde le corresponde ahora."
```

---

## Task 12: Actualizar el spec general

`2026-07-17-pokemon-phantom-design.md` sigue diciendo "shooter espacial" en cuatro sitios.

- [ ] **Step 1: Aplicar los cuatro cambios de §2 del spec de la Pieza 2.**
- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/specs/2026-07-17-pokemon-phantom-design.md
git commit -m "docs(spec): el juego de la consola es SIMA, no un shooter espacial

Alinea el spec general con la Pieza 2. El mecanismo narrativo no cambia:
prologo -> maquina del bar en el Dia 1 -> consola de Carlos en el Dia 2."
```

---

## Notas para quien ejecute

- **`RunTextPrinters()`** debe estar en el callback principal de cualquier modo que imprima texto. `minigame_pre.c` no lo tenía y con `speed = 0` el bug quedaba oculto; con velocidad real el texto simplemente no aparece.
- **Nunca usar `speed = 0`** en `AddTextPrinter*`: no imprime, vuelca la cadena en un bucle y marca el printer inactivo, con lo que el `\p` deja de esperar y su flecha ▼ queda congelada. Usar `GetPlayerTextSpeedDelay()`.
- **`BeginNormalPaletteFade` devuelve `FALSE` y no hace nada si ya hay un fundido activo** — no encola. Gatear siempre con `!gPaletteFade.active`.
- **Aritmética de fundidos:** `deltaY = 2`, así que 0→16 son 8 pasos, y cada paso cuesta `delay + 2` frames. Total = `8 * (delay + 2)`.
- **Si un conteo de colores de un asset parece raro**, filtrar por alfa: contar tuplas RGBA incluye los píxeles transparentes, que llevan RGB basura debajo e inflan la cifra.
