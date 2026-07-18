# Fase Debug visual + Sandbox — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax.

**Goal:** (1) Un harness de debugging visual autónomo (Python/`mgba`) versionado, y (2) un mapa sandbox que valida cada mecánica del Día 1 (mapa nuevo, NPC, mini-escena, desaturación de paleta, dormir) en aislamiento, verificado visualmente con (1).

**Architecture:** El harness visual es un paquete Python en `tools/phantom-debug/` que envuelve el paquete `mgba` (bindings de libmgba): carga la ROM, inyecta inputs, avanza frames, captura PNG y lee memoria por símbolo (`.map`/`nm` + `pyelftools`). El sandbox es un mapa nuevo vía el pipeline JSON del repo + scripts `.inc` + un `src/phantom.c` con specials en C reusables; su lógica se testea con el smoke test in-ROM (Fase 0-1) y su parte visual con el harness. Nada de esto es contenido final: el sandbox es andamiaje desechable.

**Tech Stack:** pokeemerald (`make modern`/gcc), Python 3.11 (venv vía `uv`), paquete `mgba` (cffi), `pyelftools`, el pipeline `mapjson`, scripts de evento `.inc`.

## Global Constraints
- Build target `make modern` (gcc); nunca depender de agbcc. Release limpio (`make modern` → `pokeemerald_modern.gba` sin harness); test build → `pokeemerald_modern_test.gba`.
- El harness Python es una HERRAMIENTA versionada en `tools/phantom-debug/`, no scratch. NO se distribuye con la ROM.
- Entorno verificado: `libmgba` instalado; `uv` disponible; el paquete `mgba` solo tiene wheels cp310/cp311 → venv Python 3.11. `mgba.log.silence()` obligatorio. `set_video_buffer()` ANTES de `core.reset()`. `load_raw_state` exige `ffi.new("unsigned char[]", data)`.
- Mapas: solo son FUENTE `map.json`, `scripts.inc`, y el C. NUNCA editar `header.inc`/`events.inc`/`connections.inc` (auto-generados). El `.include` del `scripts.inc` en `data/event_scripts.s` es MANUAL (mapjson no lo genera). Registrar mapas al FINAL de estructuras (nunca insertar en medio → desplaza índices `MAP_*`).
- Tinte de paleta: vive en `gPlttBufferUnfaded` (y faded), gateado por `FLAG_PHANTOM_MEOWTH_EXECUTED` (persistente, NO `FLAG_TEMP_*`, NO gate por `VAR_PHANTOM_TIME`). NO tintar dentro del `LoadPalette` global (rompe menús/combate).
- Commits: SIN `Co-Authored-By`, SIN `Claude-Session`. No `git push`. Identificadores en inglés; texto de juego / comentarios de dominio en español.
- Docs de referencia obligatorios: `docs/design/harness-fase2-visual.md` (API mgba, gotchas) y `docs/design/guia-slice-dia1.md` (mecánicas del slice con file:line).

## File Structure
- `tools/phantom-debug/phantom_dbg/__init__.py`, `emu.py` (driver), `symbols.py` (lectura por símbolo), `cli.py` — el harness Python.
- `tools/phantom-debug/README.md` — setup (venv, sudo libmgba) y uso.
- `tools/phantom-debug/pyproject.toml` o `requirements.txt` — deps (`mgba`, `pyelftools`).
- `data/maps/PhantomSandbox/{map.json,scripts.inc}` — el mapa sandbox.
- `data/maps/map_groups.json`, `data/event_scripts.s` — registro del mapa.
- `src/phantom.c`, `include/phantom.h` — specials en C (`PhantomAdvanceDay`, `PhantomMarkExecutionSeen`) + helper de tinte.
- `data/specials.inc` — `def_special` de los specials.
- `src/fieldmap.c` (hook BG `:865`), `src/event_object_movement.c` (hook sprites `:2048`) — desaturación.
- `include/constants/flags.h` — renombrar `FLAG_UNUSED_0x020` → `FLAG_PHANTOM_MEOWTH_EXECUTED`.
- `src/new_game.c` (redirect temporal del warp inicial al sandbox), `src/overworld.c:1542` (quitar camión).
- `src/phantom_test.c` — tests headless de los specials.

---

## Task 1: Harness visual — driver del emulador + bootstrap del venv

**Files:**
- Create: `tools/phantom-debug/phantom_dbg/__init__.py`, `tools/phantom-debug/phantom_dbg/emu.py`
- Create: `tools/phantom-debug/requirements.txt`
- Create: `tools/phantom-debug/README.md`
- Create: `tools/phantom-debug/setup-venv.sh`

**Interfaces:**
- Produces: clase `Emu` en `phantom_dbg/emu.py` con: `Emu(rom_path, savestate=None)`, `.press(key, held=2, release=10)`, `.run(frames)`, `.screenshot(path)`, `.save_state(path)`, `.load_state(path)`, `.mem_u8/mem_u16/mem_u32(addr)`, y las constantes de teclas. Lo consumen Tasks 2/3.

- [ ] **Step 1: Leer la doc de referencia**

