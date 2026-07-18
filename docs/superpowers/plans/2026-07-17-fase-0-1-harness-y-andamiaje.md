# Fase 0-1: Harness de testing + andamiaje de flujo — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dejar el repo con un build reproducible de `make modern`, un loop de test automatizado (smoke test headless que prueba build+boot+estado), y el esqueleto de flujo del hack (sin intro de Birch, protagonista fijo, `VAR_PHANTOM_TIME`, guardado diegético, sin encuentros salvajes) — todo verificado por el harness.

**Architecture:** Se instala el toolchain ARM (Fase 0) y se compila la ROM. Se añade un módulo de test in-ROM (`src/phantom_test.c`) que, bajo el flag de compilación `PHANTOM_TEST`, ejecuta una secuencia de aserciones emitiendo checkpoints por el canal de log de mGBA (`DebugPrintf`) y termina con un `svc` que `mgba-rom-test` traduce a exit code. Cada cambio de flujo posterior se verifica añadiendo una aserción a esa secuencia y/o con una lectura puntual por GDB. Nada depende de agbcc; todo compila con gcc (`make modern`).

**Tech Stack:** pokeemerald (decomp GBA, C gnu89/C11), `arm-none-eabi-gcc`, GNU make, mGBA (`mgba-qt` + `mgba-rom-test`), GDB con soporte ARM, bash.

## Global Constraints

- **Build objetivo: `make modern`** (gcc). Nunca depender de agbcc; el árbol no lo tiene (`tools/agbcc` ausente).
- **No romper el build de release:** todo el código de test vive detrás del flag `PHANTOM_TEST`; sin ese flag, cero cambios de comportamiento y `NDEBUG` sigue definido.
- **Commits sin trailer `Co-Authored-By`** y sin línea `Claude-Session`. Mensajes limpios.
- **No `git push`** a ningún remoto (el remoto principal `git.nozz.skin` aún no está configurado; GitHub es para más adelante). Solo commits locales.
- **Rama de trabajo:** `design/phantom-spec` (ya creada, contiene los docs de diseño) o una rama nueva desde ella; nunca commitear directo a `master`.
- **Idioma:** identificadores en inglés (consistencia con el decomp); textos de juego y comentarios de dominio en español.
- **Comandos `make` en scripts/tool-calls:** usar `make -j$(nproc)` explícito (en shells no interactivos el alias `make -j12` no aplica).
- **Entorno:** CachyOS (Arch), pacman. Python 3.14 del sistema; `uv` disponible para un 3.12 aislado si hace falta (Fase 2, fuera de este plan).

---

## File Structure

- `src/phantom_test.c` (nuevo) — módulo de test in-ROM: registro de aserciones, checkpoints, salida por `svc`. Solo se compila con `PHANTOM_TEST`.
- `include/phantom_test.h` (nuevo) — API del harness (`PHANTOM_CHECKPOINT`, `PHANTOM_ASSERT`, `PhantomTest_Run`, `PhantomTest_Finish`).
- `include/constants/phantom.h` (nuevo) — constantes del hack: `VAR_PHANTOM_TIME` (alias de un `VAR_UNUSED_*`), estados `PHANTOM_TIME_*`.
- `test/smoke.sh` (nuevo) — runner headless: compila (si hace falta), corre `mgba-rom-test`, parsea checkpoints y devuelve exit code.
- `test/gdb-read.sh` (nuevo) — helper para leer un símbolo/expresión vía el stub GDB de mGBA.
- `include/config.h` (modificar) — guardar `#define NDEBUG` detrás de `#ifndef PHANTOM_TEST`.
- `src/main.c` (modificar) — hook en `AgbMain`: bajo `PHANTOM_TEST`, llamar a `PhantomTest_Run()` tras `MgbaOpen()`.
- `src/main_menu.c` / `src/new_game.c` (modificar) — recorte de intro de Birch + protagonista fijo.
- `src/new_game.c` (modificar) — init de `VAR_PHANTOM_TIME` y desactivación global de encuentros.
- `src/start_menu.c` (modificar) — quitar la entrada SAVE del menú.
- `Makefile` (modificar) — variable `PHANTOM_TEST` que añade `-DPHANTOM_TEST` a `CPPFLAGS` y compila `phantom_test.c`.

---

## Task 1: Toolchain ARM + primer build modern (DESBLOQUEO)

> Esta tarea contiene el único paso que requiere `sudo`: **lo ejecuta Valentin** (en el prompt, con `! sudo ...`). El resto es agent-executable. Hasta que esta tarea pase, ninguna otra se puede verificar.

**Files:**
- Ninguno versionado. Produce artefactos ignorados: `pokeemerald_modern.gba`, `.elf`, `.map`.

**Interfaces:**
- Produces: un build funcional de `make modern` y el binario `pokeemerald_modern.elf` con DWARF (para GDB en tareas posteriores).

- [ ] **Step 1 (Valentin, en el prompt): instalar el toolchain**

