# Límites gráficos de la GBA — target para elegir assets

> Referencia práctica para comprar/descargar arte (itch.io y similares) que entre
> en el juego sin rehacerlo. Cifras verificadas contra este repo (`include/gba/defines.h`,
> `include/gba/types.h`, `include/sprite.h`, `tools/gbagfx/convert_png.c`), no de memoria.

## Lo que el pipeline EXIGE del PNG

Esto es lo primero que hay que mirar en cualquier pack, y lo que más packs incumple:

| Requisito | Valor | Dónde se comprueba |
|---|---|---|
| **PNG indexado (con paleta)** | obligatorio | `convert_png.c:146` — *"does not contain a palette"* |
| Profundidad de bits | 1, 2, 4 u 8 | `convert_png.c:130` |
| Dimensiones | múltiplo de 8 px | los tiles son 8×8 |

**El 90% de los packs de itch.io vienen en RGBA**, no indexados. Eso **no compila**:
hay que reindexarlos antes (con ImageMagick, Aseprite o un script). No es un
impedimento para comprar un pack, pero sí un paso obligatorio.

El **color 0 de la paleta es la transparencia**. En los assets actuales del shmup
ese color es rojo puro `(255,0,0)` — el rojo que se ve en `bg_*.png` no es arte,
es el fondo transparente.

## Color

- **15 bits, BGR555**: 32 niveles por canal, no 256. Los degradados suaves de un PNG
  se cuantizan y salen a bandas. El arte con pocos colores planos sobrevive intacto;
  el arte con sombreado fino, no.
- **16 paletas de BG + 16 paletas de OBJ**, de 16 colores cada una (`BG_PLTT_SIZE`
  y `OBJ_PLTT_SIZE` = `0x200` = 256 colores cada bloque).
- A **4bpp** (lo normal) cada tile o sprite usa **una** paleta de 16 colores, y uno
  de esos 16 se gasta en la transparencia → **15 colores útiles**.
- 8bpp (256 colores) existe, pero gasta el doble de VRAM y se come el presupuesto.

## Sprites (OBJ)

**Solo existen 12 tamaños.** Cualquier otro hay que trocearlo en varios sprites
(es lo que hace la grieta del título: una imagen a pantalla completa partida en
una rejilla de 12 sprites de 64×64).

| Cuadrados | Anchos | Altos |
|---|---|---|
| 8×8 · 16×16 · 32×32 · **64×64** | 16×8 · 32×8 · 32×16 · 64×32 | 8×16 · 8×32 · 16×32 · 32×64 |

- **64×64 es el máximo absoluto.** No hay sprites más grandes.
- **`MAX_SPRITES = 64`** (`include/sprite.h:5`). Ojo: es límite **de pokeemerald**,
  no del hardware (la GBA soporta 128). Ese presupuesto lo comparten jugador,
  enemigos, disparos, explosiones y cualquier overlay de UI.
- **VRAM de OBJ: 32 KB** (`OBJ_VRAM0_SIZE 0x8000`) = **1024 tiles** de 4bpp.
- Hay además un límite **por línea de barrido**: demasiados sprites en la misma
  fila y los últimos no se dibujan. Un shmup con muchos disparos simultáneos lo
  toca antes de agotar los 64.

## Fondos (BG)

- **VRAM de BG: 64 KB** (`BG_VRAM_SIZE 0x10000`), repartida entre bloques de tiles
  y de mapa.
- **Bloque de tiles: 16 KB** (`BG_CHAR_SIZE 0x4000`) = **512 tiles** de 4bpp.
- **Bloque de mapa: 2 KB** (`BG_SCREEN_SIZE 0x800`) = un tilemap de 32×32.
- **Modo 0: 4 capas** de texto independientes, cada una con scroll propio. Es el
  modo que usan tanto el título como el minijuego (4 capas de parallax).
- Los tiles son **8×8** y **se deduplican**: dos tiles idénticos ocupan uno solo, y
  el tilemap puede voltearlos en horizontal/vertical y asignarles paleta por tile.

**Consecuencia práctica:** el arte de fondo tiene que ser *repetitivo y alineado a
la rejilla de 8×8*. Un fondo tipo ilustración, con ruido o textura fotográfica,
revienta el presupuesto de tiles. Un fondo con formas planas que se repiten, no.

## Presupuesto real, con lo que ya hay

Del minijuego actual (`graphics/minigame_spaceship/`), tamaños de los `.4bpp` ÷ 32
bytes por tile:

| Asset | Tiles | Nota |
|---|---|---|
| `bg_0` | 80 | capa de parallax |
| `bg_1` | 160 | capa de parallax |
| `bg_2` | 96 | capa de parallax |
| `bg_3` | 48 | estrellas/partículas |
| **total fondos** | **384** | de 512 por bloque |
| `player_ship` | 16 | 4 frames de 16×16 |
| `boy` | 176 | la imagen del camarote |

O sea: los fondos ya se comen **384 de 512 tiles**. Queda sitio, pero no de sobra —
un pack con fondos más detallados obliga a recortar capas.

## Qué géneros entran bien

Ordenados por coste de implementación, no por gusto:

- **Muy barato** — puzzle de rejilla, memoria, ritmo/timing de un botón, "atrapa lo
  que cae". Pocos sprites, tilemap estático, lógica trivial.
- **Medio** — shoot 'em up (el actual), runner de scroll lateral, breakout. Necesitan
  colisiones y aparición de entidades, pero nada exótico.
- **Caro** — plataformas (colisión con el tilemap, física), cualquier cosa con
  rotación o escalado (obliga a modo 1/2 y sprites afines), 3D falso tipo Mode 7.

**Restricción narrativa, que manda sobre todas las anteriores:** el minijuego es
una consola portátil que sostiene un chico en un camarote. Debe *parecer* un juego
de portátil de los 90. El arte que ya tienes es **monocromo verde de dos tonos**
(look Game Boy/DMG) y eso es un acierto: refuerza la ficción y además es lo más
barato que existe en tiles y paletas.

Al buscar packs, los términos que devuelven arte compatible son
**"Game Boy"**, **"DMG"**, **"1-bit"**, **"2-bit"** y **"demake"** — suelen venir ya
con 2-4 colores planos, alineados a 8×8, que es exactamente el target.