Lee `docs/design/harness-fase2-visual.md` (API `mgba`, orden `set_video_buffer` antes de `reset`, `mgba.log.silence()`, memoria por bus base-0). Es la fuente de la API.

- [ ] **Step 2: requirements + setup-venv.sh**

`tools/phantom-debug/requirements.txt`:
```
mgba==0.10.2
pyelftools
```
`tools/phantom-debug/setup-venv.sh`:
```bash
#!/usr/bin/env bash
# Requiere: sudo pacman -S --needed libmgba  (una vez; ya instalado)
set -euo pipefail
VENV="${PHANTOM_VENV:-$HOME/.venvs/mgba-py}"
uv venv --python 3.11 "$VENV"
uv pip install --python "$VENV" -r "$(dirname "$0")/requirements.txt"
echo "venv listo: $VENV  (usa: $VENV/bin/python -m phantom_dbg ...)"
```
`chmod +x tools/phantom-debug/setup-venv.sh`

- [ ] **Step 3: escribir `phantom_dbg/emu.py`**

```python
"""Driver headless de mGBA para Pokémon Phantom (verificado con libmgba)."""
import mgba.core
import mgba.image
import mgba.log

mgba.log.silence()  # mata el spam de BIOS/DMA


class Emu:
    def __init__(self, rom_path, savestate=None):
        self._core = mgba.core.load_path(rom_path)
        if self._core is None:
            raise RuntimeError(f"no se pudo cargar la ROM: {rom_path}")
        w, h = self._core.desired_video_dimensions()
        self._img = mgba.image.Image(w, h)
        self._core.set_video_buffer(self._img)   # ANTES de reset()
        self._core.reset()
        if savestate is not None:
            self.load_state(savestate)

    # --- teclas ---
    @property
    def KEY(self):
        c = self._core
        return {"A": c.KEY_A, "B": c.KEY_B, "START": c.KEY_START,
                "SELECT": c.KEY_SELECT, "UP": c.KEY_UP, "DOWN": c.KEY_DOWN,
                "LEFT": c.KEY_LEFT, "RIGHT": c.KEY_RIGHT, "L": c.KEY_L, "R": c.KEY_R}

    def run(self, frames):
        for _ in range(frames):
            self._core.run_frame()

    def press(self, key, held=2, release=10):
        k = self.KEY[key] if isinstance(key, str) else key
        self._core.add_keys(k)
        self.run(held)
        self._core.clear_keys(k)
        self.run(release)

    # --- video ---
    def screenshot(self, path):
        with open(path, "wb") as f:
            self._img.save_png(f)
        return path

    # --- memoria (bus base-0: direcciones GBA directas) ---
    def mem_u8(self, addr):  return self._core.memory.u8[addr]
    def mem_u16(self, addr): return self._core.memory.u16[addr]
    def mem_u32(self, addr): return self._core.memory.u32[addr]

    # --- savestate ---
    def save_state(self, path):
        with open(path, "wb") as f:
            f.write(bytes(self._core.save_raw_state()))

    def load_state(self, path):
        import mgba._pylib
        with open(path, "rb") as f:
            data = f.read()
        buf = mgba._pylib.ffi.new("unsigned char[]", data)
        self._core.load_raw_state(buf)

    @property
    def game_title(self): return self._core.game_title
```
`phantom_dbg/__init__.py`:
```python
from .emu import Emu
__all__ = ["Emu"]
```

- [ ] **Step 4: README con setup y un ejemplo**

`tools/phantom-debug/README.md` — documenta: el sudo `pacman -S --needed libmgba` (una vez), `./setup-venv.sh`, y un ejemplo mínimo:
```python
from phantom_dbg import Emu
e = Emu("pokeemerald_modern.gba")
e.run(160)
e.screenshot("/tmp/title.png")
print(e.game_title)
```
Nota: correr con `PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python ...`.

- [ ] **Step 5: verificar — captura real de la title screen**

Run:
```bash
~/.venvs/mgba-py/bin/python - <<'PY'
import sys; sys.path.insert(0, "tools/phantom-debug")
from phantom_dbg import Emu
e = Emu("pokeemerald_modern.gba")
e.run(160)
p = e.screenshot("/tmp/t1.png")
print("title:", e.game_title)
import os; print("PNG bytes:", os.path.getsize(p))
PY
file /tmp/t1.png
```
Expected: `title: POKEMON EMER`, PNG > 1000 bytes, `PNG image data, 240 x 160`. (El venv ya existe de la verificación; si no, corre `tools/phantom-debug/setup-venv.sh` primero.)

- [ ] **Step 6: commit**

```bash
git add tools/phantom-debug
git commit -m "feat(debug): harness visual mgba-py (driver Emu + venv setup)"
```

---

## Task 2: Harness visual — lectura de estado por símbolo

**Files:**
- Create: `tools/phantom-debug/phantom_dbg/symbols.py`
- Modify: `tools/phantom-debug/phantom_dbg/__init__.py` (exportar helpers)

