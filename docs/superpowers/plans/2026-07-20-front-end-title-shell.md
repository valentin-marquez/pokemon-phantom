# Front-end del título (cáscara) — Plan de implementación

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convertir el título en el front-end real: PRESS START → vidrio impactado → (sin save: nueva partida directa / con save: menú Nueva·Continúa) → ruta.

**Architecture:** Un módulo aditivo nuevo `src/phantom_intro.c` hospeda el efecto de vidrio (task + máquina de estados) y el menú/enrutado (overlay por sprites sobre el título vivo). `title_screen.c` solo cambia una rama: al pulsar A/START llama a `PhantomIntro_OnStartPressed()` en vez de ir al minijuego. Nada de lo ya hecho (mareo, desaturación, sandbox, harness) se toca.

**Tech Stack:** C (pokeemerald / arm-none-eabi-gcc, `make modern`), sprites OBJ + registros GPU (BLDY/BLDCNT, BG offsets), `ScanlineEffect` no se usa aquí, `gPaletteFade` para el fundido. Assets generados por script Python (PIL) → PNG → pipeline gbagfx. Verificación: `make modern`, `./test/smoke.sh`, harness visual (`tools/phantom-debug`).

## Global Constraints

- **No tocar** `src/phantom_fx.c`, `src/phantom.c`, el sandbox, `src/phantom_test.c`, ni las mecánicas del Día 1. Solo se crea `phantom_intro.c`/`.h`, se editan ~6 líneas de `title_screen.c`, y se añaden assets en `graphics/phantom_intro/`.
- **`.bss` discipline:** nada de estáticos de archivo inicializados a no-cero (van a `.data`, que el ld modern descarta → error de link). Usar estáticos zero-init. Patrón ya usado en `phantom_fx.c`.
- **Español** en todo el texto de cara al jugador. Sin acentos en las etiquetas del menú generadas por sprite (la fuente pixel del script no incluye glifos acentuados): usar `NUEVA PARTIDA` y `CONTINUAR`.
- **Commits:** sin `Co-Authored-By` ni `Claude-Session`. No hacer `git push`.
- **El vidrio SOLO se dispara al confirmar una opción que cambia de pantalla**, nunca al navegar el menú.
- **`entradaIntro` = `CB2_InitMinigameShip`** (único punto que la Pieza 2 sustituirá).
- Los tres builds (release / `PHANTOM_TEST` / `PHANTOM_DEBUG_BOOT`) siguen mutuamente excluyentes; `PHANTOM_DEBUG_BOOT` sigue saltando el título.

## File Structure

- `include/phantom_intro.h` — interfaz pública: `void PhantomIntro_OnStartPressed(void);`
- `src/phantom_intro.c` — efecto de vidrio (`PhantomGlass_Start`) + menú overlay + enrutado. Recogido automáticamente por el glob `src/*.c` del Makefile (como `phantom_fx.c`); no hace falta editar el Makefile.
- `src/title_screen.c` — editar la rama de input de `Task_TitleScreenMain` (líneas ~529-534) para llamar a `PhantomIntro_OnStartPressed()`.
- `graphics/phantom_intro/gen.py` — script Python (PIL) que genera `crack.png` (motivo de impacto central con grietas radiales) y `menu.png` (las dos etiquetas + cursor en una hoja de sprites). Commiteado como referencia reproducible.
- `graphics/phantom_intro/crack.png`, `graphics/phantom_intro/menu.png` — PNGs fuente (commiteados). Sus `.4bpp`/`.gbapal` son artefactos de build (no commiteados), generados por gbagfx al hacer INCBIN.

---

## Task 1: Módulo + enrutado por save (sin vidrio, sin menú todavía)

Mueve la decisión de arranque al módulo nuevo: sin save → minijuego; con save → continuar. Todavía sin animación de vidrio ni menú (se añaden en tareas siguientes). Preserva el comportamiento visible actual para "sin save" (título → minijuego).

**Files:**
- Create: `include/phantom_intro.h`
- Create: `src/phantom_intro.c`
- Modify: `src/title_screen.c` (rama de input de `Task_TitleScreenMain`)

