# Pieza 2 — El prólogo de la consola (spec de diseño)

> Spec de diseño (21 jul 2026). Sustituye el minijuego shmup del prólogo por un
> dungeon crawler de Game Boy. Documentos hermanos:
> [`2026-07-17-pokemon-phantom-design.md`](2026-07-17-pokemon-phantom-design.md)
> (spec general — **este documento obliga a modificarlo**, ver §2),
> [`../../design/limites-graficos-gba.md`](../../design/limites-graficos-gba.md)
> (límites de hardware verificados),
> [`2026-07-20-front-end-title-shell-design.md`](2026-07-20-front-end-title-shell-design.md)
> (Pieza 1, ya en master).

## 0. Qué es

La secuencia a la que "Nueva partida" entrega el control: desde el camarote hasta
el overworld. La apertura (camarote a oscuras, narración con la luz subiendo) ya
está construida y commiteada; lo que falta es **todo lo que viene después**.

```
[hecho]  título -> vidrio -> camarote a oscuras + narración
[FALTA]  -> juego de la consola (dungeon crawler)
         -> muerte contra el jefe invencible
         -> marcador: PISO ALCANZADO vs RECORD PISO 07 CAR
         -> entrada del nombre, dentro de la ficción de la consola
         -> el capitán anuncia la isla
         -> overworld
```

## 1. La decisión central: shmup -> dungeon crawler

El prólogo era un shooter espacial. Pasa a ser un **dungeon crawler monocromo de
Game Boy**. Razones, por orden de peso:

1. **El récord deja de ser una cifra y pasa a ser un lugar.** "PISO 07" responde a
   *¿hasta dónde llegó Carlos?*, no a *cuántos puntos hizo*. La isla entera va de
   descender y no volver; el juego de la consola lo ensaya.
2. **La derrota deja de oler a amaño.** En un shooter, perder a propósito es un
   truco. Aquí pierdes contra un **jefe invencible** — y eso *es* una regla del
   juego, no una trampa (ver §4).
3. **No hay coste hundido.** El shmup nunca tuvo lógica: cero enemigos, cero
   disparos, cero colisiones (verificado sobre `src/minigame_spaceship.c`). Solo
   existían 4 capas de parallax y una nave. Cambiar ahora cuesta assets, no código.
4. **Hay assets gratis y ya descargados** en el estilo correcto (§5).

## 2. Cambios obligados en el spec general

`2026-07-17-pokemon-phantom-design.md` fija el shooter en cuatro sitios. Los cuatro
tienen que cambiar de forma coherente, o el giro de Carlos se rompe:

| Línea | Dice hoy | Pasa a decir |
|---|---|---|
| 22 | "la consola portátil que compartían, con el récord de Carlos en el **shooter espacial**" | ...en el **dungeon crawler** |
| 42 (Día 1) | "máquina recreativa del bar (**shmup**, récord 'CAR' visible)" | máquina recreativa del bar (**el mismo dungeon crawler**, récord "PISO 07 — CAR" visible) |
| 49 (Día 2) | "encontrar la consola de Carlos con su récord recontextualiza todo el prólogo" | *(sin cambio de texto; el mecanismo se mantiene)* |
| 70 | Carlos presente solo en rastros: "récord 'CAR'" | *(sin cambio de texto)* |

**El mecanismo narrativo que hay que preservar intacto:** juegas en el prólogo ->
en el bar del Día 1 reconoces **la misma máquina** con "CAR" -> en el Día 2
encuentras la consola de Carlos y entiendes que llevabas toda la partida jugando
con su fantasma.

## 3. El juego de la consola

Un dungeon crawler ficticio: **SIMA**.

Una *sima* es una cavidad vertical en la roca — un agujero por el que se baja. El
nombre describe la forma del juego (pisos que descienden) y es la isla en
miniatura: un sitio al que se entra hacia abajo y del que no se vuelve. En lo
práctico son cuatro letras, legibles en un logo gordo de Game Boy, y una palabra
poco común, así que suena a cartucho real. No se reutiliza "Super Gameboy Quest"
(el nombre del pack) para no arrastrar una marca ajena dentro de la ficción.

### Estructura

- **Un piso = una pantalla fija.** 240÷16 = 15 y 160÷16 = 10, así que con los tiles
  de 16×16 del pack **una pantalla es exactamente una rejilla de 15×10**. Sin
  scroll, sin cámara, sin generación procedural: salas dibujadas a mano.