**Interfaces:**
- Consumes: `Emu` (Task 1); `pokeemerald_modern.map`, `pokeemerald_modern.elf`.
- Produces: `SymbolReader(emu, map_path, elf_path)` con `.global_addr(name)`, `.struct_offset(struct, field)`, y helpers de alto nivel `.read_var(var_const_or_id)`, `.read_flag(flag_id)`, `.player_map()`, `.player_xy()`.

- [ ] **Step 1: `symbols.py` — globales por `.map`/`nm` + offsets por `pyelftools`**

```python
"""Resuelve símbolos y offsets de struct para leer estado del juego."""
import re
from elftools.elf.elffile import ELFFile

VARS_START = 0x4000   # include/constants/vars.h
FLAGS_START = 0x0      # los flags se indexan desde 0 en el bit-array

class SymbolReader:
    def __init__(self, emu, map_path, elf_path):
        self.emu = emu
        self._globals = self._parse_map(map_path)
        self._elf_path = elf_path
        self._offset_cache = {}

    def _parse_map(self, map_path):
        g = {}
        # líneas tipo: "                0x03007328                gSaveBlock1Ptr"
        pat = re.compile(r"^\s+0x([0-9a-fA-F]{8,})\s+(\w+)\s*$")
        with open(map_path) as f:
            for line in f:
                m = pat.match(line)
                if m:
                    g.setdefault(m.group(2), int(m.group(1), 16))
        return g

    def global_addr(self, name):
        return self._globals[name]

    def struct_offset(self, struct, field):
        key = (struct, field)
        if key in self._offset_cache:
            return self._offset_cache[key]
        with open(self._elf_path, "rb") as f:
            dwarf = ELFFile(f).get_dwarf_info()
            for cu in dwarf.iter_CUs():
                for die in cu.iter_DIEs():
                    if (die.tag == "DW_TAG_structure_type"
                            and die.attributes.get("DW_AT_name")
                            and die.attributes["DW_AT_name"].value == struct.encode()):
                        for ch in die.iter_children():
                            if (ch.tag == "DW_TAG_member"
                                    and ch.attributes.get("DW_AT_name")
                                    and ch.attributes["DW_AT_name"].value == field.encode()):
                                off = ch.attributes["DW_AT_data_member_location"].value
                                self._offset_cache[key] = off
                                return off
        raise KeyError(f"offset no encontrado: {struct}.{field}")

    # --- alto nivel ---
    def _sb1(self):
        return self.emu.mem_u32(self.global_addr("gSaveBlock1Ptr"))

    def read_var(self, var_id):
        base = self._sb1() + self.struct_offset("SaveBlock1", "vars")
        return self.emu.mem_u16(base + 2 * (var_id - VARS_START))

    def read_flag(self, flag_id):
        base = self._sb1() + self.struct_offset("SaveBlock1", "flags")
        byte = self.emu.mem_u8(base + (flag_id >> 3))
        return bool(byte & (1 << (flag_id & 7)))

    def player_map(self):
        base = self._sb1()
        grp = self.emu.mem_u8(base + self.struct_offset("SaveBlock1", "location") + 0)
        num = self.emu.mem_u8(base + self.struct_offset("SaveBlock1", "location") + 1)
        return (grp, num)
```
(Nota: confirma con `grep` los nombres reales de los campos: `SaveBlock1.vars`, `.flags`, `.location` en `include/global.h`; ajusta si difieren. `WarpData.mapGroup/mapNum` son los 2 primeros bytes de `location`.)

- [ ] **Step 2: exportar en `__init__.py`**
```python
from .emu import Emu
from .symbols import SymbolReader
__all__ = ["Emu", "SymbolReader"]
```

- [ ] **Step 3: verificar — leer VAR_PHANTOM_TIME en vivo tras el boot**

Run:
```bash
~/.venvs/mgba-py/bin/python - <<'PY'
import sys; sys.path.insert(0, "tools/phantom-debug")
from phantom_dbg import Emu, SymbolReader
e = Emu("pokeemerald_modern.gba")
e.run(160)
for _ in range(40):   # atravesar título+menú → New Game
    e.press("A"); e.press("START")
sr = SymbolReader(e, "pokeemerald_modern.map", "pokeemerald_modern.elf")
print("SaveBlock1.vars offset =", hex(sr.struct_offset("SaveBlock1","vars")))
print("VAR_PHANTOM_TIME =", sr.read_var(0x404E))
PY
```
Expected: offset `0x139c`, y `VAR_PHANTOM_TIME = 1` (PROLOGUE) tras entrar a New Game — o `0` si no llegó a New Game (ajusta los frames). Lo importante: la lectura por símbolo funciona sin error. (Si el `.elf` no tiene DWARF de juego, `make clean && make DINFO=1 modern` primero — ver la doc.)

- [ ] **Step 4: commit**
```bash
git add tools/phantom-debug/phantom_dbg
git commit -m "feat(debug): lectura de estado por símbolo (.map + pyelftools)"
```

---

## Task 3: PHANTOM_DEBUG_BOOT (arranque directo al juego) + CLI phantom_dbg