**Interfaces:**
- Consumes: `gSaveFileStatus`, `SAVE_STATUS_OK` (de `save.h`); `CB2_InitMinigameShip` (de `minigame_ship.h`); `CB2_ContinueSavedGame` (de `main_menu.h`); `FadeOutBGM`, `BeginNormalPaletteFade`, `SetMainCallback2`.
- Produces: `void PhantomIntro_OnStartPressed(void);`

- [ ] **Step 1: Crear el header**

`include/phantom_intro.h`:
```c
#ifndef GUARD_PHANTOM_INTRO_H
#define GUARD_PHANTOM_INTRO_H

// Front-end del título. Llamado por title_screen.c al pulsar A/START.
// Decide la ruta (nueva partida / continuar) y reproduce el vidrio impactado.
void PhantomIntro_OnStartPressed(void);

#endif // GUARD_PHANTOM_INTRO_H
```

- [ ] **Step 2: Crear el módulo con el enrutado (sin vidrio aún)**

`src/phantom_intro.c`:
```c
#include "global.h"
#include "phantom_intro.h"
#include "main.h"
#include "sound.h"
#include "palette.h"
#include "save.h"
#include "main_menu.h"
#include "minigame_ship.h"
#include "constants/rgb.h"

// Punto de entrada de la secuencia de intro (Pieza 2 lo sustituirá).
#define ENTRADA_INTRO CB2_InitMinigameShip

static void GoTo(MainCallback nextCB)
{
    FadeOutBGM(4);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    SetMainCallback2(nextCB);
}

void PhantomIntro_OnStartPressed(void)
{
    if (gSaveFileStatus == SAVE_STATUS_OK)
        GoTo(CB2_ContinueSavedGame);   // temporal: menú se añade en Task 4
    else
        GoTo(ENTRADA_INTRO);
}
```
Nota: el `SetMainCallback2(nextCB)` aquí va con fade a negro simple; la task siguiente lo reemplaza por la máquina del vidrio. `CB2_ContinueSavedGame` durante el fade funciona porque el título corre `UpdatePaletteFade` en su `MainCB2`; pero como cambiamos el callback de inmediato, el fade lo termina el nuevo callback. Para preservar el comportamiento actual (fade completo antes de saltar) se refina en Task 2 con la task del vidrio. En Task 1 aceptamos el salto directo tras iniciar el fade (igual que hará el vidrio: el nextCB se llama al completar).

- [ ] **Step 3: Enganchar el título**

En `src/title_screen.c`, añadir el include cerca de los otros (tras `#include "minigame_ship.h"`):
```c
#include "phantom_intro.h"
```
Reemplazar el cuerpo de la rama de input en `Task_TitleScreenMain` (hoy es `FadeOutBGM(4); BeginNormalPaletteFade(...); SetMainCallback2(CB2_TaskFadeOutToMinigame);`) por:
```c
    if (JOY_NEW(A_BUTTON) || JOY_NEW(START_BUTTON))
    {
        PhantomIntro_OnStartPressed();
    }
```

- [ ] **Step 4: Build (verificación de compilación y link .bss)**

Run: `make modern -j$(nproc)`
Expected: compila y linka limpio (`pokeemerald_modern.gba` generado, sin error de `.data`/`.bss`).

- [ ] **Step 5: Smoke**

Run: `./test/smoke.sh`
Expected: `SMOKE: OK`

- [ ] **Step 6: Verificación visual (sin save → minijuego)**

Run: `PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python -m phantom_dbg boot --frames 220 --press start --screenshot $CLAUDE_JOB_DIR/tmp/t1.png`
(Arranca desde power-on sobre la ROM release fresca sin save; pulsa START en el título.)
Expected: tras el fade, aparece la pantalla del minijuego (no vuelve al título). Confirmar en `t1.png`.

- [ ] **Step 7: Commit**

```bash
git add include/phantom_intro.h src/phantom_intro.c src/title_screen.c
git commit -m "feat(intro): módulo de front-end + enrutado por save (sin vidrio aún)"
```

---

## Task 2: Efecto de vidrio (flash + sacudida + fundido, sin grietas)

Introduce `PhantomGlass_Start(nextCB)` como task-máquina de estados y lo mete en el enrutado. Todavía sin la textura de grietas (Task 3): solo impacto (flash blanco vía BLDY + SFX), sacudida de las BG, aguante y fundido a negro, y al completar `SetMainCallback2(nextCB)`.