Ejecuta en la sesión:
```
! sudo pacman -S --needed arm-none-eabi-gcc arm-none-eabi-binutils arm-none-eabi-newlib arm-none-eabi-gdb mgba-qt make gcc
```
(Si algún nombre de paquete no existe en el repo, avísame y ajusto — en Arch/CachyOS los ARM viven en `extra`.)

- [ ] **Step 2: verificar el toolchain**

Run:
```bash
arm-none-eabi-gcc --version && arm-none-eabi-as --version | head -1 && mgba-qt --version 2>&1 | head -1
```
Expected: versiones impresas, sin "command not found".

- [ ] **Step 3: build de los host-tools**

Run: `make tools -j$(nproc)`
Expected: compila `gbagfx`, `preproc`, `scaninc`, etc. sin error (usan g++ del sistema). Verifica: `ls tools/preproc/preproc tools/gbagfx/gbagfx`

- [ ] **Step 4: primer build modern con debug info**

Run: `make DINFO=1 modern -j$(nproc)`
Expected: termina con `pokeemerald_modern.gba` generado. Verifica:
```bash
ls -la pokeemerald_modern.gba pokeemerald_modern.elf pokeemerald_modern.map
```
Los tres deben existir. (Este primer build es lento; los siguientes son incrementales.)

- [ ] **Step 5 (Valentin o agente con display): boot visual**

Run: `mgba-qt pokeemerald_modern.gba`
Expected: arranca, muestra copyright → tu title screen custom. Cierra la ventana tras confirmar. (Si corre headless, salta este paso; el smoke test de la Task 3 lo cubre.)

- [ ] **Step 6: confirmar que los artefactos están git-ignored**

Run: `git status --short | grep -E 'pokeemerald_modern|build/' || echo "OK: artefactos ignorados"`
Expected: `OK: artefactos ignorados` (el `.gitignore` ya cubre `*.gba`/`build/`). Si aparecieran, NO commitearlos.

*(Sin commit: esta tarea no cambia archivos versionados.)*

---

## Task 2: Obtener `mgba-rom-test`

**Files:**
- Create: `tools/mgba/mgba-rom-test` (binario, git-ignored) o documentar ruta del sistema.
- Modify: `.gitignore` (añadir `tools/mgba/` si se guarda el binario ahí).

**Interfaces:**
- Produces: un ejecutable `mgba-rom-test` invocable que, dado un ROM, corre headless y sale con el valor de un registro cuando el ROM ejecuta un `svc` reservado. Lo consume `test/smoke.sh` (Task 3).

- [ ] **Step 1: crear el directorio e intentar el binario precompilado de pokeemerald-expansion**

`mgba-rom-test` fue eliminado del master de mGBA; la vía práctica es el binario que mantiene pokeemerald-expansion, o compilarlo del tag 0.10.5.

Run:
```bash
mkdir -p tools/mgba
# Intento A: descargar el prebuilt de expansion (verificar URL vigente antes de confiar)
curl -fLo tools/mgba/mgba-rom-test \
  https://raw.githubusercontent.com/rh-hideout/pokeemerald-expansion/master/tools/mgba/mgba-rom-test \
  && chmod +x tools/mgba/mgba-rom-test \
  && echo "descargado" || echo "FALLO_DESCARGA"
```
Expected: `descargado`. Si sale `FALLO_DESCARGA` o el binario no ejecuta (arquitectura/glibc incompatible), ir al Step 2.

- [ ] **Step 2 (fallback): compilar `mgba-rom-test` del tag 0.10.5**

Run:
```bash
git clone --depth 1 --branch 0.10.5 https://github.com/mgba-emu/mgba.git /tmp/mgba-src
cmake -S /tmp/mgba-src -B /tmp/mgba-build -DBUILD_ROM_TEST=ON -DBUILD_QT=OFF -DBUILD_SDL=OFF
cmake --build /tmp/mgba-build --target mgba-rom-test -j$(nproc)
cp /tmp/mgba-build/mgba-rom-test tools/mgba/mgba-rom-test
```
Expected: `tools/mgba/mgba-rom-test` existe y es ejecutable. (Requiere `cmake` + deps; instalar con pacman si falta.)

- [ ] **Step 3: verificar que ejecuta**

Run: `tools/mgba/mgba-rom-test --help 2>&1 | head -5`
Expected: imprime uso/flags (incluye `-S`/`--swi` y `-R`/`--register` para la señal de salida). Sin "command not found" ni error de librería.

- [ ] **Step 4: ignorar el binario y commitear el `.gitignore`**

Run:
```bash
grep -q '^tools/mgba/$' .gitignore || printf 'tools/mgba/\n' >> .gitignore
git add .gitignore
git commit -m "build: ignorar tools/mgba (binario mgba-rom-test local)"
```
Expected: commit creado, sin el binario dentro.

---

## Task 3: Harness de smoke test in-ROM + runner

