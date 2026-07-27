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

1. **Anclaje** (REVISADO — el primer intento, "sobre el jugador", se probó
   jugando y el dueño lo rechazó: tapaba el sprite del jugador, se veía mal).
   La espada se dibuja centrada en la **casilla ADYACENTE en la dirección de
   mirada** — la misma casilla que ya amenaza el daño
   (`SimaActors_WeaponHitbox`) — pegada al jugador pero **sin solaparlo**.
   `SimaActors_WeaponHitbox` no cambia de geometría; ahora se reutiliza
   también para POSICIONAR el sprite, no solo para calcular el daño.
   - `SimaActors_WeaponHitbox(sAttackFacing, sPlayerX, sPlayerY, &hitX, &hitY)`
     da la esquina superior izquierda de la casilla objetivo, en píxeles.
     Como la función solo desplaza en X (vista de perfil pura: el objetivo
     siempre está en la misma fila que el jugador), `hitY == sPlayerY`
     siempre.
   - Anclaje vertical (NÚMERO DE GUSTO, probado jugando): base del arma
     alineada con la base de la casilla objetivo — los 16px inferiores del
     arma "de pie" ocupan esa casilla, los 16px superiores quedan en la fila
     de arriba (espada alzada). Mismo esquema vertical que el intento
     anterior, solo cambia la columna sobre la que se centra.
     top-left del arma = `(hitX, hitY - 16)` → centro para `CreateSprite`
     = `(hitX + 8, hitY)`.
   - Verificado contra el piso 1 real (fila y=6, sin muro en la fila de
     arriba en las casillas donde golpean los enemigos): no tapa ninguna
     fila de muro de forma fea. Si un piso futuro coloca muro justo encima
     de una casilla golpeable, revisar este offset.
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
- `CreateSprite` del arma (InitPlayer): posición inicial arbitraria (columna
  del jugador), invisible hasta el primer golpe — `UpdateAttack` la recoloca
  cada frame de golpe.
- `UpdateAttack`:
  - Posición anclada a la casilla adyacente (`SimaActors_WeaponHitbox(sAttackFacing,
    sPlayerX, sPlayerY, &hitX, &hitY)`, luego `x=hitX+8, y=hitY`) — REVISADO,
    ver decisión 1 actualizada arriba. `SimaActors_WeaponHitbox` en sí no
    cambió de geometría, ahora se llama también desde `UpdateAttack` (antes
    solo desde `UpdateEnemies` para el daño).
  - `hFlip = (sAttackFacing == SIMA_FACING_RIGHT)` (invierte la lógica actual).
  - Selección de frame: windup → `RAISED`; dentro del activo, índice relativo
    `0..3` → `ARC1,ARC2,ARC3,REST` (clamp al último); recuperación → oculto.
- Reescribir el bloque de comentario grande (~173–215) para documentar la
  espada sostenida en vez del arco.

## Sin cambios
- `SimaActors_WeaponHitbox` — geometría de daño intacta (mismos valores LEFT/RIGHT
  que antes); ahora tiene DOS llamadores (`UpdateAttack` para posicionar,
  `UpdateEnemies` para el daño) en vez de uno.
- `src/phantom_test.c` — `Test_SimaWeaponHitbox` sigue verde (LEFT→84, RIGHT→116).
- Paleta compartida `TAG_SIMA_PLAYER`.

## Riesgo a validar al correr (RESUELTO)
Orden de dibujo espada↔jugador cuando se solapan (subpriority): con el
anclaje revisado (decisión 1) la espada ya NO se solapa con el jugador —
vive en la casilla de al lado — así que este riesgo dejó de aplicar.

## Verificación
- `make PHANTOM_TEST=1 modern` compila; el harness in-ROM pasa (hitbox intacto).
- Inspección visual del golpe a izquierda y derecha en el minijuego: la espada
  debe verse en la casilla de al lado, sin tapar al jugador.