**Files:**
- Modify: `src/phantom_intro.c`

**Interfaces:**
- Consumes: `CreateTask`, `DestroyTask`, `FindTaskIdByFunc`, `gTasks` (de `task.h`); `SetGpuReg`, `GetGpuReg`, `REG_OFFSET_BLDCNT/BLDY/BG0-3 HOFS/VOFS` (de `gpu_regs.h`); `PlaySE` + `SE_ICE_BREAK`, `SE_ICE_CRACK` (de `sound.h`/`constants/songs.h`); `BeginNormalPaletteFade`, `gPaletteFade` (de `palette.h`); `gSineTable` opcional.
- Produces: `static void PhantomGlass_Start(MainCallback nextCB);`

- [ ] **Step 1: Añadir includes y estáticos .bss**

Añadir a los includes de `src/phantom_intro.c`:
```c
#include "task.h"
#include "gpu_regs.h"
#include "trig.h"
#include "constants/songs.h"
```
Añadir estáticos (zero-init) y prototipos tras los `#define`:
```c
static MainCallback sGlassNextCB;   // a dónde ir al terminar
static u8 sGlassPhase;              // 0=impacto,1=sacudida,2=aguanta,3=fundido
static u8 sGlassTimer;

static void Task_PhantomGlass(u8 taskId);
```

- [ ] **Step 2: Implementar el arranque del efecto**

```c
// Reproduce el vidrio impactado y, al terminar el fundido, salta a nextCB.
static void PhantomGlass_Start(MainCallback nextCB)
{
    sGlassNextCB = nextCB;
    sGlassPhase = 0;
    sGlassTimer = 0;
    FadeOutBGM(4);
    PlaySE(SE_ICE_BREAK);
    // Flash blanco: mezcla la pantalla hacia el blanco vía BLDY sobre todas las capas.
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_ALL | BLDCNT_EFFECT_LIGHTEN);
    SetGpuReg(REG_OFFSET_BLDY, 16);   // máximo blanco en el impacto
    if (FindTaskIdByFunc(Task_PhantomGlass) == TASK_NONE)
        CreateTask(Task_PhantomGlass, 0);
}
```

- [ ] **Step 3: Implementar la máquina de estados**

```c
#define GLASS_SHAKE_FRAMES 14
#define GLASS_HOLD_FRAMES  20
#define GLASS_SHAKE_MAX    3   // px

static void Task_PhantomGlass(u8 taskId)
{
    sGlassTimer++;
    switch (sGlassPhase)
    {
    case 0: // impacto: bajar el flash en ~4 frames
        if (sGlassTimer >= 4)
        {
            SetGpuReg(REG_OFFSET_BLDY, 0);
            sGlassPhase = 1;
            sGlassTimer = 0;
        }
        else
        {
            SetGpuReg(REG_OFFSET_BLDY, 16 - (sGlassTimer * 4));
        }
        break;
    case 1: // sacudida: jitter decreciente de las 4 BG
    {
        s32 amp = GLASS_SHAKE_MAX - (sGlassTimer * GLASS_SHAKE_MAX / GLASS_SHAKE_FRAMES);
        s32 dx = (sGlassTimer & 1) ? amp : -amp;
        s32 dy = (sGlassTimer & 2) ? amp : -amp;
        SetGpuReg(REG_OFFSET_BG0HOFS, dx);   SetGpuReg(REG_OFFSET_BG0VOFS, dy);
        SetGpuReg(REG_OFFSET_BG2HOFS, dx);   SetGpuReg(REG_OFFSET_BG2VOFS, dy);
        SetGpuReg(REG_OFFSET_BG3HOFS, dx);   SetGpuReg(REG_OFFSET_BG3VOFS, dy);
        if (sGlassTimer >= GLASS_SHAKE_FRAMES)
        {
            SetGpuReg(REG_OFFSET_BG0HOFS, 0); SetGpuReg(REG_OFFSET_BG0VOFS, 0);
            SetGpuReg(REG_OFFSET_BG2HOFS, 0); SetGpuReg(REG_OFFSET_BG2VOFS, 0);
            SetGpuReg(REG_OFFSET_BG3HOFS, 0); SetGpuReg(REG_OFFSET_BG3VOFS, 0);
            sGlassPhase = 2;
            sGlassTimer = 0;
        }
        break;
    }
    case 2: // aguanta (Task 3 mostrará las grietas aquí)
        if (sGlassTimer >= GLASS_HOLD_FRAMES)
        {
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            sGlassPhase = 3;
            sGlassTimer = 0;
        }
        break;
    case 3: // fundido a negro
        if (!gPaletteFade.active)
        {
            DestroyTask(taskId);
            SetGpuReg(REG_OFFSET_BLDCNT, 0);
            SetMainCallback2(sGlassNextCB);
        }
        break;
    }
}
```
Nota: no se toca BG1 en la sacudida porque es la capa de nubes cuyo `BG1HOFS` lo maneja el scroll del título; jitrear las otras 3 basta para el temblor. Las constantes (frames, amplitud) se afinan en vivo.