**Files:**
- Create: `include/phantom_test.h`
- Create: `src/phantom_test.c`
- Create: `test/smoke.sh`
- Modify: `include/config.h:9` (guard de `NDEBUG`)
- Modify: `src/main.c` (hook en `AgbMain`, tras `MgbaOpen()` en la línea ~126)
- Modify: `Makefile` (flag `PHANTOM_TEST`)

**Interfaces:**
- Produces:
  - `void PhantomTest_Run(void);` — corre la secuencia de tests y no retorna (termina con `svc`).
  - `void PhantomTest_Finish(u8 exitCode);` — `mov r0, exitCode; svc 0x27` (mgba-rom-test sale con ese código).
  - `PHANTOM_CHECKPOINT(name)` — macro → `DebugPrintf(":P %s", name)`.
  - `PHANTOM_ASSERT(cond, name)` — macro: si `cond`, checkpoint `PASS name`; si no, checkpoint `FAIL name` y marca fallo global.
  - Convención de exit: `0` = todos los asserts pasaron; `1` = hubo al menos un FAIL.
- Consumes: `DebugPrintf`/`MgbaPrintf` (`include/gba/isagbprint.h`), `MgbaOpen` ya llamado en `AgbMain`.

- [ ] **Step 1: guardar `NDEBUG` detrás de `PHANTOM_TEST` en `include/config.h`**

Localiza (config.h:9):
```c
#define NDEBUG
```
Reemplázalo por:
```c
// PHANTOM_TEST activa el canal de log de mGBA (DebugPrintf) para el harness de test.
#ifndef PHANTOM_TEST
#define NDEBUG
#endif
```
(Así, en release `NDEBUG` sigue definido y `DebugPrintf` es no-op; en builds de test se habilita `MgbaPrintf`.)

- [ ] **Step 2: crear `include/phantom_test.h`**

```c
#ifndef GUARD_PHANTOM_TEST_H
#define GUARD_PHANTOM_TEST_H

// Harness de smoke test in-ROM. Solo activo con -DPHANTOM_TEST.
// Corre bajo mgba-rom-test: emite checkpoints por el log de mGBA y
// termina con `svc 0x27` dejando el exit code en r0 (0 = OK, 1 = fallo).

#include "gba/isagbprint.h"

#define PHANTOM_CHECKPOINT(name) DebugPrintf(":P %s", name)

extern u8 gPhantomTestFailed;

#define PHANTOM_ASSERT(cond, name)                    \
    do {                                              \
        if (cond) {                                   \
            DebugPrintf(":P PASS %s", name);          \
        } else {                                      \
            DebugPrintf(":P FAIL %s", name);          \
            gPhantomTestFailed = 1;                   \
        }                                             \
    } while (0)

void PhantomTest_Run(void);      // corre la secuencia; no retorna
void PhantomTest_Finish(u8 exitCode);

#endif // GUARD_PHANTOM_TEST_H
```

- [ ] **Step 3: crear `src/phantom_test.c` con el primer test "boot-ok"**

Todo el cuerpo va envuelto en `#ifdef PHANTOM_TEST` para que en release el TU quede vacío (cero código muerto, cero `svc` en la ROM final):
```c
#include "global.h"
#include "phantom_test.h"

#ifdef PHANTOM_TEST

u8 gPhantomTestFailed = 0;

// Termina la emulación: deja exitCode en r0 y ejecuta el svc que
// mgba-rom-test observa (invocado con -S 0x27 -R r0).
void PhantomTest_Finish(u8 exitCode)
{
    asm volatile("mov r0, %0\n\t"
                 "svc 0x27\n"
                 :: "r"(exitCode) : "r0", "memory");
    while (1) {} // por si el svc no corta la ejecución
}

// Test 0: el ROM compila, arranca y el código se ejecuta.
static void Test_BootOk(void)
{
    PHANTOM_CHECKPOINT("boot");
    PHANTOM_ASSERT(TRUE, "boot-reached");
}

void PhantomTest_Run(void)
{
    PHANTOM_CHECKPOINT("suite-start");
    Test_BootOk();
    PHANTOM_CHECKPOINT("suite-end");
    PhantomTest_Finish(gPhantomTestFailed);
}

#endif // PHANTOM_TEST
```
(A partir de aquí, cada tarea que añada un test lo hace **dentro** de este `#ifdef` y lo registra en `PhantomTest_Run`.)

- [ ] **Step 4: hook en `src/main.c` (`AgbMain`, tras `MgbaOpen()`)**

Localiza en `AgbMain` (main.c:126):
```c
    (void) MgbaOpen();
```
Inmediatamente después, añade:
```c
#ifdef PHANTOM_TEST
    PhantomTest_Run(); // no retorna: corre la suite y termina la emulación
#endif
```
Y añade el include cerca de los demás `#include` al inicio de `src/main.c`:
```c
#ifdef PHANTOM_TEST
#include "phantom_test.h"
#endif
```

- [ ] **Step 5: soporte de `PHANTOM_TEST` en el `Makefile`**