> **Descubrimiento (jul 2026):** el front-end del hack es un dead-end — title screen → (START) → minijuego shmup inacabado → vuelve al title. Nadie llama a `CB2_InitMainMenu`; NO hay ruta jugable al overworld. Para desarrollar/testear el juego mientras el front-end esté sin terminar, esta tarea añade un flag de compilación `PHANTOM_DEBUG_BOOT` que arranca directo en `CB2_NewGame`, generando una ROM aparte `pokeemerald_modern_debug.gba`. Release y título INTACTOS.

**Files:**
- Modify: `Makefile` (flag `PHANTOM_DEBUG_BOOT` + variante de object-dir/ROM `_debug`)
- Modify: `src/intro.c` (bajo `PHANTOM_DEBUG_BOOT`, rutear copyright → `CB2_NewGame` en vez del título)
- Create: `tools/phantom-debug/phantom_dbg/cli.py`, `tools/phantom-debug/phantom_dbg/__main__.py`

**Interfaces:**
- Consumes: `Emu`, `SymbolReader`; `CB2_NewGame` (`src/overworld.c:1532`, decl. `include/overworld.h:134`).
- Produces: build `make PHANTOM_DEBUG_BOOT=1 modern` → `pokeemerald_modern_debug.gba` (arranca en el overworld de New Game). CLI `python -m phantom_dbg` con `screenshot`, `boot`, `read`. `boot(emu, frames)` reusable.

### Parte A — flag PHANTOM_DEBUG_BOOT (game-side)

- [ ] **Step 1: leer la lógica actual de variantes en el Makefile** — la Fase 0-1 ya introdujo `PHANTOM_TEST` con object-dir separado (`build/modern_test`) y ROM `pokeemerald_modern_test.gba`. Localiza: el `PHANTOM_TEST ?= 0` temprano (cerca de `MODERN ?= 0`), el cálculo de `MODERN_OBJ_DIR_NAME`, el de `MODERN_ROM_NAME`, y el `ifeq ($(PHANTOM_TEST),1) CPPFLAGS += -DPHANTOM_TEST`.

- [ ] **Step 2: generalizar a un sufijo de variante (`_test` / `_debug` / vacío)** — añadir `PHANTOM_DEBUG_BOOT ?= 0` junto al `PHANTOM_TEST ?= 0`, y derivar un sufijo (mutuamente excluyentes; test tiene prioridad si ambos se pasaran):
```make
ifeq ($(PHANTOM_TEST),1)
  PHANTOM_SUFFIX := _test
else ifeq ($(PHANTOM_DEBUG_BOOT),1)
  PHANTOM_SUFFIX := _debug
else
  PHANTOM_SUFFIX :=
endif
```
Hacer que `MODERN_OBJ_DIR_NAME` use `$(BUILD_DIR)/modern$(PHANTOM_SUFFIX)` y `MODERN_ROM_NAME` use `$(FILE_NAME)_modern$(PHANTOM_SUFFIX).gba` (reemplazando la lógica `_test`-específica). Y en el bloque de flags: mantener `-DPHANTOM_TEST` si test, y añadir `-DPHANTOM_DEBUG_BOOT` si debug:
```make
ifeq ($(PHANTOM_DEBUG_BOOT),1)
  CPPFLAGS += -DPHANTOM_DEBUG_BOOT
endif
```
Confirma que `make modern` (sin flags) sigue produciendo `pokeemerald_modern.gba` desde `build/modern` (release intacto).

- [ ] **Step 3: rutear el arranque en `src/intro.c`** — hay dos transiciones copyright→título: en `MainCB2_EndIntro` (~`intro.c:70`, `SetMainCallback2(CB2_InitTitleScreen)`) y en `SetUpCopyrightScreen` caso `COPYRIGHT_START_INTRO` (~`intro.c:135`). Bajo `#ifdef PHANTOM_DEBUG_BOOT` rutear ambas a `CB2_NewGame`:
```c
#ifdef PHANTOM_DEBUG_BOOT
    SetMainCallback2(CB2_NewGame);   // debug: salta título+minijuego, entra directo al juego
#else
    SetMainCallback2(CB2_InitTitleScreen);
#endif
```
Añadir `#include "overworld.h"` en `intro.c` si no está (para `CB2_NewGame`). Las prerequisitas de `CB2_NewGame` (save blocks + heap) ya están inicializadas por `CB2_InitCopyrightScreenAfterBootup` antes de llegar aquí.

- [ ] **Step 4: build + verificación VISUAL del debug-boot** —
Run: `make PHANTOM_DEBUG_BOOT=1 DINFO=1 modern -j$(nproc)` (debe compilar/linkar y producir `pokeemerald_modern_debug.gba`). Luego:
```bash
~/.venvs/mgba-py/bin/python - <<'PY'
import sys; sys.path.insert(0,"tools/phantom-debug")
from phantom_dbg import Emu, SymbolReader
e = Emu("pokeemerald_modern_debug.gba"); e.run(300)
e.screenshot("/tmp/dbgboot.png")
sr = SymbolReader(e, "pokeemerald_modern_debug.map", "pokeemerald_modern_debug.elf")
print("phantomTime =", sr.read_var(0x404E), "map =", sr.player_map())
PY
file /tmp/dbgboot.png
```
Expected: `phantomTime = 1` (PROLOGUE → prueba que `NewGameInitData` corrió), y el PNG muestra contenido de JUEGO (interior del camión / Littleroot), NO el título ni el minijuego. El controlador MIRA `/tmp/dbgboot.png`. Verifica también que `make modern` (release) sigue limpio y su ROM NO arranca en New Game (opcional: bootear `pokeemerald_modern.gba` y confirmar que sale el título/copyright).

