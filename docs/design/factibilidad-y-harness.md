# Factibilidad técnica y harness de testing — Pokémon Phantom

> Auditoría (17 jul 2026) de cada sistema del diseño contra el código real del repo, con evidencia `file:line`, + arquitectura del harness de testing IA. Producido por auditoría multi-agente sobre `/mnt/datos/dev/pokemon-phantom`. Build objetivo: `make modern` (gcc); nada depende de agbcc.

## Leyenda
**SÍ** = hook vanilla directo · **SÍ-T** = sí con trabajo (código C nuevo sobre infraestructura existente). Complejidad: **S** (horas), **M** (días), **L** (semanas).

## Matriz de factibilidad

| Sistema del diseño | Fact. | Compl. | Hooks verificados (file:line) |
|---|---|---|---|
| **Reloj / culpa** (contador de días, estado-mundo persistente, NPCs distorsionados por día) | SÍ | S | 22 `VAR_UNUSED_*` persistentes 0x404E–0x40FF `include/constants/vars.h:98-275`; 376 `FLAG_UNUSED_*` `include/constants/flags.h`; persistencia `include/global.h:1019-1020`; ON_TRANSITION/ON_FRAME/ON_WARP `include/constants/map_scripts.h:40-42` + `asm/macros/map.inc:10,16` (ej. `data/maps/LittlerootTown/scripts.inc:1-4,40-52`); specials nuevos `data/specials.inc:1-6` + `src/scrcmd.c:118-130`; distorsión de sprites `OBJ_EVENT_GFX_VAR_0..F` `include/constants/event_objects.h:262-278` → `src/event_object_movement.c:1914-1936` → `src/event_data.c:191-194` (`VAR_OBJ_GFX_ID_0..F` 0x4010–0x401F persisten, `vars.h:32-47`) |
| **Ritual de la cama / cena** (multichoice) | SÍ | S | `multichoice`/`multichoicedefault`/`yesnobox` `asm/macros/event.inc:887-923`; motor `src/script_menu.c:39,96-97,188` (resultado en `gSpecialVar_Result`, B=127 `include/constants/script_menu.h:8`); tabla data-driven `src/data/script_menu.h:785` |
| **Guardado diegético** (solo cama/altar, sin SAVE en start menu) | SÍ | S | Quitar SAVE = borrar 1 línea `src/start_menu.c:334` (menú dinámico `:276-308`); guardar por script: `def_special SaveGame` `data/specials.inc:107` → `src/start_menu.c:896-930`; script común `data/scripts/std_msgbox.inc:46-49`; uso vanilla `BattleFrontier_BattleDomeLobby/scripts.inc:181`. Cero C nuevo |
| **Sin encuentros salvajes** | SÍ | S | Omitir mapa de `src/data/wild_encounters.json`; `GetCurrentMapWildMonHeaderId`→`HEADER_NONE` `src/wild_encounter.c:305-332`, salida limpia `:552-563`; toggle runtime `DisableWildEncounters` `:77-79` (añadir special si se quiere por script) |
| **Combates puzzle + imposibles** | SÍ-T | M / M–L | `setwildbattle`/`dowildbattle` `asm/macros/event.inc:1448,1456` → `src/script_pokemon_util.c:137-149`; resultado `GetBattleOutcome` `data/specials.inc:194` + `src/field_specials.c:922` (uso `FarawayIsland_Interior/scripts.inc:13`), `B_OUTCOME_*` `include/constants/battle.h:100-110`. Moves fijos: patrón `LoadWallyZigzagoon` `src/field_specials.c:1423-1435`. Leer DERROTA: excepción Pyramid/Secret Base `src/battle_setup.c:624-626,1329-1336`. Whiteout→escena: `DoWhiteOut` `src/overworld.c:358-366` + `setrespawn` `src/scrcmd.c:2005-2011`. **Imposible cinemático (molde Wally) = L; vía barata (M) = enemigo invencible + excepción de derrota + forzar `gBattleOutcome`** (`src/battle_util.c:535`, `src/battle_script_commands.c:6489-6491`) |
| **Breedmare-estado** (cicatriz, cry propio, persecución) | SÍ-T | M (L multi-mapa) | Cicatriz: `GetMonData`/`SetMonData` desde special, molde `ChangePokemonNickname` `src/tv.c:3292-3305`; **NUNCA stats directos** (`CalculateMonStats` los recalcula) — vía EVs/IVs/flag. Cry: case nuevo en `PlayCryInternal` `src/sound.c:368-455` (reversa ya en ROM `gCryTable_Reverse` `:474-475`, pitch grave 14440–15360) vía `playmoncry` `asm/macros/event.inc:1296` → `src/scrcmd.c:2019-2026`. Persecución 1 mapa: IA de Mew invertida `src/faraway_island.c:44-64` + `MOVEMENT_TYPE_COPY_PLAYER` `src/event_object_movement.c:4157-4176`; **multi-mapa: NO hay follower** — duplicar NPC por mapa (patrón rival) |
| **Gramática de glitches** (tinte, mosaic, scanline, metatiles, clima, temblor, SE paneado) | SÍ-T | M (S por pieza) | Tinte overworld: stub FRLG `ApplyGlobalTintToPaletteEntries` `src/fieldmap.c:864-868` (llamado `:885,890,895`), `TintPalette_*` `src/palette.c:852,891,915` — **aplicar a `gPlttBufferUnfaded`** o el clima lo pisa (`src/field_weather.c:459-486`); OBJ palettes por ruta aparte. Mosaic: `Blur_Init` `src/battle_transition.c:1141-1162` + `src/fldeff_misc.c:1206-1227`. Scanline wave: `src/scanline_effect.c:214-235` + `src/overworld.c:1784-1792`; **incompatible con flash de cueva, muere en cada warp**. Metatiles: `setmetatile` `src/scrcmd.c:2034-2048` + `special DrawWholeMapView` `data/specials.inc:156`; **no persiste** → re-aplicar en ON_LOAD (molde Sootopolis `scripts.inc:19-24`). Clima: `setweather`+`doweather` `src/scrcmd.c:705-723`, FOG/ASH/SHADE `include/constants/weather.h:10-15`. Temblor: `ShakeCamera` `src/field_specials.c:1470-1506`. SE con pan: `PlaySE12WithPanning` `src/sound.c:576-583` (sin comando de script → wrapper special) |
| **Plano de Sombraluna** (key item con script) | SÍ-T | S–M | Molde Wailmer Pail: `src/data/items.h:3258-3268` (`POCKET_KEY_ITEMS`+`ITEM_USE_FIELD`+`fieldUseFunc`) → `src/item_use.c:707-730` (`ScriptContext_SetupScript`). Nuevo: fn en `src/item_use.c` + `include/item_use.h` + slot `ITEM_0xx` libre + **icono OBLIGATORIO** `src/data/item_icon_table.h:1` |
| **Estampas fullscreen** | SÍ | M | Molde `src/minigame_pre.c:35-37,52-67,101-114,139-171,222-263` (INCBIN, BgTemplates, VBlank/CB2 propios); variante 4 BGs `src/minigame_spaceship.c:91-132`. **Límites: 1 imagen 8bpp por vez** (37.5 KB char + 2 KB map en 64 KB BG VRAM); ≤240 colores (paleta 15 al textbox `minigame_pre.c:154`). Falta hook de entrada (special) y retorno a `CB2_ReturnToField` |
| **Shmup v2 corrupto** | SÍ-T | M | Gfx por INCBIN `src/minigame_spaceship.c:33-56`; **swap de paleta corrupta = trivial** (`LoadSpaceshipGraphics :243`). **PERO no hay sistema de enemigos**: el loop es placeholder con timer 30 s (`:475-483`) — spawn/movimiento/colisión del "escolta" es código nuevo (infra sprites `:181-189,322-331`) |
| **Recorte de intro / menús** | SÍ | S | Intro ya destripada (`src/intro.c:31-35,135`); new game: fijar género/nombre y saltar a `CB2_NewGame` (`src/main_menu.c:1776-1786`, `src/new_game.c:149`); camión `src/overworld.c:1542`, `src/new_game.c:127-131`. Pokédex/Pokénav: NO setear `FLAG_SYS_POKEDEX_GET`/`POKENAV_GET`/`POKEMON_GET` (`src/start_menu.c:315-337`) |
| **Audio** (2-3 piezas, 3-4 SEs, códigos en texto) | SÍ-T | M | `.mid` + `midi.cfg` → `audio_rules.mk:28-45`, `Makefile:214-218` + `ld_script_modern.ld:88`; **anexar AL FINAL de `sound/song_table.inc`** + constante en `include/constants/songs.h`; SE precedente `midi.cfg:262`. **Reusar los 182 voicegroups** (samples nuevos multiplican costo). Códigos msgbox `PAUSE`/`PLAY_SE`/`PAUSE_MUSIC`/`RESUME_MUSIC` `charmap.txt:420-436` → `src/text.c:1013-1062`. Costo dominante: autoría MIDI |
| **Strings de sistema** | SÍ | S | Battle intro: `src/battle_message.c:380-384` (sel. `:2031`); guardado: `data/text/save.inc:1-33`; duplicados `src/strings.c:168,1268-1269,1464`. Acentos del español vía charmap |