Tras la definición de `CPPFLAGS` (Makefile:116, `CPPFLAGS := ...`), añade:
```make
# Build de test: habilita el harness in-ROM (src/phantom_test.c) y el log de mGBA.
PHANTOM_TEST ?= 0
ifeq ($(PHANTOM_TEST),1)
  CPPFLAGS += -DPHANTOM_TEST
endif
```
`src/phantom_test.c` se recoge automáticamente por el wildcard `C_SRCS_IN` (Makefile:198); bajo release, su cuerpo referencia el harness pero nadie lo llama y `gPhantomTestFailed` es dato inerte — compila sin efecto. (Si el linker se quejara de código muerto, envolver el cuerpo de `phantom_test.c` en `#ifdef PHANTOM_TEST`.)

- [ ] **Step 6: crear el runner `test/smoke.sh`**

```bash
#!/usr/bin/env bash
# Smoke test headless: build de test + mgba-rom-test + parseo de checkpoints.
set -euo pipefail
cd "$(dirname "$0")/.."

ROM=pokeemerald_modern.gba
ROMTEST=tools/mgba/mgba-rom-test

echo ">> build PHANTOM_TEST=1"
make PHANTOM_TEST=1 DINFO=1 modern -j"$(nproc)"

echo ">> run $ROMTEST"
LOG=$(mktemp)
set +e
stdbuf -oL "$ROMTEST" -S 0x27 -R r0 -l 15 "$ROM" | tee "$LOG"
EXIT=${PIPESTATUS[0]}
set -e

echo ">> checkpoints:"
grep ':P' "$LOG" || { echo "!! sin checkpoints (¿NDEBUG/log?)"; rm -f "$LOG"; exit 2; }

if grep -q ':P FAIL' "$LOG"; then echo "SMOKE: FAIL"; rm -f "$LOG"; exit 1; fi
if [ "$EXIT" -ne 0 ]; then echo "SMOKE: exit=$EXIT (!=0)"; rm -f "$LOG"; exit "$EXIT"; fi
echo "SMOKE: OK"
rm -f "$LOG"
```
Run: `chmod +x test/smoke.sh`

- [ ] **Step 7: correr el smoke test (debe PASAR)**

Run: `./test/smoke.sh`
Expected: build OK, líneas `GBA Debug: :P boot`, `:P PASS boot-reached`, `:P suite-end`, y al final `SMOKE: OK` con exit 0.
(Si no aparecen checkpoints: revisar que el build llevó `PHANTOM_TEST=1` y que `config.h` quedó con el guard del Step 1. Si `mgba-rom-test` no reconoce `-S/-R`, ajustar los flags según `--help` de la Task 2.)

- [ ] **Step 8: verificar que el build de RELEASE sigue limpio**

Run: `make modern -j$(nproc)`
Expected: compila sin error y sin el flag; `DebugPrintf` queda no-op (NDEBUG activo). Prueba de no-regresión.

- [ ] **Step 9: commit**

```bash
git add include/phantom_test.h src/phantom_test.c test/smoke.sh include/config.h src/main.c Makefile
git commit -m "test: harness de smoke test in-ROM + runner headless (mgba-rom-test)"
```

---

## Task 4: Recorte de intro de Birch + protagonista fijo

> Objetivo: New Game salta la charla de Birch y la selección de nombre/género; el Forastero es fijo (masculino, sin nombre editable). El mapa inicial queda el de vanilla como placeholder (las zonas de la isla llegan en un plan posterior).

**Files:**
- Modify: `src/main_menu.c` (flujo de New Game → saltar la intro de Birch; hooks en ~1776-1786 según auditoría)
- Modify: `src/new_game.c` (`NewGameInitData`, ~149: fijar género/nombre)
- Test: aserción nueva en `src/phantom_test.c`

**Interfaces:**
- Consumes: harness de Task 3 (`PHANTOM_ASSERT`, secuencia en `PhantomTest_Run`).
- Produces: tras New Game, `gSaveBlock2Ptr->playerGender == MALE` y el flujo no pasa por `CB2_InitBirchsBattle`/naming. **Además introduce el setup de save-blocks en `PhantomTest_Run`** (`SetSaveBlocksPointers(...)`) del que dependen los tests de estado de las Tasks 5 y 7 — por eso esos tests pueden llamar `NewGameInitData()` sin preparar los punteros ellos mismos.

- [ ] **Step 1: confirmar el flujo real de New Game (ya verificado — solo re-confirmar líneas)**