- [ ] **Step 5: commit (game-side)**
```bash
git add Makefile src/intro.c
git commit -m "feat(debug): flag PHANTOM_DEBUG_BOOT arranca directo en CB2_NewGame (ROM _debug)"
```

### Parte B — CLI phantom_dbg (tool-side)

- [ ] **Step 6: `cli.py`** — `boot()` ya NO navega menús (la ROM debug arranca directo al overworld): solo avanza frames. La ROM por defecto es la de debug.
```python
import argparse
from .emu import Emu
from .symbols import SymbolReader

DEF_ROM = "pokeemerald_modern_debug.gba"
DEF_MAP = "pokeemerald_modern_debug.map"
DEF_ELF = "pokeemerald_modern_debug.elf"

def boot(emu, frames=300):
    """La ROM debug (PHANTOM_DEBUG_BOOT) arranca directo en el overworld: solo avanza frames."""
    emu.run(frames)

def main(argv=None):
    p = argparse.ArgumentParser(prog="phantom_dbg")
    p.add_argument("--rom", default=DEF_ROM)
    p.add_argument("--map", default=DEF_MAP)
    p.add_argument("--elf", default=DEF_ELF)
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("screenshot"); s.add_argument("out"); s.add_argument("--frames", type=int, default=300)
    b = sub.add_parser("boot"); b.add_argument("--screenshot"); b.add_argument("--frames", type=int, default=300); b.add_argument("--read", action="append", default=[])
    r = sub.add_parser("read"); r.add_argument("kind", choices=["var","flag"]); r.add_argument("id"); r.add_argument("--frames", type=int, default=300)
    args = p.parse_args(argv)

    emu = Emu(args.rom)
    emu.run(args.frames)
    if args.cmd == "screenshot":
        print(emu.screenshot(args.out))
    elif args.cmd == "boot":
        if args.screenshot: print(emu.screenshot(args.screenshot))
        if args.read:
            sr = SymbolReader(emu, args.map, args.elf)
            for tok in args.read:
                print(tok, "=", sr.read_var(int(tok, 0)))
    elif args.cmd == "read":
        sr = SymbolReader(emu, args.map, args.elf)
        v = sr.read_var(int(args.id,0)) if args.kind=="var" else sr.read_flag(int(args.id,0))
        print(v)
    return 0
```
`__main__.py`:
```python
import sys
from .cli import main
sys.exit(main())
```

- [ ] **Step 7: verificar la CLI**
Run desde la raíz del repo:
```bash
PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python -m phantom_dbg \
  boot --screenshot /tmp/cli.png --read 0x404E
```
Expected: imprime la ruta del PNG y `0x404E = 1`. `file /tmp/cli.png` → PNG 240×160 de contenido de juego. (Usa las rutas por defecto `pokeemerald_modern_debug.*`.)

- [ ] **Step 8: commit (tool-side)**
```bash
git add tools/phantom-debug/phantom_dbg
git commit -m "feat(debug): CLI phantom_dbg (screenshot/boot/read sobre la ROM debug)"
```

---

## Task 4: Mapa sandbox + redirect temporal del arranque

**Files:**
- Create: `data/maps/PhantomSandbox/map.json`, `data/maps/PhantomSandbox/scripts.inc`
- Modify: `data/maps/map_groups.json`, `data/event_scripts.s`
- Modify: `src/new_game.c` (`WarpToTruck`: destino → sandbox), `src/overworld.c:1542` (quitar camión)

**Interfaces:**
- Consumes: la guía `docs/design/guia-slice-dia1.md` (Fase 0 y 1).
- Produces: `MAP_PHANTOM_SANDBOX` alcanzable; New Game aterriza en él.

- [ ] **Step 1: leer la guía (Fase 0 y 1)** — `docs/design/guia-slice-dia1.md`, secciones "Fase 0" y "Fase 1". Elegir un layout vanilla a reusar (p.ej. `LAYOUT_DEWFORD_TOWN`, 20×20) y copiar coords caminables de `data/maps/DewfordTown/map.json`.

- [ ] **Step 2: `data/maps/PhantomSandbox/map.json`** — `id="MAP_PHANTOM_SANDBOX"`, `name="PhantomSandbox"`, `layout="LAYOUT_DEWFORD_TOWN"`, `music`, `region_map_section`, `map_type="MAP_TYPE_TOWN"`, `connections: null`, `warp_events: []` (por ahora), `object_events: []`, `coord_events: []`, `bg_events: []`. Copiar los campos exactos de `data/maps/DewfordTown/map.json` como plantilla.

- [ ] **Step 3: `data/maps/PhantomSandbox/scripts.inc`** (mínimo obligatorio):
```
PhantomSandbox_MapScripts::
	.byte 0
```