- [ ] **Step 4: Enrutar a través del vidrio**

Reemplazar `GoTo(...)` en `PhantomIntro_OnStartPressed` por el vidrio:
```c
void PhantomIntro_OnStartPressed(void)
{
    if (gSaveFileStatus == SAVE_STATUS_OK)
        PhantomGlass_Start(CB2_ContinueSavedGame);  // menú se añade en Task 4
    else
        PhantomGlass_Start(ENTRADA_INTRO);
}
```
Borrar la función `GoTo` (ya no se usa) y su include innecesario si queda alguno.

- [ ] **Step 5: Build**

Run: `make modern -j$(nproc)`
Expected: compila y linka limpio.

- [ ] **Step 6: Smoke**

Run: `./test/smoke.sh`
Expected: `SMOKE: OK`

- [ ] **Step 7: Verificación visual (GIF del vidrio, sin grietas)**

Run:
```bash
PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python -m phantom_dbg boot \
  --frames 260 --press start --gif $CLAUDE_JOB_DIR/tmp/glass_v1.gif --gif-from 30
```
Expected: en el GIF, al pulsar START: destello blanco → temblor de la imagen → breve aguante → fundido a negro → minijuego. Revisar `glass_v1.gif` y ajustar constantes si hace falta.

- [ ] **Step 8: Commit**

```bash
git add src/phantom_intro.c
git commit -m "feat(intro): vidrio impactado (flash+sacudida+fundido, sin grietas)"
```

---

## Task 3: Textura de grietas (script + PNG) y su render en el vidrio

Genera el motivo de impacto y lo dibuja como sprites durante la fase de aguante del vidrio.

**Files:**
- Create: `graphics/phantom_intro/gen.py`
- Create (generado por el script, commiteado): `graphics/phantom_intro/crack.png`
- Modify: `src/phantom_intro.c`

**Interfaces:**
- Consumes: `LoadCompressedSpriteSheet`/`LoadSpriteSheet`, `LoadSpritePalette`, `CreateSprite`, `DestroySprite`, `gSprites`, `struct SpriteTemplate`, `struct OamData` (de `sprite.h`); `INCBIN_U32`/`INCBIN_U16`.
- Produces: sprites de grieta creados en `PhantomGlass_Start` y destruidos al saltar a `nextCB`.

- [ ] **Step 1: Escribir el generador**

`graphics/phantom_intro/gen.py` (genera un motivo de impacto central 64x64, grietas claras radiales sobre fondo transparente índice 0; paleta de 16 colores):
```python
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
```

- [ ] **Step 2: Generar el PNG**

Run: `python3 graphics/phantom_intro/gen.py`
Expected: `crack.png generado` y el archivo `graphics/phantom_intro/crack.png` existe (64x64, indexado).

- [ ] **Step 3: Declarar el asset y el sprite en el módulo**