Símbolos verificados (jul 2026) contra el código; re-confirma que siguen ahí antes de editar:
```bash
sed -n '1056,1069p' src/main_menu.c   # switch(action): case ACTION_NEW_GAME (~1058-1063) lanza Task_NewGameBirchSpeech_Init
sed -n '1532,1548p' src/overworld.c   # CB2_NewGame: llama NewGameInitData() y entra al field vía ExecuteTruckSequence
```
Hechos clave:
- El despacho de New Game está en `src/main_menu.c:1058-1063` (`case ACTION_NEW_GAME: default:`), que hoy hace `gTasks[taskId].func = Task_NewGameBirchSpeech_Init;` (arranca la charla de Birch + género + naming).
- La rama `ACTION_CONTINUE` (justo debajo, ~1064-1069) es el molde a imitar: pone el pltt a negro, `SetMainCallback2(CB2_ContinueSavedGame)` y `DestroyTask(taskId)`.
- `CB2_NewGame` (`src/overworld.c:1532`, declarado `include/overworld.h:134`) ya llama `NewGameInitData()` y entra al overworld — es el destino correcto para saltar Birch. El `ExecuteTruckSequence` (intro del camión en Littleroot) queda como placeholder hasta que existan los mapas de la isla.

- [ ] **Step 2: escribir la aserción que FALLA (protagonista fijo)**

En `src/phantom_test.c` (dentro del `#ifdef PHANTOM_TEST`), añade includes arriba del archivo:
```c
#include "load_save.h"
#include "new_game.h"
#include "main.h"
#include "constants/pokemon.h" // MALE/FEMALE
```
El harness corre en `AgbMain` antes de que existan los save blocks, así que centraliza su preparación en `PhantomTest_Run` (una sola vez, antes de los tests de estado). Modifica `PhantomTest_Run` para que quede:
```c
void PhantomTest_Run(void)
{
    PHANTOM_CHECKPOINT("suite-start");
    Test_BootOk();
    // Setup compartido de estado para los tests de new game (Tasks 4/5/7).
    SetSaveBlocksPointers(GetSaveBlocksPointersBaseOffset());
    Test_NewGameProtagonist();
    PHANTOM_CHECKPOINT("suite-end");
    PhantomTest_Finish(gPhantomTestFailed);
}
```
Y el test:
```c
static void Test_NewGameProtagonist(void)
{
    NewGameInitData();
    PHANTOM_ASSERT(gSaveBlock2Ptr->playerGender == MALE, "protagonist-male");
}
```

- [ ] **Step 3: correr el smoke test y verlo FALLAR**

Run: `./test/smoke.sh`
Expected: hoy `NewGameInitData` no fija el género → o bien `FAIL protagonist-male`, o el valor por defecto no es MALE. Confirma que la aserción está realmente ejercitándose (aparece `:P ... protagonist-male`).

- [ ] **Step 4: fijar género/nombre en `NewGameInitData` (`src/new_game.c`)**

Dentro de `NewGameInitData()` (def. en `new_game.c:149`, antes del `WarpToTruck()` de `:195`), fuerza el protagonista:
```c
    // Pokémon Phantom: el Forastero es fijo (masculino, sin nombre editable).
    gSaveBlock2Ptr->playerGender = MALE;
    StringCopy(gSaveBlock2Ptr->playerName, gText_PhantomPlayerName);
```
El campo es `playerName[PLAYER_NAME_LENGTH + 1]` con **`PLAYER_NAME_LENGTH == 7`**, así que el nombre interno debe caber en ≤7 caracteres (el Forastero es "sin nombre"; este valor casi nunca se muestra). Placeholder que cabe — ajustable cuando decidamos si/cómo se muestra:
```c
static const u8 gText_PhantomPlayerName[] = _("?");
```
Añádelo cerca de otros `static const u8 gText_...` en `src/new_game.c`. Requiere `#include "string_util.h"` (para `StringCopy`) y `#include "constants/global.h"` (para `MALE`) si no están ya en el archivo. `MALE` = 0 (`include/constants/global.h:113`).

- [ ] **Step 5: saltar la intro de Birch en `src/main_menu.c` (líneas 1058-1063)**

En el `switch (action)`, reemplaza el cuerpo del `case ACTION_NEW_GAME: default:` (que hoy hace `gTasks[taskId].func = Task_NewGameBirchSpeech_Init;`) por el mismo patrón que usa `ACTION_CONTINUE`, pero apuntando a `CB2_NewGame`:
```c
            case ACTION_NEW_GAME:
            default:
                gPlttBufferUnfaded[0] = RGB_BLACK;
                gPlttBufferFaded[0] = RGB_BLACK;
                // Pokémon Phantom: sin charla de Birch ni selección de nombre/género.
                SetMainCallback2(CB2_NewGame);
                DestroyTask(taskId);
                break;
```
`CB2_NewGame` (`overworld.c:1532`) ya llama `NewGameInitData()` (donde viven nuestras inits de las Tasks 4/5/7) y entra al field. Verifica que `include/overworld.h` esté incluido en `main_menu.c` (lo está en vanilla); si no, añádelo.

- [ ] **Step 6: correr el smoke test (debe PASAR) + boot smoke intacto**

Run: `./test/smoke.sh`
Expected: `:P PASS protagonist-male`, `:P PASS boot-reached`, `SMOKE: OK`.

- [ ] **Step 7: verificación manual del recorte (con display) o por GDB**

