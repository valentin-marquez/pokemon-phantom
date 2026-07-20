# Front-end del título: PRESS START, vidrio impactado y menú Nueva/Continúa

**Fecha:** 2026-07-20
**Estado:** Diseño aprobado, pendiente de plan de implementación.
**Alcance:** Pieza 1 de 2. Esta pieza es la **cáscara** del front-end (título → PRESS START →
vidrio → menú → enrutado). La **Pieza 2** (spec aparte) es la *secuencia de intro narrativa*
(tren → nombre → derrota guionizada → marcador con récord del hermano → capitán → isla), a la
que "Nueva partida" entregará el control.

## Objetivo

Convertir el título actual (que hoy va a un dead-end del minijuego) en el front-end real y
jugable del juego:

- Título con "PRESS START" (ya existe).
- **Sin partida guardada:** PRESS START → animación de **vidrio impactado** → arranca la partida.
- **Con partida guardada:** PRESS START → selector **Nueva partida / Continúa** sobre el título →
  al elegir → vidrio impactado → (nueva partida | cargar save).
- El vidrio impactado **solo** se dispara al **confirmar una opción que cambia de pantalla**,
  nunca al navegar el menú.

## Restricción dura: no tocar lo ya hecho

Los sistemas ya construidos **funcionan y quedan congelados**: `src/phantom_fx.c` (mareo),
`src/phantom.c` (desaturación rojiza), el sandbox, el harness de test/visual, y las mecánicas del
Día 1. Esta feature es **aditiva**: un módulo nuevo + el mínimo cambio de enrutado en el título.

## Arquitectura

### Archivos

- **NUEVO `src/phantom_intro.c`** (+ `include/phantom_intro.h`) — todo el front-end: el efecto de
  grietas y la lógica de menú + enrutado. Una unidad con un propósito claro, aislada del título.
- **EDITADO `src/title_screen.c`** — cambio mínimo: donde hoy `Task_TitleScreenMain` hace
  `FadeOutBGM + BeginNormalPaletteFade + SetMainCallback2(CB2_TaskFadeOutToMinigame)` al pulsar
  A/START, se llama en su lugar a `PhantomIntro_OnStartPressed()`. El resto del título (nubes,
  logo, silueta, estrellas, glitch, sprites PRESS START) **no cambia**. Se puede borrar el
  `CB2_TaskFadeOutToMinigame`/`Task_FadeOutToMinigame` muertos si quedan sin uso, pero no es
  obligatorio.
- **NUEVO asset de grietas** — textura generada por un **script Python** commiteado
  (`graphics/phantom_intro/crack_gen.py` → `graphics/phantom_intro/crack.png`), convertida por el
  pipeline normal (`.png` → `.4bpp`/`.gbapal` vía gbagfx). Nada dibujado a mano. La textura es
  grietas de araña claras sobre fondo transparente (índice 0), pensada para la capa OBJ.

### Interfaz pública (`include/phantom_intro.h`)

```c
// Llamado por title_screen.c al pulsar A/START en el título.
void PhantomIntro_OnStartPressed(void);
```

Todo lo demás (el efecto de vidrio, el menú, el enrutado) es interno a `phantom_intro.c`.

## Componentes

### 1. Efecto de vidrio impactado — `PhantomGlass_Start(MainCallback nextCB)`

Máquina de estados por task que reproduce la animación y, al terminar, hace
`SetMainCallback2(nextCB)`. Fases:

1. **Impacto** (frame 0): SFX (`SE_ICE_BREAK` como golpe, opcional `SE_ICE_CRACK` en la expansión)
   + flash blanco breve vía `BLDY`.
2. **Sacudida** (~10–14 frames): jitter decreciente de los offsets de las BG (unos pocos px) para
   que la pantalla "tiemble".
3. **Grietas**: la textura de araña se muestra desde el centro en la capa **OBJ (sprites)**, por
   encima de las 4 BG del título (no las molesta). Puede revelarse en 2 etapas (grieta pequeña →
   completa) para dar sensación de que "se expande".
4. **Aguanta** (~20 frames) con las grietas fijas.
5. **Fade a negro** (`BeginNormalPaletteFade` a negro) y, al completar, `SetMainCallback2(nextCB)`.

Notas de implementación:
- Estáticos en `.bss` (nada inicializado a no-cero → el ld modern descarta `.data`). Patrón ya
  aprendido en `phantom_fx.c`.
- La textura de grietas se carga en VRAM de OBJ y sus sprites se crean al empezar el efecto; se
  liberan implícitamente en el reset de la siguiente pantalla (el `nextCB` reinicializa sprites).