En `src/phantom_intro.c`, añadir includes:
```c
#include "sprite.h"
#include "decompress.h"
```
Declarar el asset (INCBIN de los .4bpp/.gbapal que gbagfx generará del PNG):
```c
static const u32 sCrackGfx[]  = INCBIN_U32("graphics/phantom_intro/crack.4bpp");
static const u16 sCrackPal[]  = INCBIN_U16("graphics/phantom_intro/crack.gbapal");

#define TAG_CRACK 0x5000

static const struct OamData sOam_Crack = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .priority = 0,
};
static const struct SpriteSheet sSheet_Crack = { sCrackGfx, 64 * 64 / 2, TAG_CRACK };
static const struct SpritePalette sPal_Crack = { sCrackPal, TAG_CRACK };
static const struct SpriteTemplate sTmpl_Crack = {
    .tileTag = TAG_CRACK, .paletteTag = TAG_CRACK, .oam = &sOam_Crack,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
};

static u8 sCrackSpriteId = SPRITE_NONE;
```
Nota: `64*64/2` bytes = tamaño 4bpp de un sprite 64x64. `SPRITE_NONE` es zero-init-safe solo si vale 0; **no** lo es (0xFF). Para respetar `.bss`, no inicializar: declarar `static u8 sCrackSpriteId;` y tratar 0 como "sin crear" usando un bool aparte:
```c
static bool8 sCrackShown;
static u8 sCrackSpriteId;
```

- [ ] **Step 4: Mostrar/ocultar las grietas en el vidrio**

En `PhantomGlass_Start`, cargar el sheet al empezar (antes de crear la task):
```c
    LoadSpriteSheet(&sSheet_Crack);
    LoadSpritePalette(&sPal_Crack);
    sCrackShown = FALSE;
```
En `Task_PhantomGlass`, al entrar en la fase de aguante (`case 1` justo cuando pasa a `sGlassPhase = 2`), crear el sprite centrado:
```c
        if (sGlassTimer >= GLASS_SHAKE_FRAMES)
        {
            // ...reset de offsets (ya existente)...
            sCrackSpriteId = CreateSprite(&sTmpl_Crack, 120, 80, 0);  // centro de 240x160
            sCrackShown = TRUE;
            sGlassPhase = 2;
            sGlassTimer = 0;
        }
```
En `case 3`, antes de `SetMainCallback2`, limpiar:
```c
        if (!gPaletteFade.active)
        {
            if (sCrackShown && sCrackSpriteId < MAX_SPRITES)
                DestroySprite(&gSprites[sCrackSpriteId]);
            FreeSpriteTilesByTag(TAG_CRACK);
            FreeSpritePaletteByTag(TAG_CRACK);
            DestroyTask(taskId);
            SetGpuReg(REG_OFFSET_BLDCNT, 0);
            SetMainCallback2(sGlassNextCB);
        }
```

- [ ] **Step 5: Build**

Run: `make modern -j$(nproc)`
Expected: gbagfx convierte `crack.png`→`crack.4bpp`/`crack.gbapal` automáticamente; compila y linka limpio.

- [ ] **Step 6: Smoke**

Run: `./test/smoke.sh`
Expected: `SMOKE: OK`

- [ ] **Step 7: Verificación visual (GIF con grietas)**

Run:
```bash
PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python -m phantom_dbg boot \
  --frames 260 --press start --gif $CLAUDE_JOB_DIR/tmp/glass_v2.gif --gif-from 30
```
Expected: el GIF muestra el impacto con **grietas visibles** en el centro durante el aguante, antes del fundido. Afinar posición/tamaño del motivo si hace falta (regenerando `crack.png` o moviendo el sprite).

- [ ] **Step 8: Commit**

```bash
git add graphics/phantom_intro/gen.py graphics/phantom_intro/crack.png src/phantom_intro.c
git commit -m "feat(intro): grietas de impacto (script Python + sprite) en el vidrio"
```

---

## Task 4: Menú Nueva/Continúa (overlay por sprites) para el camino con save

Cuando hay save, PRESS START abre un selector de dos opciones sobre el título vivo. Navegar no truena el vidrio; A confirma → vidrio → ruta; B vuelve a PRESS START.

**Files:**
- Modify: `graphics/phantom_intro/gen.py` (añadir generación de `menu.png`)
- Create (generado, commiteado): `graphics/phantom_intro/menu.png`
- Modify: `src/phantom_intro.c`
- Modify: `src/title_screen.c` (ocultar/mostrar el sprite PRESS START según el menú)

**Interfaces:**
- Consumes: `JOY_NEW`, `A_BUTTON`, `B_BUTTON`, `DPAD_UP`, `DPAD_DOWN` (de `main.h`/`gba/`); `PlaySE`, `SE_SELECT`; los sprites como en Task 3.
- Produces: `static void Task_PhantomMenu(u8 taskId);`, y estado de menú que `PhantomIntro_OnStartPressed` arranca.

