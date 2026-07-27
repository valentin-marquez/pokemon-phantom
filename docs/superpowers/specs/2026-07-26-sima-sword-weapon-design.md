# SIMA — espada 16×32 sostenida por el jugador

Fecha: 2026-07-26
Ámbito: minijuego dungeon crawler (SIMA), sprite del arma del jugador.

## Motivación

El arma principal del prota es una **espada**. Hasta ahora el golpe se dibujaba
como un **arco de tajo de 16×16 centrado sobre la casilla objetivo** — una
solución que se adoptó para desambiguar 4 direcciones diagonales. Con la vista
de perfil pura (el prota solo mira izquierda/derecha), esa ambigüedad ya no
existe: una espada en horizontal se lee sin problema. El dueño del proyecto
dibujó frames propios de la espada (por ahora solo mirando **izquierda**) y hay
que integrarlos reemplazando el arco actual.

El arte viene en frames de **16×32** (el jugador es 16×16), lo que introduce el
problema central de este spec: **cómo alinear el sprite alto respecto al
jugador**.

## Arte de origen

`~/Descargas/SGQ_Dungeon/weapons_and_projectiles/weapon_left_{0..4}.png`
— 5 frames de 16×32, RGBA. Verificado: usan **exactamente** los 4 colores de
la paleta compartida de SIMA (marrón oscuro, verde medio, verde claro, crema),
sin alfa parcial. Por eso **no necesita paleta propia**: reindexa limpio en
`TAG_SIMA_PLAYER`, igual que el resto de sprites de SIMA.

Lectura de los frames (secuencia del mandoble descendente):
- `0` — espada alzada (windup / telégrafo).
- `1,2,3` — arco de tajo verde barriendo hacia abajo (impacto).
- `4` — reposo / follow-through.

## Decisiones (fijadas con el dueño)

1. **Anclaje**: sobre el jugador, base abajo. Los 16px inferiores del arma tapan
   la casilla del jugador; los 16px superiores quedan en la fila de arriba
   (espada alzada). El arte ya se inclina a la izquierda y "alcanza" la casilla
   objetivo por su propia silueta, así que no se desplaza hacia el objetivo.
   - top-left del arma = `(sPlayerX, sPlayerY - 16)` → centro para `CreateSprite`
     = `(sPlayerX + 8, sPlayerY)`.
2. **Secuencia de frames** sobre la cadencia existente (windup 3f / activo 4f /
   recuperación 9f): windup → frame 0; activo (4 frames) → frames 1,2,3,4;
   recuperación → oculto. La ventana de daño (`AttackHitboxActive`, timers 4–7)
   **no se toca**.
3. **Derecha**: HFLIP del arte izquierdo (arte nativo mira izquierda →
   `hFlip = (sAttackFacing == SIMA_FACING_RIGHT)`). El juego no se rompe al
   atacar a la derecha durante la prueba, aunque el arte "de verdad" de derecha
   se dibuje más adelante.

## Cambios

### 1. `graphics/sima/gen.py` — `generate_weapon()`
Reescribir para reindexar los 5 `weapon_left_{0..4}.png` con el `reindex()`
existente y apilarlos **verticalmente** en `weapon.png` de **16×160** (frame 0
arriba → frame 4 abajo). `reindex()` ya aborta el build ante cualquier color
fuera de la paleta o alfa parcial (garantía que se mantiene). Deja de depender
de `weapons.png`/`WEAPON_ARC_CELLS`.

### 2. `graphics_file_rules.mk` — regla `weapon.4bpp`
`-mheight 2` → `-mheight 4` (metatiles de 16×32 = 8 tiles/frame consecutivos, en
orden OBJ 1D). `-mwidth 2` se mantiene. Actualizar el comentario de la regla.

### 3. `src/sima_actors.c`
- `sOam_SimaWeapon`: `SPRITE_SHAPE/SIZE(16x16)` → `(16x32)`.
- Constantes: `WEAPON_TILES_PER_FRAME` 4→8, `WEAPON_SHEET_FRAMES` 4→5. Sustituir
  `FRAME_WEAPON_VERT/HORIZ_*` por `FRAME_WEAPON_RAISED/ARC1/ARC2/ARC3/REST`
  (`i * WEAPON_TILES_PER_FRAME`).
- `CreateSprite` del arma (InitPlayer): centro `(sPlayerX + 8, sPlayerY)`.
- `UpdateAttack`:
  - Posición anclada al jugador (`x=sPlayerX+8, y=sPlayerY`); ya no usa
    `SimaActors_WeaponHitbox` para posicionar (esa función sigue existiendo, la
    usa `UpdateEnemies` para el daño).
  - `hFlip = (sAttackFacing == SIMA_FACING_RIGHT)` (invierte la lógica actual).
  - Selección de frame: windup → `RAISED`; dentro del activo, índice relativo
    `0..3` → `ARC1,ARC2,ARC3,REST` (clamp al último); recuperación → oculto.
- Reescribir el bloque de comentario grande (~173–215) para documentar la
  espada sostenida en vez del arco.

## Sin cambios
- `SimaActors_WeaponHitbox` — geometría de daño intacta.
- `src/phantom_test.c` — `Test_SimaWeaponHitbox` sigue verde (LEFT→84, RIGHT→116).
- Paleta compartida `TAG_SIMA_PLAYER`.

## Riesgo a validar al correr
Orden de dibujo espada↔jugador cuando se solapan (subpriority). Si la espada
queda por detrás del prota, ajustar subpriority del sprite del arma.

## Verificación
- `make PHANTOM_TEST=1 modern` compila; el harness in-ROM pasa (hitbox intacto).
- Inspección visual del golpe a izquierda y derecha en el minijuego.