Con display:
```bash
make modern -j$(nproc) && mgba-qt pokeemerald_modern.gba
```
New Game debe ir del menú principal al juego sin pantalla de nombre ni charla de Birch.
Sin display (headless), verifica por GDB que no se entra a la intro de Birch — ver `test/gdb-read.sh` (Task 5); pon breakpoint en el símbolo de la intro y confirma que no se alcanza. (Anota el resultado en el commit.)

- [ ] **Step 8: commit**

```bash
git add src/new_game.c src/main_menu.c src/strings.c src/phantom_test.c
git commit -m "feat: recortar intro de Birch y fijar el protagonista (Forastero)"
```

---

## Task 5: `VAR_PHANTOM_TIME` + estados del reloj

> Define la variable de estado-mundo persistente y la inicializa en New Game. Base del sistema de días. También entrega `test/gdb-read.sh`, usado por otras tareas.

**Files:**
- Create: `include/constants/phantom.h`
- Create: `test/gdb-read.sh`
- Modify: `src/new_game.c` (`NewGameInitData`: `VarSet(VAR_PHANTOM_TIME, PHANTOM_TIME_PROLOGUE)`)
- Test: aserción en `src/phantom_test.c`

**Interfaces:**
- Consumes: `VarSet`/`VarGet` (`include/event_data.h`); un `VAR_UNUSED_*` persistente (rango 0x404E–0x40FF, `include/constants/vars.h`); el setup de save-blocks que `PhantomTest_Run` ya hace (Task 4) — este test corre después de él y solo llama `NewGameInitData()`.
- Produces: `VAR_PHANTOM_TIME` y las constantes `PHANTOM_TIME_PROLOGUE..PHANTOM_TIME_DAWN`. Lo consumen todos los sistemas por-día en planes posteriores.

- [ ] **Step 1: confirmar el `VAR_UNUSED` a usar (ya verificado)**

Run: `grep -n "VAR_UNUSED_0x404E" include/constants/vars.h`
Expected: `include/constants/vars.h:98: #define VAR_UNUSED_0x404E 0x404E // Unused Var` — es el primer var persistente libre del rango (0x404E–0x40FF), verificado. Se usa como base de `VAR_PHANTOM_TIME`.

- [ ] **Step 2: crear `include/constants/phantom.h`**

```c
#ifndef GUARD_CONSTANTS_PHANTOM_H
#define GUARD_CONSTANTS_PHANTOM_H

// Estado-mundo de Pokémon Phantom. Ver docs/superpowers/specs/2026-07-17-pokemon-phantom-design.md

// Reutiliza el primer VAR_UNUSED persistente libre (verificado: vars.h:98).
#define VAR_PHANTOM_TIME   VAR_UNUSED_0x404E

// Franjas narrativas (el tiempo avanza solo al dormir).
#define PHANTOM_TIME_PROLOGUE  0
#define PHANTOM_TIME_DAY1      1
#define PHANTOM_TIME_DAY2      2
#define PHANTOM_TIME_DAY3      3
#define PHANTOM_TIME_DAWN      4

#endif // GUARD_CONSTANTS_PHANTOM_H
```

- [ ] **Step 3: escribir la aserción que FALLA**

En `src/phantom_test.c`, añade el include:
```c
#include "event_data.h"
#include "constants/phantom.h"
```
Y el test (reutiliza el setup de save-blocks que `PhantomTest_Run` ya hace antes de los tests de estado):
```c
static void Test_PhantomTimeInit(void)
{
    NewGameInitData();
    PHANTOM_ASSERT(VarGet(VAR_PHANTOM_TIME) == PHANTOM_TIME_PROLOGUE, "phantom-time-prologue");
}
```
Regístralo en `PhantomTest_Run` entre `Test_NewGameProtagonist()` y `suite-end`.

- [ ] **Step 4: correr el smoke test y verlo FALLAR**

Run: `./test/smoke.sh`
Expected: `:P FAIL phantom-time-prologue` (la var arranca en 0 por el clear del save, pero el test valida que la init la fija explícitamente; si pasa por casualidad al ser 0, cámbiala en el Step 5 y verás la relación causal). Confirma que el checkpoint aparece.

- [ ] **Step 5: inicializar la variable en `NewGameInitData` (`src/new_game.c`)**

Añade (junto a la fijación de protagonista de la Task 4):
```c
    VarSet(VAR_PHANTOM_TIME, PHANTOM_TIME_PROLOGUE);
```
Include arriba de `src/new_game.c` si no está:
```c
#include "constants/phantom.h"
```

- [ ] **Step 6: correr el smoke test (debe PASAR)**

Run: `./test/smoke.sh`
Expected: `:P PASS phantom-time-prologue`, `SMOKE: OK`.

- [ ] **Step 7: crear `test/gdb-read.sh` (helper reutilizable)**

