# Fase "Debug visual + Sandbox" — Diseño

> Spec (18 jul 2026). Fase puente entre el andamiaje (Fase 0-1, ya en master) y el vertical slice del Día 1. Dos entregables: (1) un harness de debugging **visual** autónomo, y (2) un **mapa sandbox** donde validar cada mecánica del Día 1 en aislamiento antes de tocar contenido real. Referencias: [`docs/design/harness-fase2-visual.md`](../../design/harness-fase2-visual.md) (visual, verificado) y [`docs/design/guia-slice-dia1.md`](../../design/guia-slice-dia1.md) (mecánicas del slice, file:line).

## Por qué esta fase
El smoke test de la Fase 0-1 verifica **lógica** headless (vars/flags) pero no puede ver mapas, escenas ni efectos de paleta. El Día 1 es puro visual+interactivo. Sin debugging visual no puedo desarrollarlo de forma autónoma. Y conviene probar cada patrón (crear mapa, NPC, escena, desaturación, dormir) en un sandbox desechable antes de construir el pueblo real.

## Entregable 1 — Harness de debugging visual (Fase 2)
Herramienta Python **versionada** (en `tools/phantom-debug/`) sobre el paquete `mgba` (verificado end-to-end: ~1378 fps, captura la title screen real, lee memoria por símbolo). Expone:
- Cargar ROM (release o `_test`) + savestate opcional; inyectar inputs frame-a-frame; avanzar N frames; capturar PNG.
- Leer estado por símbolo: `VAR_PHANTOM_TIME`, flags, coords/mapa del jugador — vía `.map`/`nm` (globales) + `pyelftools` (offsets de struct; el gdb del toolchain tiene el DWARF5 roto).
- Savestates save/load para reproducibilidad.
- CLI de conveniencia: `boot-newgame` (bootea → título/menú → New Game → overworld), `--screenshot`, `--read VAR/FLAG`.
- **No reemplaza** al smoke test (ese sigue siendo el pass/fail de lógica); esto es verificación **visual**.
- Setup: `sudo pacman -S libmgba` (ya hecho) + venv 3.11 con `uv` (sin sudo).

## Entregable 2 — Mapa sandbox (`MAP_PHANTOM_SANDBOX`)
Mapa de pruebas desechable que ejercita **cada mecánica del Día 1 una vez**, verificado con el Entregable 1. Reusa un layout vanilla (cero binarios de mapa), arte placeholder vanilla. Durante esta fase, New Game arranca ahí (redirect temporal del warp inicial). Contiene:
- Un mapa nuevo vía el pipeline JSON (map.json + scripts.inc + registro en map_groups.json/event_scripts.s).
- Un NPC con diálogo (`msgbox`, texto español).
- Una mini-escena scripteada: fundido a negro + 1 SE + texto tipo acta + encender una flag (el esqueleto de la ejecución, sin contenido).
- La **desaturación de paleta** activada por esa flag (hook `fieldmap.c:865` para BG + hook sprites `event_object_movement.c:2048`), gateada por `FLAG_PHANTOM_MEOWTH_EXECUTED`. Aquí **vemos y decidimos el look** por captura (gris total vs tono frío; fuerza).
- Una "cama" que avanza `VAR_PHANTOM_TIME` + guarda, con specials en C reusables (`PhantomAdvanceDay`, `PhantomMarkExecutionSeen`) y tests headless de esos specials.

## Alcance / decisiones
- **Sandbox = desechable.** No es contenido final; es andamiaje para validar patrones y para que yo tenga un estado visual estable donde iterar. Puede borrarse o reciclarse cuando empiece el Día 1 real.
- **Verificación doble por tarea:** smoke test (lógica) + captura de pantalla (visual) donde aplique. La desaturación se valida VISUALMENTE (antes/después de la flag).
- **Diferido al Día 1 real** (el sandbox nos deja verlos antes de decidir): tileset del puerto/pueblo, fuerza/tono exacto de la desaturación, si el Meowth se mueve, trigger de la escena (auto vs interacción), grupo de mapa nuevo vs append.
- **Orden:** Entregable 1 (herramienta) primero — es el habilitador; luego el sandbox lo usa.

## Fuera de alcance
- El contenido real del Día 1 (puerto/pueblo de Sombraluna, los 10 NPCs, la ejecución del Meowth guionada) — es el plan siguiente.
- `mgba-qt` GUI / GDB stub interactivo (requieren `-Syu` o `mgba-sdl`; no necesarios para el path Python).
- Override de RTC end-to-end (savestates cubren el determinismo).
