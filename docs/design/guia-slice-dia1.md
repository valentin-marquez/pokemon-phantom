# Guía técnica — Vertical slice del Día 1 (Pokémon Phantom)

> Investigación multi-agente verificada contra el repo (jul 2026). Objetivo del slice: **New Game → puerto → pueblo → escena de la ejecución del Meowth → dormir (avanza el día) → mundo desaturado permanentemente**, con assets vanilla y el mínimo de C.

**Regla de oro del motor:** solo son FUENTE `map.json`, `scripts.inc`, y el C que añadas. Todo `*.inc`/`*.h` con cabecera `@ DO NOT MODIFY THIS FILE! It is auto-generated` (p.ej. `data/maps/DewfordTown_House1/header.inc:1-3`, `events.inc:1-3`, `include/constants/map_event_ids.h:5`) lo regenera `make modern` desde el JSON. No editar.

**Orden de trabajo:** `0` arranque → `1` mapas → `2` NPCs+scripts → `3` escena ejecución → `4` cama/avance de día → `5` tinte de paleta (C) → `6` tests headless → `make modern` + `./test/smoke.sh`. Cada fase deja el juego arrancable.

## Fase 0 — Redirigir el New Game (quitar el camión)
Dos puntos independientes:
- **(a) Destino del warp** — cuerpo de `WarpToTruck()` en `src/new_game.c:135-139` (llamado en `:210`) → `SetWarpDestination(MAP_GROUP(MAP_PHANTOM_PORT), MAP_NUM(MAP_PHANTOM_PORT), WARP_ID_NONE, x, y); WarpIntoMap();`. Firma en `overworld.c:633`; coords resueltas en `SetPlayerCoordsFromWarp` (`overworld.c:603-624`): con `WARP_ID_NONE`(-1) usa `x,y` si ambos ≥0. Placeholder inicial posible: `MAP_DEWFORD_TOWN`.
- **(b) Cinemática del camión** — `CB2_NewGame()` (`overworld.c:1532`), línea `:1542`: `gFieldCallback = ExecuteTruckSequence;` → `FieldCB_WarpExitFadeFromBlack` (`:1563,1694`) o `NULL`. El avatar ya se resetea en `ResetInitialPlayerAvatarState()` (`:1538`).

## Fase 1 — Crear mapas (sin porymap, reusando layouts vanilla)
Por mapa: crear a mano `map.json` + `scripts.inc`; NUNCA crear `header.inc`/`events.inc`/`connections.inc` (los genera mapjson).
Truco: **reusar un `LAYOUT_*` vanilla** evita tocar `layouts.json` y crear `.bin`. Puerto → `LAYOUT_ISLAND_HARBOR` (17×13; coords de `data/maps/NavelRock_Harbor/map.json`). Pueblo → `LAYOUT_DEWFORD_TOWN` (20×20; coords de `data/maps/DewfordTown/map.json`).
- `scripts.inc` mínimo obligatorio: `<Name>_MapScripts::` + `.byte 0` (mapjson lo referencia en el header, `mapjson.cpp:150-153`; si falta → error de linker).
- **Registro (3 ediciones fuente):** `data/maps/map_groups.json` (añadir grupo `gMapGroup_Phantom` al final de `group_order` — NUNCA insertar en medio, desplaza índices `MAP_* = idx|(grupo<<8)`); `data/event_scripts.s` (añadir A MANO `.include "data/maps/PhantomPort/scripts.inc"` — **mapjson NO lo genera**, error de novato: compila pero no linka).
- Warps: `dest_warp_id` = índice 0-based en `warp_events` del mapa destino (`asm/macros/map.inc:58-66`). Desde script: `warp MAP_X, warpId` (`event.inc:432-476`).
- Geometría propia (Tier B, opcional): entrada en `layouts.json` + `map.bin` (w·h·2 bytes u16 LE: bits 0-9 metatile, 10-11 colisión, 12-15 elevación) + `border.bin`. Preferir reuso.