- **Tiempo real.** Movimiento libre en 4 direcciones, no por turnos.
- **Ataque.** El jugador tiene arma (el pack trae `weapons_animated.png` y
  `projectiles_animated.png`).
- **Daño por contacto** con los enemigos. Vida corta (3 corazones).
- **Escalera** en cada piso: llegar a ella baja al siguiente.
- **Tres pisos.** 1 y 2 jugables y justos; en el 3 espera el jefe.

### Enemigos

- Pisos 1-2: **rata, murciélago y slime** del pack de mazmorra (son los únicos con
  animación de ataque y muerte, no solo idle/move/hit).
- Variedad opcional si sobra presupuesto: goblin, esqueleto, araña, fantasma.

## 4. El jefe invencible

En el piso 3 aparece el **Slime King**. **No se puede matar.** Le pegas, parpadea
(el pack trae los frames claros de "golpeado"), y no baja de vida. Te mata.

Esto no es dificultad inflada: es la primera vez que el juego enuncia la regla que
después rige Sombraluna. El spec general ya la tiene escrita para los combates de
la isla:

> *"2-3 **imposibles** ... huir es la respuesta correcta, y **el juego nunca te debe
> una pelea justa**"*

El juego de la consola se la enseña al jugador **antes** que la isla. Y como no hay
habilidad que valga, **ningún jugador puede romper el guion** llegando al piso 7.

Queda deliberadamente sin responder: *¿a Carlos lo mató el mismo jefe, o llegó al 7
porque encontró cómo huir?* No se contesta nunca (regla de la casa: dread al
principio, explicación al final — y esta no se explica).

## 5. Assets

### Origen y licencias