- [ ] **Step 4: registrar en `data/maps/map_groups.json`** — añadir al FINAL de `group_order` un grupo `"gMapGroup_Phantom"` y la entrada `"gMapGroup_Phantom": ["PhantomSandbox"]`. El string debe ser idéntico al nombre de carpeta y al `name` del JSON.

- [ ] **Step 5: `.include` MANUAL en `data/event_scripts.s`** — junto a los otros `.include "data/maps/.../scripts.inc"` añadir:
```
	.include "data/maps/PhantomSandbox/scripts.inc"
```
(mapjson NO genera esto; sin él linka mal.)

- [ ] **Step 6: redirigir el arranque** — en `src/new_game.c` `WarpToTruck()` (`:135-139`) cambiar el destino a `MAP_PHANTOM_SANDBOX` con coords caminables:
```c
    SetWarpDestination(MAP_GROUP(MAP_PHANTOM_SANDBOX), MAP_NUM(MAP_PHANTOM_SANDBOX), WARP_ID_NONE, 6, 6);
    WarpIntoMap();
```
Y en `src/overworld.c:1542` cambiar `gFieldCallback = ExecuteTruckSequence;` por `gFieldCallback = FieldCB_WarpExitFadeFromBlack;`. (Comentar el porqué: redirect temporal del sandbox.)

- [ ] **Step 7: build + verificación VISUAL**
Run: `make modern -j$(nproc)` (debe compilar y linkar) y luego:
```bash
cd tools/phantom-debug && PYTHONPATH=. ~/.venvs/mgba-py/bin/python -m phantom_dbg \
  --rom ../../pokeemerald_modern.gba --map ../../pokeemerald_modern.map --elf ../../pokeemerald_modern.elf \
  boot-newgame --screenshot /tmp/sandbox.png --read 0x404E
```
Expected: linka sin error; el PNG muestra al jugador de pie en el mapa sandbox (tileset de Dewford), NO el camión ni Littleroot. El controlador MIRA `/tmp/sandbox.png`. (Ajustar coords si el spawn cae en colisión.)

- [ ] **Step 8: commit**
```bash
git add data/maps/PhantomSandbox data/maps/map_groups.json data/event_scripts.s src/new_game.c src/overworld.c
git commit -m "feat(sandbox): mapa PhantomSandbox + arranque de New Game redirigido (temporal)"
```

---

## Task 5: Specials en C + NPC con diálogo + mini-escena + tests headless

**Files:**
- Create: `src/phantom.c`, `include/phantom.h`
- Modify: `data/specials.inc` (`def_special`)
- Modify: `include/constants/flags.h` (renombrar `FLAG_UNUSED_0x020` → `FLAG_PHANTOM_MEOWTH_EXECUTED`)
- Modify: `data/maps/PhantomSandbox/map.json` (un NPC), `data/maps/PhantomSandbox/scripts.inc` (script NPC + mini-escena)
- Modify: `src/phantom_test.c` (tests de los specials)

**Interfaces:**
- Consumes: la guía (Fases 2, 3, 4, 6); `VarSet/VarGet/FlagSet/FlagGet` (`event_data.h`).
- Produces: specials `PhantomAdvanceDay`, `PhantomMarkExecutionSeen`; `FLAG_PHANTOM_MEOWTH_EXECUTED`; un NPC hablante y un trigger de mini-escena en el sandbox.

- [ ] **Step 1: renombrar el flag** — en `include/constants/flags.h` renombrar `FLAG_UNUSED_0x020` a `FLAG_PHANTOM_MEOWTH_EXECUTED` (mismo valor). Confirmar con grep que `FLAG_UNUSED_0x020` no se use en otro sitio.

- [ ] **Step 2: `include/phantom.h`**
```c
#ifndef GUARD_PHANTOM_H
#define GUARD_PHANTOM_H
void PhantomAdvanceDay(void);
void PhantomMarkExecutionSeen(void);
#endif
```

- [ ] **Step 3: `src/phantom.c`**
```c
#include "global.h"
#include "phantom.h"
#include "event_data.h"
#include "constants/phantom.h"
#include "constants/flags.h"

// Avanza el reloj narrativo (llamado al dormir).
void PhantomAdvanceDay(void)
{
    VarSet(VAR_PHANTOM_TIME, VarGet(VAR_PHANTOM_TIME) + 1);
}

// Marca que el jugador presenció la ejecución (swap de NPCs intra-día).
void PhantomMarkExecutionSeen(void)
{
    FlagSet(FLAG_PHANTOM_SAW_EXECUTION);
}
```
(Nota: `FLAG_PHANTOM_SAW_EXECUTION` = renombrar otro `FLAG_UNUSED_*` libre en `flags.h`, análogo al Step 1.)

- [ ] **Step 4: `def_special` en `data/specials.inc`** — añadir al inicio (junto a los otros `def_special`):
```
	def_special PhantomAdvanceDay
	def_special PhantomMarkExecutionSeen
```