**Transversales:** mapas nuevos = SÍ [S] con pipeline JSON + porymap (`tools/mapjson/`, `map_data_rules.mk:29-41`). Nada crece EWRAM/SaveBlock; todo compila con `make modern`.

## Red flags (baked into el spec)

1. **BLOQUEANTE:** hoy `make modern` no compila (falta `arm-none-eabi-gcc`, no hay `.gba`/`.elf`). Fase 0 del harness.
2. **mGBA 0.10.5 sin `--script` ni headless con scripting:** los `.lua` se cargan por GUI (o `xdotool`, frágil); la ruta headless real es `libmgba-py` (exige Python ≤3.12; el sistema tiene 3.14).
3. **Determinismo:** el RTC de Emerald toma la hora del host — justo el sistema de días; los tests fijan estado por savestate/memoria, nunca "desde cero".
4. **Persecución multi-mapa:** sin follower en vanilla → duplicar NPC por mapa (o segmentos de un mapa por warp). El spec acota a segmentos de mapa único.
5. **Combate imposible cinemático (Wally) = L:** default = vía barata M (enemigo invencible + excepción de derrota + `gBattleOutcome`).
6. **Perder batalla scripteada → `CB2_WhiteOut` corta el script** (`battle_setup.c:628/1338`): leer `B_OUTCOME_LOST` exige la excepción Pyramid (~5 líneas); `DoWhiteOut` cura y resta ½ dinero incondicionalmente — condicionar si la escena lo requiere.
7. **Cicatriz del compañero:** nunca stats/HP directos (`CalculateMonStats`); vía EVs/IVs o flag. Añadir campo a `struct Pokemon` rompe el savefile.
8. **Tinte de paletas:** debe vivir en `gPlttBufferUnfaded` (stub `fieldmap.c:864`) o el clima/fadescreen lo pisa; paletas OBJ por ruta aparte.
9. **`ScanlineEffect_InitWave`:** comparte DMA0 con el flash de cuevas (incompatibles) y muere en cada warp → relanzar por mapa, prohibir en mapas con `flashLevel>0`.
10. **Estampas:** solo UNA imagen 8bpp 240×160 a la vez (transición = fade + recarga), ≤240 colores, paleta 15 al textbox; falta hook de entrada/retorno.
11. **Shmup v2:** el "escolta" implica escribir enemigos desde cero; el swap de paleta corrupta es trivial.
12. **Audio:** anexar canciones al final de `gSongTable` (insertar en medio rompe constantes); reutilizar voicegroups; `mid2agb` quisquilloso con el MIDI.
13. **`setmetatile` no persiste** (escribe RAM): re-aplicar en `MAP_SCRIPT_ON_LOAD` ligado a flags; cambio en caliente exige `special DrawWholeMapView`.
14. **`OBJ_EVENT_GFX_VAR`:** 16 slots, `graphicsId` u8, `VAR_0` ya lo usa el rival, setear en ON_TRANSITION (no refresca NPCs ya spawneados). Presupuestar slots desde el diseño.
15. **Códigos en texto:** `PAUSE_MUSIC` deja el BGM parado si el texto se corta antes de `RESUME_MUSIC` — prohibir textos cortables entre ese par.
16. **Key item Plano:** omitir el icono en `gItemIconTable` crashea la bolsa; usar slot `ITEM_0xx` libre sin tocar `ITEMS_COUNT`.
17. **GDB stub de mGBA:** watchpoints remotos lentísimos/cuelgan (mgba#1355), hardware watchpoints rotos desde 0.8 (mgba#1947) → solo breakpoints + lecturas; `mgba-rom-test` eliminado en mGBA master (usar binario de expansion o compilar tag 0.10.5).

---

## Harness de testing IA — arquitectura en capas

**Entorno verificado (2026-07-17):** `gdb 17.2` con `--enable-targets=all` (depura ARM sin toolchain extra), `lua 5.5`, `xdotool`, Python 3.14. **Falta:** `arm-none-eabi-gcc` y el propio ROM. A favor: el logging a mGBA ya está cableado en vanilla — `include/config.h:34` (`LOG_HANDLER_MGBA_PRINT`), `src/main.c:126` (`MgbaOpen`), `src/libisagbprn.c:216-236` (`MgbaPrintf`). `DebugPrintf(...)` imprime en mGBA sin tocar nada.

### Fase 0 — Bootstrap (una vez, bloqueante)
```bash
sudo pacman -S --needed mgba-qt mgba-sdl arm-none-eabi-gcc arm-none-eabi-binutils \
  arm-none-eabi-newlib arm-none-eabi-gdb xorg-server-xvfb
make DINFO=1 modern -j$(nproc)   # → pokeemerald_modern.gba + .elf (DWARF) + .map
# mgba-rom-test: eliminado en mGBA master → binario precompilado de pokeemerald-expansion
#   (tools/mgba/mgba-rom-test) o compilar del tag 0.10.5 con cmake -DBUILD_ROM_TEST=ON
```
Éxito de la fase: la ROM bootea en `mgba-qt` y el log muestra el handshake mGBA.

### Fase 1 — Smoke test headless (implementar PRIMERO; es la red de seguridad más barata)
1. Hook en C tras `CB2_NewGame`: si una var/flag de test está activa, ejecutar la escena bajo prueba emitiendo `DebugPrintf(":P checkpoint")` y terminar con un SWI reservado dejando pass/fail en r0.
2. Runner:
```bash
stdbuf -oL ./mgba-rom-test -S 0x27 -Rr0 -l15 pokeemerald_modern.gba | tee test.log
echo "exit=$?"
```
`mgba-rom-test` no tiene Lua ni inputs — solo tests autoejecutables dentro de la ROM. NO portar el framework de tests de expansion (L); un smoke-runner propio es M.

### Fase 2 — Control interactivo con screenshots (el agente "juega")
Ruta limpia: **libmgba-py** (patrón pokebot-gen3, soporta Emerald frame-perfect).
```bash
uv python install 3.12   # Python 3.14 rompe el build cffi de libmgba-py
# compilar libmgba con BUILD_PYTHON, instalar bindings en venv 3.12
```
Driver Python: ROM + savestate → bucle `set_inputs → run_frame → screenshot PNG → leer memoria`. Direcciones por símbolo: `grep gSaveBlock1Ptr pokeemerald_modern.map`.
Alternativa GUI visible: `mgba-qt ROM` + cargar por GUI (Tools > Scripting) un socket-server Lua (mGBA-http / mcp-mgba, precedente directo de MCP↔Claude↔mGBA), controlar por `curl`. La API Lua cubre `emu:read8/16/32`, `emu:setKeys`, `emu:screenshot(file)`, sockets TCP, `emu:runFrame()`.

### Fase 3 — Debug simbólico bajo demanda (funciona hoy con el gdb del sistema)
```bash
mgba-qt pokeemerald_modern.gba -g &            # stub GDB en :2345, arranca pausado
gdb -batch pokeemerald_modern.elf \
  -ex 'target remote localhost:2345' \
  -ex 'b ScriptContext_RunScript' -ex 'continue' \
  -ex 'p/x gSaveBlock1Ptr->location' \
  -ex 'p gSaveBlock1Ptr->vars[0x40]' -ex 'detach'
```
Requiere `DINFO=1` (DWARF). Solo breakpoints + lecturas; evitar watchpoints. La emulación se congela con gdb parado — no mezclar con Fase 2 en la misma sesión.

### Fase 4 — Menú debug in-ROM (calidad de vida, al final)
Menú mínimo propio (pret wiki "Add a debug menu": ~4 archivos, `make DDEBUGGING=1`) = S. Alternativa completa: `tx_debug_system` de TheXaman (warp, flags, vars, give, heal) pero con conflictos de merge contra este árbol ya modificado. Empezar por el mínimo.

### Orden y porqué
Fase 0 (sin toolchain no hay nada) → Fase 1 (pass/fail automático, red de seguridad) → Fase 2 (verificación visual: estampas, glitches, timing) → Fase 3 (inspección puntual de estado al fallar un test) → Fase 4 (acelera iterar, no bloquea).