- El efecto es reutilizable: recibe el `nextCB`, no conoce el enrutado.

### 2. Menú Nueva/Continúa (overlay sobre el título)

Solo aparece cuando hay save y se pulsa PRESS START. Ventana de texto estándar (sistema
`Window`/`AddTextPrinterParameterized` + cursor de menú) con dos opciones en español:

```
▸ Nueva partida
  Continúa
```

- El sprite "PRESS START" se **oculta** mientras el menú está arriba.
- Navegación ↑/↓ mueve el cursor (**sin** disparar el vidrio).
- **A** confirma la opción → dispara el vidrio con la ruta correspondiente.
- **B** cierra el menú y **vuelve** a PRESS START (re-muestra el sprite).
- Posición/estilo exactos del menú se afinan **en vivo** (iteración visual).

### 3. Enrutado — `PhantomIntro_OnStartPressed()`

Lee `gSaveFileStatus` (de `save.h`):

- **`!= SAVE_STATUS_OK`** (sin partida válida): `PhantomGlass_Start(entradaIntro)` directo.
- **`== SAVE_STATUS_OK`** (hay partida): abre el menú.
  - Nueva partida → `PhantomGlass_Start(entradaIntro)`.
  - Continúa → `PhantomGlass_Start(CB2_ContinueSavedGame)`.

Donde **`entradaIntro` = `CB2_InitMinigameShip`** (la entrada actual del shmup placeholder).
En la Pieza 2 este destino se sustituye por el punto de entrada de la secuencia real; es el
**único** punto que cambiará.

## Flujo de datos

```
Título (PRESS START parpadea)
  │  [A / START]
  ▼
PhantomIntro_OnStartPressed()  ── lee gSaveFileStatus
  ├─ sin save ──────────────────────────────► PhantomGlass_Start(CB2_InitMinigameShip)
  └─ con save ─► menú Nueva/Continúa (overlay)
                   ├─ [B] ─► cierra, vuelve a PRESS START
                   ├─ [A en "Nueva partida"] ─► PhantomGlass_Start(CB2_InitMinigameShip)
                   └─ [A en "Continúa"] ──────► PhantomGlass_Start(CB2_ContinueSavedGame)
                                                        │
  animación (impacto→sacudida→grietas→aguanta→fade)  ◄─┘
                                                        │  al completar el fade
                                                        ▼
                                                 SetMainCallback2(nextCB)
```

## Manejo de errores / bordes

- **Save corrupto o incompleto:** solo `SAVE_STATUS_OK` cuenta como "hay partida → menú".
  Cualquier otro estado (`EMPTY`, `CORRUPT`, `ERROR`, `NO_FLASH`) cae al camino sin-save
  (PRESS START → vidrio → nueva partida). Simple y robusto; evita ofrecer un Continue que fallaría.
- **Doble input:** una vez disparado el vidrio, se ignora más input (la task del efecto es la que
  manda hasta el `nextCB`).
- **B en el menú:** único camino que no cambia de pantalla ni truena el vidrio.

## Testing y cómo se itera

- **Visual (tú lo ves, yo lo evalúo):** el harness arranca desde power-on → cae en el título
  natural → se inyecta START, se captura **GIF de las grietas**; con un save en la flash se captura
  el **menú** y ambas rutas. Se te manda un GIF por iteración. El look (sacudida, expansión de
  grietas, SFX, posición del menú) se ajusta en vivo con tu criterio + mi evaluación.
- **Smoke:** `./test/smoke.sh` sigue verde (`make PHANTOM_TEST=1 modern` compila el módulo nuevo;
  el harness no ejercita el título, pero el build no debe romperse).
- **Builds:** `make modern` (release) debe compilar limpio con el módulo aditivo. Los tres builds
  (release / `PHANTOM_TEST` / `PHANTOM_DEBUG_BOOT`) siguen siendo mutuamente excluyentes; el
  `PHANTOM_DEBUG_BOOT` sigue saltando el título (no le afecta esta feature).

## Fuera de alcance (Pieza 2, spec aparte)

- La secuencia de intro real: imagen del tren + "una partida más".
- Entrada de nombre dentro del shmup.
- Partida forzada a perder.
- Marcador con el récord del hermano (Carlos).
- Escena del bote/capitán → "llegamos a la isla" → overworld.
- Reconciliar el andamiaje de New Game (nombre "?" fijo) con la entrada de nombre en el shmup.