```bash
#!/usr/bin/env bash
# Lee una expresión C del ROM en marcha vía el stub GDB de mGBA.
# Uso: test/gdb-read.sh 'VarGet(VAR_PHANTOM_TIME)'  (con mgba-qt ... -g corriendo)
# Requiere: mgba-qt pokeemerald_modern.gba -g &   (stub en :2345)
set -euo pipefail
cd "$(dirname "$0")/.."
EXPR="${1:?uso: gdb-read.sh '<expr C>'}"
gdb -q -batch pokeemerald_modern.elf \
  -ex 'target remote localhost:2345' \
  -ex 'continue &' -ex 'interrupt' \
  -ex "print $EXPR" \
  -ex 'detach' 2>&1 | grep -A1 "\$"
```
Run: `chmod +x test/gdb-read.sh`
(Nota: solo breakpoints/lecturas; sin watchpoints — están rotos en el stub de mGBA. Este helper es best-effort para inspección manual; el pass/fail autoritativo es el smoke test.)

- [ ] **Step 8: commit**

```bash
git add include/constants/phantom.h test/gdb-read.sh src/new_game.c src/phantom_test.c
git commit -m "feat: VAR_PHANTOM_TIME (reloj narrativo) + helper gdb-read"
```

---

## Task 6: Guardado diegético (quitar SAVE del menú de inicio)

> Enuncia "aquí no se guarda gratis". Se elimina la entrada SAVE del start menu; el guardado por script (camas/altar) queda para cuando existan mapas. Verificación: build + boot smoke sin regresión, y aserción de que el array de acciones ya no contiene SAVE.

**Files:**
- Modify: `src/start_menu.c:334` (quitar `AddStartMenuAction(MENU_ACTION_SAVE);` en `BuildStartMenuActions`)
- Modify: `include/start_menu.h` (declarar el helper de test)
- Test: aserción en `src/phantom_test.c`

**Interfaces:**
- Consumes: harness Task 3.
- Produces: el start menu de campo (`BuildStartMenuActions`, `start_menu.c:276`) no incluye `MENU_ACTION_SAVE`. Símbolos verificados: array `sCurrentStartMenuActions[9]` (`start_menu.c:85`), contador `sNumStartMenuActions`, helper `AddStartMenuAction(u8)` (`:310`), constante `MENU_ACTION_SAVE` (`:58`).

- [ ] **Step 1: confirmar la construcción del menú (ya verificado)**

Run: `sed -n '310,336p' src/start_menu.c`
Expected: `BuildStartMenuActions` (empieza en :276) añade acciones con `AddStartMenuAction(...)`; la línea **`start_menu.c:334`** es `AddStartMenuAction(MENU_ACTION_SAVE);`. El array es `sCurrentStartMenuActions` y el contador `sNumStartMenuActions`.

- [ ] **Step 2: escribir la aserción que FALLA**

En `src/phantom_test.c`, añade include:
```c
#include "start_menu.h"
```
Y un test que construye el menú de campo normal y verifica que SAVE no está. Como las funciones internas pueden ser `static`, la vía robusta es exponer un helper de test en `start_menu.c`. Primero declara en `include/start_menu.h`:
```c
#ifdef PHANTOM_TEST
bool8 PhantomTest_StartMenuHasSave(void);
#endif
```
El test:
```c
static void Test_NoSaveInStartMenu(void)
{
    PHANTOM_ASSERT(PhantomTest_StartMenuHasSave() == FALSE, "no-save-in-startmenu");
}
```
Regístralo en `PhantomTest_Run`.

- [ ] **Step 3: implementar el helper de test en `src/start_menu.c`**

Al final del archivo (nombres ya verificados: `BuildStartMenuActions`, `sCurrentStartMenuActions`, `sNumStartMenuActions`, `MENU_ACTION_SAVE`):
```c
#ifdef PHANTOM_TEST
bool8 PhantomTest_StartMenuHasSave(void)
{
    u32 i;
    BuildStartMenuActions();
    for (i = 0; i < sNumStartMenuActions; i++)
        if (sCurrentStartMenuActions[i] == MENU_ACTION_SAVE)
            return TRUE;
    return FALSE;
}
#endif
```
(`BuildStartMenuActions` arma el menú de campo estándar leyendo flags de sistema; en el test corre con el estado por defecto, suficiente para verificar la ausencia de SAVE.)

- [ ] **Step 4: correr el smoke test y verlo FALLAR**

Run: `./test/smoke.sh`
Expected: `:P FAIL no-save-in-startmenu` (SAVE aún se añade).

- [ ] **Step 5: quitar la entrada SAVE (`src/start_menu.c:334`)**

En `BuildStartMenuActions`, comenta la línea 334:
```c
    AddStartMenuAction(MENU_ACTION_PLAYER);
    // Pokémon Phantom: sin guardado desde el menú; solo diegético (camas/altar).
    // AddStartMenuAction(MENU_ACTION_SAVE);
    AddStartMenuAction(MENU_ACTION_OPTION);
```
(El array `sCurrentStartMenuActions[9]` sigue sobrado de tamaño; no hace falta tocar nada más.)