## Fase 2 — NPCs y scripts
- **NPC en `map.json`** (`object_events`; plantilla `data/maps/LittlerootTown/map.json:22-36`): `graphics_id` (`OBJ_EVENT_GFX_*`), `x/y/elevation`, `movement_type`, `trainer_type="TRAINER_TYPE_NONE"`, `script`, `flag`. `local_id="LOCALID_X"` solo si el script lo referencia.
- **Semántica invertida del `flag`:** el NPC se OCULTA cuando el flag está SET; se muestra con `clearflag` (`LittlerootTown/scripts.inc:115` esconde, `:89` muestra). Flags arrancan en 0 → NPC con `flag=FLAG_HIDE_*` es visible por defecto.
- **Script NPC** (`LittlerootTown/scripts.inc:244-281`): `lock`/`faceplayer`/`msgbox Texto, MSGBOX_NPC`/`release`/`end`. Texto al final: `<Mapa>_Text_X: .string "...$"` (`$`=fin, `\n`=línea, `\l`=scroll, `\p`=página).
- **Map scripts** bajo `<name>_MapScripts::`: `map_script`/`map_script_2` (`asm/macros/map.inc:10/16`); constantes `map_scripts.h:39-45` (`ON_LOAD=1`, `ON_FRAME_TABLE=2`, `ON_TRANSITION=3`, `ON_WARP_INTO_MAP_TABLE=4`). Terminadores distintos: `map_script`→`.byte 0`; `map_script_2`→`.2byte 0`.
- **Swap de NPCs pre/post ejecución (mismo Día 1):** usa la FLAG `FLAG_PHANTOM_SAW_EXECUTION` en `ON_TRANSITION` (corre antes de spawnear objetos), NO el var (el var solo avanza al dormir). Días futuros: ramas `goto_if_ge VAR_PHANTOM_TIME, PHANTOM_TIME_DAY2`.
- **Escena automática al entrar:** `ON_FRAME_TABLE` con `map_script_2 VAR, valor, Script` + `.2byte 0` (`LittlerootTown/scripts.inc:104-108`); el script hace `setvar` al final para no re-dispararse.
- **Decisiones:** `yesnobox x,y` + `goto_if_eq VAR_RESULT, YES/NO` (sin C); listas custom vía `multichoice` + `switch VAR_RESULT`. `VAR_RESULT=0x800D`.

## Fase 3 — Escena de la ejecución
Un solo script de evento:
```
Phantom_EventScript_MeowthExecution::
    lockall
    applymovement LOCALID_VILLAGER_1, Movement_CloseCircle1
    ... (varios aldeanos cerrando el círculo)
    waitmovement 0
    fadeoutbgm 2                     @ corte seco (event.inc:382)
    delay 30
    message Phantom_Text_ActaLine1
    waitmessage
    playse SE_BANG                   @ SE_BANG=20; campana SE_DING_DONG=73 (songs.h)
    @ (opc) special ShakeCamera + waitstate (patrón cave_of_origin.inc:29-36)
    fadescreen FADE_TO_BLACK          @ =1 (field_weather.h:21)
    setflag FLAG_PHANTOM_MEOWTH_EXECUTED   @ enciende la desaturación
    setflag FLAG_PHANTOM_SAW_EXECUTION     @ swap de NPCs
    delay 60
    fadescreen FADE_FROM_BLACK        @ =0
    fadeinbgm 3                        @ o deja silencio
    releaseall
    end
```
- El SE se lanza desde el script (`playse`+`waitse`), NO como `{PLAY_SE}` inline (dispararía al imprimir el carácter).
- Meowth víctima = `OBJ_EVENT_GFX_MEOWTH_DOLL` (150, `event_objects.h:157`, `.inanimate=TRUE`) con `MOVEMENT_TYPE_NONE` sobre el poste. No hay OW de Meowth vivo.
- Texto en tono de acta: `{PAUSE 15}` (charmap.txt:420); acentos directos en `.string` (charmap: `á é í ó ú ñ ¿ ¡`).

## Fase 4 — Cama, avanzar día, guardar, curar
No hay metatile-behavior de cama en vanilla. Usar `bg_event` tipo `sign` (u object/coord event) sobre la cama:
```
Phantom_EventScript_Bed::
    lockall
    fadescreen FADE_TO_BLACK
    special HealPlayerParty            @ specials.inc:11
    special PhantomAdvanceDay          @ VAR_PHANTOM_TIME++
    call Common_EventScript_SaveGame   @ std_msgbox.inc:46-49
    fadescreen FADE_FROM_BLACK
    releaseall
    end
```
**Orden crítico:** avanzar el día ANTES de `SaveGame`.
Specials en C nuevos (`src/phantom.c`, registrados con `def_special` en `data/specials.inc:1`):
```c
void PhantomAdvanceDay(void)        { VarSet(VAR_PHANTOM_TIME, VarGet(VAR_PHANTOM_TIME) + 1); }
void PhantomMarkExecutionSeen(void) { FlagSet(FLAG_PHANTOM_SAW_EXECUTION); }
```