- [ ] **Step 1: Extender el generador para las etiquetas + cursor**

Añadir al final de `graphics/phantom_intro/gen.py` una función que dibuje `NUEVA PARTIDA`, `CONTINUAR` y un cursor `>` con una fuente pixel 5x7 embebida, en una hoja `menu.png` (indexada, índice 0 transparente). Implementación:
```python
# --- menu.png: dos etiquetas + cursor, fuente 5x7 embebida ---
FONT = {  # 5x7, filas de bits (1 = pixel encendido)
 'A':[0x0E,0x11,0x11,0x1F,0x11,0x11,0x11],'C':[0x0E,0x11,0x10,0x10,0x10,0x11,0x0E],
 'E':[0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F],'I':[0x0E,0x04,0x04,0x04,0x04,0x04,0x0E],
 'N':[0x11,0x19,0x15,0x13,0x11,0x11,0x11],'O':[0x0E,0x11,0x11,0x11,0x11,0x11,0x0E],
 'P':[0x1E,0x11,0x11,0x1E,0x10,0x10,0x10],'R':[0x1E,0x11,0x11,0x1E,0x14,0x12,0x11],
 'T':[0x1F,0x04,0x04,0x04,0x04,0x04,0x04],'U':[0x11,0x11,0x11,0x11,0x11,0x11,0x0E],
 'V':[0x11,0x11,0x11,0x11,0x11,0x0A,0x04],'>':[0x08,0x04,0x02,0x01,0x02,0x04,0x08],
 ' ':[0,0,0,0,0,0,0],
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

# Hoja 128x32: fila0 y=2 "NUEVA PARTIDA", fila1 y=18 "CONTINUAR", cursor en 120..
menu = Image.new("P", (128, 32), 0)
menu.putpalette(palette)  # reutiliza la paleta de crack (1=blanco)
text(menu, "NUEVA PARTIDA", 8, 2, 1)
text(menu, "CONTINUAR",     8, 18, 1)
mp = menu.load()
for row in range(7):  # cursor '>' suelto en x=120,y=2 (tile aparte)
    bits = FONT['>'][row]
    for col in range(5):
        if bits & (1 << (4 - col)):
            mp[120 + col, 2 + row] = 1
menu.save("graphics/phantom_intro/menu.png")
print("menu.png generado")
```
Nota de layout: las etiquetas caben en 128x32 (dos filas de texto); el cursor va suelto en la esquina para poder crear un sprite propio que se reposiciona junto a la opción activa. Posiciones exactas se afinan en vivo.

- [ ] **Step 2: Generar los PNG**

Run: `python3 graphics/phantom_intro/gen.py`
Expected: imprime `crack.png generado` y `menu.png generado`; ambos archivos existen.

- [ ] **Step 3: Declarar los sprites del menú**

En `src/phantom_intro.c` declarar el asset del menú (hoja 128x32 = como sprites 64x32; se parte en 2 sprites de 64x32 para cubrir 128 de ancho, más el cursor). Para simplicidad, usar dos sprites 64x32 para el texto y uno pequeño para el cursor recortando por `tileNum`:
```c
static const u32 sMenuGfx[] = INCBIN_U32("graphics/phantom_intro/menu.4bpp");
static const u16 sMenuPal[] = INCBIN_U16("graphics/phantom_intro/menu.gbapal");
#define TAG_MENU 0x5001
```
Definir un `struct SpriteSheet sSheet_Menu = { sMenuGfx, 128*32/2, TAG_MENU };`, su `SpritePalette`, y templates 64x32 (`SPRITE_SHAPE(64x32)`, `SPRITE_SIZE(64x32)`) para las dos mitades del texto, y un template pequeño (`SPRITE_SHAPE(8x8)` apuntando al tile del cursor) para el cursor. (Los `tileNum`/offsets exactos se ajustan al ver la hoja; el generador fija el layout.)

Estado de menú (zero-init):
```c
static bool8 sMenuOpen;
static u8 sMenuCursor;   // 0 = Nueva, 1 = Continuar
static u8 sMenuSpriteIds[3];
static void Task_PhantomMenu(u8 taskId);
```