- [ ] **Step 5: NPC + mini-escena en el sandbox** — en `map.json` añadir un `object_event` (p.ej. `graphics_id="OBJ_EVENT_GFX_OLD_MAN"`, coords caminables, `script="PhantomSandbox_EventScript_Villager"`). En `scripts.inc` añadir el script del NPC y una mini-escena disparada por interacción con un `bg_event`/coord o el propio NPC:
```
PhantomSandbox_EventScript_Villager::
	lock
	faceplayer
	msgbox PhantomSandbox_Text_Villager, MSGBOX_NPC
	release
	end

PhantomSandbox_EventScript_MiniScene::
	lockall
	fadeoutbgm 2
	delay 30
	message PhantomSandbox_Text_Acta
	waitmessage
	playse SE_BANG
	fadescreen FADE_TO_BLACK
	special PhantomMarkExecutionSeen
	setflag FLAG_PHANTOM_MEOWTH_EXECUTED
	delay 60
	fadescreen FADE_FROM_BLACK
	releaseall
	end

PhantomSandbox_Text_Villager:
	.string "Bienvenido a Sombraluna,{PAUSE 8} forastero.$"
PhantomSandbox_Text_Acta:
	.string "Se resolvió.{PAUSE 15} Como debe ser.$"
```
(Cablear `PhantomSandbox_EventScript_MiniScene` a un `bg_event` tipo sign en `map.json`. La escena enciende `FLAG_PHANTOM_MEOWTH_EXECUTED` — la Task 6 la usará para el tinte.)

- [ ] **Step 6: tests headless de los specials** — en `src/phantom_test.c`, añadir (dentro de `#ifdef PHANTOM_TEST`, registrados en `PhantomTest_Run` antes de `suite-end`):
```c
static void Test_PhantomAdvanceDay(void)
{
    VarSet(VAR_PHANTOM_TIME, PHANTOM_TIME_PROLOGUE);
    PhantomAdvanceDay();
    PHANTOM_ASSERT(VarGet(VAR_PHANTOM_TIME) == PHANTOM_TIME_DAY1, "advance-day");
}
static void Test_PhantomExecutionSeen(void)
{
    FlagClear(FLAG_PHANTOM_SAW_EXECUTION);
    PhantomMarkExecutionSeen();
    PHANTOM_ASSERT(FlagGet(FLAG_PHANTOM_SAW_EXECUTION) == TRUE, "saw-execution");
}
```
Include `"phantom.h"`, `"constants/phantom.h"`, `"constants/flags.h"` según haga falta.

- [ ] **Step 7: RED then GREEN + build + smoke**
Run: `./test/smoke.sh`. Primero, con los tests puestos pero antes de registrar los specials correctamente, deben poder fallar (RED real: p.ej. comenta el cuerpo de `PhantomAdvanceDay` → `:P FAIL advance-day`). Restaura → `SMOKE: OK`. Luego `make modern -j$(nproc)` limpio.

- [ ] **Step 8: verificación VISUAL del NPC/escena**
Run el `boot-newgame --screenshot`, luego (si el NPC/escena necesita inputs) extiende una secuencia de teclas para acercarse y hablar; captura antes/después. El controlador MIRA los PNG para confirmar el cuadro de diálogo. (La escena en sí se valida mejor en la Task 6 por el efecto de paleta.)

- [ ] **Step 9: commit**
```bash
git add src/phantom.c include/phantom.h data/specials.inc include/constants/flags.h data/maps/PhantomSandbox src/phantom_test.c
git commit -m "feat(sandbox): specials en C + NPC + mini-escena + tests headless"
```

---

## Task 6: Desaturación de paleta (BG + sprites), verificada visualmente

**Files:**
- Modify: `src/fieldmap.c` (hook BG `ApplyGlobalTintToPaletteEntries` `:865`)
- Modify: `src/event_object_movement.c` (hook sprites tras `LoadPalette(..., OBJ_PLTT_ID(slot), ...)` `:2048`)
- Create/Modify: `src/phantom.c` (helper compartido `Phantom_TintPaletteRange`)

**Interfaces:**
- Consumes: la guía (Fase 5); `FLAG_PHANTOM_MEOWTH_EXECUTED`; `gPlttBuffer*` (`palette.h:58-59`), `GET_R/G/B`, `RGB2`, `Q_8_8`.
- Produces: el mundo se desatura permanentemente tras encender la flag.

- [ ] **Step 1: leer la Fase 5 de la guía** — `docs/design/guia-slice-dia1.md`, "Fase 5". Clave: tinte en `gPlttBufferUnfaded` Y `gPlttBufferFaded`, gate por flag, aplicado en el camino de carga de paletas.

- [ ] **Step 2: helper compartido en `src/phantom.c`**
```c
#include "palette.h"
#include "constants/rgb.h"

void Phantom_TintPaletteRange(u16 offset, u16 count)
{
    u16 i;
    if (!FlagGet(FLAG_PHANTOM_MEOWTH_EXECUTED))
        return;
    for (i = 0; i < count; i++)
    {
        u16 c = gPlttBufferUnfaded[offset + i];
        s32 r = GET_R(c), g = GET_G(c), b = GET_B(c);
        u32 gray = (r * Q_8_8(0.3) + g * Q_8_8(0.59) + b * Q_8_8(0.1133)) >> 8;
        r = (r + gray) >> 1; g = (g + gray) >> 1; b = (b + gray) >> 1;
        gPlttBufferUnfaded[offset + i] = RGB2(r, g, b);
        gPlttBufferFaded[offset + i]   = RGB2(r, g, b);
    }
}
```
Declararlo en `include/phantom.h`.