## Fase 5 — Desaturación permanente (C, no editar .pal)
El tinte DEBE vivir en `gPlttBufferUnfaded` (fade y clima leen de unfaded y re-escriben faded — `field_weather.c:494,574,625,669`) y aplicarse en el CAMINO DE CARGA de paletas (corre en cada carga de mapa).
- **Hook BG (ya existe, hoy stub):** `ApplyGlobalTintToPaletteEntries(offset, size)` en `src/fieldmap.c:865`, llamado desde `LoadTilesetPalette` (`:885,890,895`). `size` viene en nº de entradas u16. Gate por `FlagGet(FLAG_PHANTOM_MEOWTH_EXECUTED)`; fórmula luminancia igual a `TintPalette_GrayScale` (`palette.c:852`); escribir unfaded Y faded.
- **La otra mitad — SPRITES:** el hook BG no los toca. Añadir el mismo tinte tras `LoadPalette(..., OBJ_PLTT_ID(slot), PLTT_SIZE_4BPP)` en `src/event_object_movement.c:2048` (helper compartido `Phantom_TintPaletteRange`, `offset=OBJ_PLTT_ID(slot)`, `count=16`, mismo gate). NO tintar dentro de `LoadPalette` global (rompería menús/combate).
- **Flag:** renombrar `FLAG_UNUSED_0x020` (`flags.h:46`) a `FLAG_PHANTOM_MEOWTH_EXECUTED` (persistente). NO `FLAG_TEMP_*` (se resetean por mapa), NO gatear por `VAR_PHANTOM_TIME` (vale DAY1 todo el día → desaturaría desde el amanecer).

## Fase 6 — Tests headless
El harness (`src/phantom_test.c`) llama funciones C directas: `fadescreen`/`SaveGame`/`waitstate` NO son ejercitables headless (gap conocido). Sí aseverable: mutación determinista (`VarSet`/`FlagSet`) → poner la lógica en los specials C reusables y testearlos:
```c
static void Test_PhantomSleepAdvancesDay(void) {
    VarSet(VAR_PHANTOM_TIME, PHANTOM_TIME_PROLOGUE);
    PhantomAdvanceDay();
    PHANTOM_ASSERT(VarGet(VAR_PHANTOM_TIME) == PHANTOM_TIME_DAY1, "sleep-advances-day");
}
```

## Arte placeholder (cero arte nuevo)
- **OWs** (`event_objects.h`): `FAT_MAN`(17), `OLD_MAN`(29), `OLD_WOMAN`(30), `LITTLE_BOY`(11), `WOMAN_1`(16), `MAN_1`(19), `SAILOR`(49), `FISHERMAN`(50); oficiante con tono: `GENTLEMAN`(48) / `HEX_MANIAC`(40).
- **Meowth** = `MEOWTH_DOLL`(150), único con silueta de Meowth; inanimate. Paleta compartida `OBJ_EVENT_PAL_TAG_NPC_2`.
- **Tilesets** (`src/data/tilesets/headers.h`): puerto → `gTileset_General`+`gTileset_IslandHarbor`(:752) o `gTileset_Facility`; pueblo → `gTileset_General`+`gTileset_Dewford`(:34) o `gTileset_Slateport`(:45). El par primary+secondary NO es libre: reusar el que ya usa un layout vanilla.
- Poste de flagelación: metatile por índice en Porymap (sin constante simbólica; no inventar IDs); el doll de Meowth encima.

## Checklist de archivos fuente a tocar
1. `data/maps/PhantomPort/{map.json,scripts.inc}` + `data/maps/PhantomTown/{map.json,scripts.inc}` (crear).
2. `data/maps/map_groups.json`, `data/event_scripts.s` (registro; include manual obligatorio).
3. `src/new_game.c:135-139`, `src/overworld.c:1542` (arranque).
4. `src/fieldmap.c:865` (hook BG) + `src/event_object_movement.c:2048` (hook sprites) + helper compartido.
5. `include/constants/flags.h:46` (renombrar flag), `include/constants/phantom.h` (times, ya existe).
6. `src/phantom.c` (specials) + `data/specials.inc` (`def_special`).
7. `src/phantom_test.c` (2 tests + registro).