- [ ] **Step 4: Abrir el menú desde OnStartPressed (camino con save)**

```c
void PhantomIntro_OnStartPressed(void)
{
    if (sMenuOpen)
        return;
    if (gSaveFileStatus == SAVE_STATUS_OK)
    {
        // Cargar sprites del menú, crear las 2 etiquetas + cursor, abrir el selector.
        LoadSpriteSheet(&sSheet_Menu);
        LoadSpritePalette(&sPal_Menu);
        // crear sprites (posiciones afinadas en vivo), guardarlos en sMenuSpriteIds
        sMenuCursor = 0;
        sMenuOpen = TRUE;
        TitleScreen_SetPressStartVisible(FALSE);   // ocultar "PRESS START"
        CreateTask(Task_PhantomMenu, 1);
    }
    else
    {
        PhantomGlass_Start(ENTRADA_INTRO);
    }
}
```

- [ ] **Step 5: Implementar la task del menú (navegación + confirmación)**

```c
static void CloseMenu(void)
{
    u32 i;
    for (i = 0; i < 3; i++)
        if (sMenuSpriteIds[i] < MAX_SPRITES)
            DestroySprite(&gSprites[sMenuSpriteIds[i]]);
    FreeSpriteTilesByTag(TAG_MENU);
    FreeSpritePaletteByTag(TAG_MENU);
    sMenuOpen = FALSE;
}

static void Task_PhantomMenu(u8 taskId)
{
    if (JOY_NEW(DPAD_UP) || JOY_NEW(DPAD_DOWN))
    {
        sMenuCursor ^= 1;
        PlaySE(SE_SELECT);
        // reposicionar el sprite del cursor junto a la opción activa (Y según fila)
    }
    else if (JOY_NEW(A_BUTTON))
    {
        MainCallback next = (sMenuCursor == 0) ? ENTRADA_INTRO : CB2_ContinueSavedGame;
        CloseMenu();
        DestroyTask(taskId);
        PhantomGlass_Start(next);   // el vidrio SOLO aquí, al confirmar
    }
    else if (JOY_NEW(B_BUTTON))
    {
        CloseMenu();
        DestroyTask(taskId);
        TitleScreen_SetPressStartVisible(TRUE);   // volver a PRESS START
    }
}
```

- [ ] **Step 6: Exponer el toggle del PRESS START desde el título**

En `src/title_screen.c`, añadir (y declarar en `include/title_screen.h`):
```c
void TitleScreen_SetPressStartVisible(bool8 visible)
{
    u8 i;
    for (i = 0; i < NUM_PRESS_START_FRAMES; i++)
    {
        // los sprites de PRESS START se crean en CreatePressStartSprites; guardarlos
        // en un array estático sPressStartSpriteIds[NUM_PRESS_START_FRAMES] para poder
        // togglear su .invisible aquí.
    }
}
```
Para ello, modificar `CreatePressStartSprites` para guardar los `spriteId` en un `static u8 sPressStartSpriteIds[NUM_PRESS_START_FRAMES];` y en `SetPressStartVisible` poner `gSprites[id].invisible = !visible` (y parar su parpadeo cuando estén ocultos). Declarar el prototipo en `include/title_screen.h`.

- [ ] **Step 7: Build**

Run: `make modern -j$(nproc)`
Expected: compila y linka limpio (menu.png convertido por gbagfx).

- [ ] **Step 8: Smoke**

Run: `./test/smoke.sh`
Expected: `SMOKE: OK`

- [ ] **Step 9: Verificación visual (necesita un save en la flash)**