| Pack | Uso | Licencia | Riesgo |
|---|---|---|---|
| [SGQ Dungeons](https://toadzillart.itch.io/dungeons-pack) | suelos, muros, props, rata/murciélago/slime, armas, proyectiles | Uso comercial y **adaptación permitidas** con crédito + enlace | bajo |
| [SGQ Monsters](https://toadzillart.itch.io/monster-pack) | Slime King, enemigos extra | Uso comercial y **adaptación permitidas** con crédito + enlace (ver nota) | bajo |
| [SGQ UI](https://toadzillart.itch.io/ui-pack) | HUD, iconos, elementos de marcador | Uso comercial y **adaptación permitidas** con crédito + enlace | bajo |

> **Nota sobre el pack de monstruos.** Su campo de metadatos en itch.io dice
> `CC BY-ND 4.0` (*sin derivados*), pero el **texto del propio autor en la
> descripción concede explícitamente** *"Adapt and/or modify the assets to match
> your project"*, igual que los otros dos packs. Es una contradicción de la página,
> no del permiso: manda la concesión expresa del titular. Se documenta aquí por si
> el proyecto dejara de ser gratuito algún día, en cuyo caso conviene pedirle al
> autor una confirmación por escrito.

Los tres packs prohíben lo mismo: **redistribuir o vender los assets directamente
(aun modificados)**, atribuirse su autoría, y usarlos en proyectos cripto/NFT.
Integrarlos convertidos dentro de la ROM no es redistribuirlos; publicar el
`graphics/` con los PNG sueltos se acercaría más a la línea.

**Atribución obligatoria** en `CREDITS.md`, con enlace a cada pack. La entrada
actual de *Classic Shmups* (CC BY-SA 4.0) se retira si dejan de usarse esos assets.

### Trabajo de conversión (verificado sobre los ficheros descargados)

Medido sobre los 32 PNG descargados:

- **Todos son RGBA.** `gbagfx` exige PNG **indexado** con paleta
  (`tools/gbagfx/convert_png.c:146`). Hay que reindexar **todos**. Es el único
  trabajo obligatorio de conversión, y nada compila hasta que exista.
- **Los 32 ficheros comparten UNA sola paleta de 4 colores.** 29 usan exactamente
  estos cuatro y los otros 3 (los iconos y el teclado) usan subconjuntos estrictos:

  | Índice | RGB | Rol |
  |---|---|---|
  | 0 | — | transparente |
  | 1 | `(88, 68, 34)` | marrón oscuro |
  | 2 | `(94, 133, 73)` | verde medio |
  | 3 | `(120, 164, 106)` | verde claro |
  | 4 | `(212, 210, 155)` | crema |

- **Cero píxeles semitransparentes** en todo el conjunto: sin antialias ni bordes
  suaves. La transparencia ya es binaria, que es justo lo que la GBA soporta (no
  hay alfa por píxel). No hay que aplanar nada.

**Consecuencias.** El reindexado es una **tabla de conversión fija de cuatro
entradas** — sin cuantización, sin dithering, sin emparejado de colores; es
determinista y no puede dar sorpresas entre ficheros. Y en tiempo de ejecución basta
**una paleta para todos los tiles de mazmorra y otra para todos los sprites**
(jugador, los doce enemigos, el jefe, proyectiles y HUD), dejando 15 de 16 paletas
de BG y 15 de 16 de OBJ libres.

## 6. Restricciones técnicas resueltas

- **El Slime King es 48×48, que NO es un tamaño de sprite legal en GBA** (los
  cuadrados son 8/16/32/64). Se mete en un sprite de **64×64 con relleno
  transparente**: cuesta 64 tiles (2 KB) por frame, así que 4 frames de jefe gastan
  256 de los 1024 tiles de VRAM de OBJ. Asumible porque cuando el jefe está en
  pantalla casi no hay nada más. La alternativa (componerlo con 4 sprites de
  32×32+32×16+16×32+16×16 = 36 tiles/frame) ahorra VRAM pero gasta 4 slots de OAM y
  bastante más código; se descarta salvo que la VRAM apriete.
- **`MAX_SPRITES = 64`** (límite de pokeemerald, no del hardware). El presupuesto lo
  comparten jugador, enemigos, proyectiles y HUD.
- **Tiles de 16×16** = 2×2 tiles de GBA. La rejilla cuadra sin ajustes.
- **Fondos:** un piso es un tilemap de 15×10 celdas de 16px. Muy por debajo de los
  512 tiles por bloque.

## 7. Marcador y entrada del nombre

Al morir:

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

- **La entrada de nombre es propia, en 2 tonos verdes**, dentro de la ficción de la
  consola. **No** se usa `DoNamingScreen` de pokeemerald: es la pantalla colorida de
  Pokémon y aparecería en mitad de un juego monocromo de Game Boy, rompiendo la
  ilusión justo en el momento clave.
- El momento de nombrarte es el momento en que **te ves escrito al lado de tu
  hermano**. Eso es el diseño, no un trámite.
- **El nombre escrito aquí es el nombre de la partida**: se copia a
  `gSaveBlock2Ptr->playerName` (`PLAYER_NAME_LENGTH = 7`). Esto **resuelve la tensión
  pendiente** del andamiaje de New Game, que hoy fija el nombre a `"?"`.
- "CAR" no es una convención de 3 letras impuesta por el juego (el campo admite 7):
  es **lo que Carlos escribió**. Un rastro suyo, no un formato.

## 8. Fuera de alcance

- La máquina recreativa del bar (Día 1) y la consola de Carlos (Día 2): usan este
  mismo juego, pero son contenido de sus propios actos.
- Rehacer el shmup. `src/minigame_spaceship.c`, `src/minigame_countdown.c` y
  `graphics/minigame_spaceship/` (salvo `boy.*`, que es el camarote y se queda) se
  retiran cuando el crawler funcione, no antes.
- La escena del capitán es parte de esta pieza, pero su guion se escribe al final,
  cuando el marcador esté en pie.

## 9. Verificación

Sin suite de tests: la verificación es build limpio + `./test/smoke.sh` + revisión
visual en mGBA de las tres rutas (sin save, con save+Nueva, con save+Continuar).
Cada tarea del plan de implementación debe terminar con los tres builds sanos
(`modern`, `PHANTOM_TEST=1`, `PHANTOM_DEBUG_BOOT=1`).

**Riesgo mayor:** la conversión de assets. Los 31 PNG vienen en RGBA y `gbagfx`
exige indexado, así que **nada compila hasta que exista el paso de reindexado**.
Conviene resolverlo en la primera tarea del plan, con un script determinista y
versionado (el proyecto ya tiene ese patrón en `graphics/phantom_intro/gen.py`),
no a mano fichero por fichero.