- [ ] **Step 6: correr el smoke test (debe PASAR) + release limpio**

Run: `./test/smoke.sh && make modern -j$(nproc)`
Expected: `:P PASS no-save-in-startmenu`, `SMOKE: OK`, y el build de release compila.

- [ ] **Step 7: commit**

```bash
git add src/start_menu.c include/start_menu.h src/phantom_test.c
git commit -m "feat: guardado diegetico (quitar SAVE del menu de inicio)"
```

---

## Task 7: Desactivar encuentros salvajes globalmente

> Enuncia "todo lo que te ataca te eligió". Sin mapas de isla todavía, se desactivan los encuentros de forma global al iniciar partida, usando el toggle runtime existente.

**Files:**
- Modify: `src/new_game.c` (`NewGameInitData`: llamar `DisableWildEncounters(TRUE)`)
- Modify: `src/wild_encounter.c` (helper de test `PhantomTest_WildEncountersDisabled`)
- Modify: `include/wild_encounter.h` (declarar el helper de test bajo `#ifdef PHANTOM_TEST`)
- Test: aserción en `src/phantom_test.c`

**Interfaces:**
- Consumes: `void DisableWildEncounters(bool8 disabled)` (declarada en `include/wild_encounter.h:31`, def. `src/wild_encounter.c:77`), flag `static u8 sWildEncountersDisabled` (`:62`, chequeado en `:557`).
- Produces: partida nueva arranca con encuentros salvajes desactivados globalmente.

- [ ] **Step 1: confirmar la firma y el flag (ya verificado)**

Run: `grep -n "DisableWildEncounters\|sWildEncountersDisabled" src/wild_encounter.c include/wild_encounter.h`
Expected: `include/wild_encounter.h:31: void DisableWildEncounters(bool8 disabled);` (ya expuesta — no hay que declararla), `src/wild_encounter.c:62` el flag `static u8 sWildEncountersDisabled`, `:77` la def., `:557` el chequeo `if (sWildEncountersDisabled == TRUE)`.

- [ ] **Step 2: exponer un lector para el test**

Como `sWildEncountersDisabled` es `static`, añade un helper de test en `src/wild_encounter.c` (al final):
```c
#ifdef PHANTOM_TEST
bool8 PhantomTest_WildEncountersDisabled(void)
{
    return sWildEncountersDisabled;
}
#endif
```
Y en `include/wild_encounter.h`:
```c
#ifdef PHANTOM_TEST
bool8 PhantomTest_WildEncountersDisabled(void);
#endif
```

- [ ] **Step 3: escribir la aserción que FALLA**

En `src/phantom_test.c`, añade include:
```c
#include "wild_encounter.h"
```
Y el test (corre tras el setup de save-blocks de `PhantomTest_Run`, igual que los tests de Tasks 4/5):
```c
static void Test_NoWildEncounters(void)
{
    NewGameInitData();
    PHANTOM_ASSERT(PhantomTest_WildEncountersDisabled() == TRUE, "no-wild-encounters");
}
```
Regístralo en `PhantomTest_Run` entre `Test_PhantomTimeInit()` y `suite-end`.

- [ ] **Step 4: correr el smoke test y verlo FALLAR**

Run: `./test/smoke.sh`
Expected: `:P FAIL no-wild-encounters` (por defecto los encuentros están activos).

- [ ] **Step 5: desactivar en `NewGameInitData` (`src/new_game.c`)**

Añade:
```c
    // Pokémon Phantom: sin encuentros aleatorios en toda la isla.
    DisableWildEncounters(TRUE);
```
Include si falta:
```c
#include "wild_encounter.h"
```

- [ ] **Step 6: correr el smoke test (debe PASAR) + release limpio**

Run: `./test/smoke.sh && make modern -j$(nproc)`
Expected: `:P PASS no-wild-encounters`, `SMOKE: OK`, release compila.

- [ ] **Step 7: commit**

```bash
git add src/new_game.c src/wild_encounter.c include/wild_encounter.h src/phantom_test.c
git commit -m "feat: desactivar encuentros salvajes globalmente al iniciar partida"
```

---

## Resultado de la Fase 0-1

Al terminar: build reproducible de `make modern`, un smoke test headless (`./test/smoke.sh`) que verifica en cada iteración build+boot+estado, un helper de GDB para inspección puntual, y el esqueleto de flujo del hack (sin intro de Birch, Forastero fijo, `VAR_PHANTOM_TIME`, sin SAVE en menú, sin encuentros) — todo con aserciones verdes. Base lista para el siguiente plan: el vertical slice jugable del Día 1 (pueblo + la ejecución) con placeholders.

**Nota sobre `DisableWildEncounters` y continuar partida:** este plan lo fija en `NewGameInitData` (partida nueva). Cuando exista guardado real y "continuar", habrá que re-aplicarlo al cargar (hook en el flujo de load) — se recoge en el plan del vertical slice.