- [ ] **Step 3: hook BG en `src/fieldmap.c:865`** — rellenar el stub `ApplyGlobalTintToPaletteEntries(u16 offset, u16 size)` para que llame `Phantom_TintPaletteRange(offset, size)` (incluir `phantom.h`). Verificar que las 3 ramas de `LoadTilesetPalette` (`:885,890,895`) lo invocan.

- [ ] **Step 4: hook sprites en `src/event_object_movement.c:2048`** — tras el `LoadPalette(..., OBJ_PLTT_ID(slot), PLTT_SIZE_4BPP)` de `PatchObjectPalette`, añadir `Phantom_TintPaletteRange(OBJ_PLTT_ID(slot), 16);` (incluir `phantom.h`).

- [ ] **Step 5: build + verificación VISUAL antes/después (el pay-off)**
Run: `make modern -j$(nproc)`, luego con el harness:
```bash
# ANTES: boot al sandbox, screenshot a color
python -m phantom_dbg ... boot-newgame --screenshot /tmp/before.png
```
Luego dispara la mini-escena (secuencia de inputs para tocar el trigger, que enciende `FLAG_PHANTOM_MEOWTH_EXECUTED`) y captura de nuevo tras recargar el mapa (salir y entrar, o el propio fade de la escena):
```bash
# DESPUÉS: screenshot desaturado
```
Expected: `/tmp/before.png` a color normal; `/tmp/after.png` visiblemente más gris. **El controlador COMPARA las dos imágenes** — este es el criterio de éxito de la tarea. Si el look no convence (muy gris / poco), es el momento de ajustar la mezcla (ver la nota de la guía sobre `TintPalette_CustomTone` y la fuerza).

- [ ] **Step 6: verificar que menús/combate NO se desaturan** — abre un menú (Start) por inputs y captura: la UI NO debe estar gris (confirma que el tinte no tocó el `LoadPalette` global). Screenshot de control.

- [ ] **Step 7: commit**
```bash
git add src/phantom.c include/phantom.h src/fieldmap.c src/event_object_movement.c
git commit -m "feat(sandbox): desaturacion persistente del overworld tras la ejecucion"
```

---

## Task 7: Cama (avanzar día + guardar), verificada por memoria

**Files:**
- Modify: `data/maps/PhantomSandbox/{map.json,scripts.inc}` (bg_event cama + script)

**Interfaces:**
- Consumes: la guía (Fase 4); `PhantomAdvanceDay` (Task 5); `special HealPlayerParty`, `Common_EventScript_SaveGame`.
- Produces: interactuar con la cama avanza `VAR_PHANTOM_TIME` y guarda.

- [ ] **Step 1: bg_event "sign" cama en `map.json`** — un `bg_event` tipo sign sobre una casilla, `script="PhantomSandbox_EventScript_Bed"`.

- [ ] **Step 2: script de la cama en `scripts.inc`**
```
PhantomSandbox_EventScript_Bed::
	lockall
	fadescreen FADE_TO_BLACK
	special HealPlayerParty
	special PhantomAdvanceDay
	call Common_EventScript_SaveGame
	fadescreen FADE_FROM_BLACK
	releaseall
	end
```
(Orden crítico: avanzar día ANTES de guardar.)

- [ ] **Step 3: build + verificación VISUAL + por memoria**
Run: `make modern -j$(nproc)`. Con el harness: boot al sandbox, leer `VAR_PHANTOM_TIME` (debe ser 1/PROLOGUE... o el valor de arranque), inyectar inputs para ir a la cama e interactuar (aceptar el guardado), y volver a leer `VAR_PHANTOM_TIME` — debe haber incrementado. El controlador confirma por `--read 0x404E` antes/después y mira el PNG del fundido.
Expected: el valor de `VAR_PHANTOM_TIME` sube en 1 tras dormir.

- [ ] **Step 4: commit**
```bash
git add data/maps/PhantomSandbox
git commit -m "feat(sandbox): cama que avanza el dia y guarda"
```

---

## Resultado de la fase
Un harness de debugging visual autónomo versionado (screenshots + estado por símbolo + savestates) y un sandbox que valida, VISTO funcionando, cada mecánica del Día 1: mapa nuevo, NPC/diálogo, mini-escena scripteada, desaturación persistente de paleta, y dormir/avanzar día. Base para el plan del **Día 1 real** (puerto/pueblo de Sombraluna + la ejecución del Meowth), donde las decisiones de look/tileset se toman con capturas reales delante.

**Nota:** el redirect del arranque al sandbox (Task 4) es TEMPORAL; el plan del Día 1 lo re-apunta al puerto real. El sandbox puede conservarse como banco de pruebas o retirarse.