Crear un save primero (arrancar la ROM debug que llega al overworld, dormir en la cama que guarda, y reusar ese `.sav`), luego arrancar la release con ese save y verificar el menú:
```bash
# 1) generar un save con la ROM debug + script de cama (guarda)
PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python -m phantom_dbg boot \
  --rom pokeemerald_modern_debug.gba --frames 600 --save-state $CLAUDE_JOB_DIR/tmp/phantom.sav \
  --script bed-save   # (o la secuencia de inputs equivalente que dispara Common_EventScript_SaveGame)
# 2) arrancar la release CON ese save y capturar el menú + ambas rutas
PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python -m phantom_dbg boot \
  --sav $CLAUDE_JOB_DIR/tmp/phantom.sav --frames 300 --press start \
  --gif $CLAUDE_JOB_DIR/tmp/menu.gif --gif-from 20
```
Expected: al pulsar START con save, aparece `> NUEVA PARTIDA / CONTINUAR`; ↑↓ mueve el cursor sin trueno de vidrio; A en Continuar → vidrio → carga la partida; B cierra y vuelve a PRESS START. (Si el harness no tiene flags `--sav`/`--script`, usar la vía de inyectar `gSaveFileStatus = SAVE_STATUS_OK` por memoria para ver el menú, y validar la carga real por separado.)

- [ ] **Step 10: Commit**

```bash
git add graphics/phantom_intro/gen.py graphics/phantom_intro/menu.png src/phantom_intro.c src/title_screen.c include/title_screen.h
git commit -m "feat(intro): menú Nueva/Continuar (overlay por sprites) con enrutado por save"
```

---

## Task 5: Pulido final y verificación de release

Afinado visual (constantes del vidrio, posición del menú/cursor, elección final de SFX) y verificación de que los tres builds siguen sanos.

**Files:**
- Modify: `src/phantom_intro.c` (constantes afinadas)
- Modify: `graphics/phantom_intro/gen.py` + PNGs regenerados (si se ajusta el look)

- [ ] **Step 1: Iterar el look con GIFs**

Repetir las capturas de Task 2/3/4 ajustando `GLASS_SHAKE_FRAMES`, `GLASS_SHAKE_MAX`, `GLASS_HOLD_FRAMES`, el SFX (`SE_ICE_BREAK` vs `SE_ICE_CRACK` vs combinación), y las posiciones del menú/cursor, hasta que el resultado convenza (revisión conjunta con Valentin).

- [ ] **Step 2: Build release + los otros dos builds**

Run:
```bash
make modern -j$(nproc)
make PHANTOM_TEST=1 modern -j$(nproc)
make PHANTOM_DEBUG_BOOT=1 modern -j$(nproc)
```
Expected: los tres compilan y linkan limpio.

- [ ] **Step 3: Smoke**

Run: `./test/smoke.sh`
Expected: `SMOKE: OK`

- [ ] **Step 4: Verificación visual final (las tres rutas)**

- Sin save: START → vidrio → minijuego.
- Con save + Nueva: START → menú → Nueva → vidrio → minijuego.
- Con save + Continuar: START → menú → Continuar → vidrio → carga.
Expected: GIFs correctos de las tres; el vidrio nunca se dispara al navegar el menú.

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "polish(intro): afinado del vidrio/menú y verificación de los 3 builds"
```

---

## Self-Review (cobertura del spec)

- **Título con PRESS START** → intacto (Task 1 no lo toca; Task 4 solo togglea visibilidad).
- **Sin save: PRESS START → vidrio → nueva partida** → Task 1 (ruta) + Task 2/3 (vidrio).
- **Con save: PRESS START → menú Nueva/Continúa → vidrio → ruta** → Task 4.
- **Vidrio solo al confirmar, nunca al navegar** → Task 4, Step 5 (el vidrio se llama solo en la rama `A_BUTTON`).
- **Efecto de vidrio impactado (flash+sacudida+grietas+aguante+fundido)** → Task 2 + Task 3.
- **Grietas por script, sin arte a mano** → Task 3 (`gen.py`).
- **Detección de save vía `gSaveFileStatus == SAVE_STATUS_OK`; edge → camino sin-save** → Task 1/4.
- **`entradaIntro = CB2_InitMinigameShip`** → constante `ENTRADA_INTRO`, único punto a cambiar en Pieza 2.
- **No tocar mareo/desaturación/sandbox/harness** → solo se crean/editan los archivos listados.
- **Verificación por build + smoke + visual** → cada tarea.

Riesgos conocidos anotados para el implementador: (a) VRAM de OBJ del título es ajustada — el motivo de grietas es central (pocos sprites) para no agotarla; (b) el toggle de PRESS START requiere guardar los spriteIds en el título; (c) el testing del camino "con save" depende de que el harness pueda cargar un `.sav` o inyectar `gSaveFileStatus` (fallback documentado en Task 4, Step 9).
