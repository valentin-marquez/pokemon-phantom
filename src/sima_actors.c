#include "global.h"
#include "sima.h"
#include "sima_rooms.h"
#include "sprite.h"
#include "decompress.h"
#include "main.h"

// Jugador de SIMA: dungeon crawler POR TURNOS (encargo del dueño del
// proyecto, sustituye a la version en tiempo real de la Tarea 4). La regla
// es simple y sin excepciones: pulsas una direccion, el jugador se desliza
// UNA casilla, y SOLO CUANDO ese deslizamiento termina les toca mover a los
// enemigos -- uno por uno hacia el jugador, tambien deslizandose. Sin
// deslizamiento en marcha (ni del jugador ni de los enemigos), nada se
// mueve: el jugador puede pensar indefinidamente entre turnos. Atacar
// (Tarea 7) tambien consume turno: golpeas, y despues de que termine la
// animacion del golpe (windup+activo+recuperacion) les toca a los enemigos.
//
// Maquina de estados del turno (enum SimaTurnPhase, mas abajo):
//   PLAYER_INPUT -> (direccion valida) -> PLAYER_MOVE -> ENEMY_STEP -> PLAYER_INPUT
//   PLAYER_INPUT -> (A)                -> PLAYER_ATTACK -> ENEMY_STEP -> PLAYER_INPUT
//   PLAYER_INPUT -> (direccion bloqueada) -> se gira, sigue en PLAYER_INPUT (SIN turno)
// src/sima.c llama a SimaActors_UpdatePlayer y SimaActors_UpdateEnemies cada
// frame desde CB2_SimaMain; cual de las dos "hace algo" en un frame dado lo
// decide sTurnPhase, compartida por ambas porque viven en el mismo archivo.
//
// Colision: con movimiento por rejilla, SimaRoom_IsSolid sobre la casilla
// destino basta -- ya no hace falta la caja de 12x12 de la version en
// tiempo real (SimaActors_BoxFits/BoxesOverlap, eliminadas con esta tarea;
// sus tests tambien, ver el informe).

// player_walk.png (graphics/sima/gen.py) es una tira de 10 celdas de 16x16
// recortadas de player.png: 3 mirando abajo (cara), 3 mirando arriba (nuca)
// y 4 de perfil (fila 2 de player.png). Izquierda reutiliza las mismas 4
// celdas de perfil volteadas por OAM (oam.hFlip más abajo); no hay arte de
// perfil-izquierda por separado. graphics_file_rules.mk convierte esta hoja
// con -mwidth 2 -mheight 2, así que cada celda de 16x16 ocupa 4 tiles de
// hardware CONTIGUOS (a diferencia de tiles.4bpp, que es BG y usa el barrido
// raster de la hoja completa vía PlaceCell en src/sima.c): "celda i" empieza
// en el tile 4*i de la hoja.
static const u32 sPlayerGfx[] = INCBIN_U32("graphics/sima/player_walk.4bpp");
// Misma paleta única de SIMA que las celdas de sala (índice 0 transparente +
// 4 tonos; ver src/sima.c). Se vuelve a incluir aquí (en vez de compartir el
// array de sima.c) para que este archivo no dependa de símbolos internos de
// sima.c -- son los mismos 32 bytes, duplicarlos en el ROM es irrelevante.
static const u16 sPlayerPal[] = INCBIN_U16("graphics/sima/grounds.gbapal");

#define TAG_SIMA_PLAYER 0x6000

#define PLAYER_SHEET_FRAMES     10
#define PLAYER_TILES_PER_FRAME  4  // 16x16 = 2x2 tiles de hardware de 8x8

// Offsets de tile (en tiles de 4bpp, no en celdas) de cada frame dentro de
// la hoja, en el orden en que PLAYER_WALK_CELLS los empaqueta en gen.py.
#define FRAME_DOWN_IDLE  (0 * PLAYER_TILES_PER_FRAME)
#define FRAME_DOWN_STEP_A (1 * PLAYER_TILES_PER_FRAME)
#define FRAME_DOWN_STEP_B (2 * PLAYER_TILES_PER_FRAME)
#define FRAME_UP_IDLE    (3 * PLAYER_TILES_PER_FRAME)
#define FRAME_UP_STEP_A  (4 * PLAYER_TILES_PER_FRAME)
#define FRAME_UP_STEP_B  (5 * PLAYER_TILES_PER_FRAME)
#define FRAME_SIDE_IDLE  (6 * PLAYER_TILES_PER_FRAME)
#define FRAME_SIDE_STEP_A (7 * PLAYER_TILES_PER_FRAME)
#define FRAME_SIDE_STEP_B (8 * PLAYER_TILES_PER_FRAME)
#define FRAME_SIDE_STEP_C (9 * PLAYER_TILES_PER_FRAME)

// enum SimaFacing vive en include/sima.h desde la Tarea 7 (lo necesita
// SimaActors_WeaponHitbox, expuesta al harness).

static const struct OamData sOam_SimaPlayer = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 1,  // igual que BG0 (la sala): por delante de su fondo, por detrás del HUD (BG1, prioridad 0)
};

static const struct SpriteSheet sSheet_SimaPlayer = {
    sPlayerGfx, PLAYER_SHEET_FRAMES * PLAYER_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_PLAYER
};
static const struct SpritePalette sPal_SimaPlayer = { sPlayerPal, TAG_SIMA_PLAYER };

// anims/images sin usar a propósito (igual que las grietas y el menú de
// phantom_intro.c): el frame se elige a mano cada tick en
// UpdatePlayerSprite escribiendo oam.tileNum directamente sobre
// sheetTileStart, no hace falta el sistema de ANIMCMD para esto.
static const struct SpriteTemplate sTmpl_SimaPlayer = {
    .tileTag = TAG_SIMA_PLAYER,
    .paletteTag = TAG_SIMA_PLAYER,
    .oam = &sOam_SimaPlayer,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// ---------------------------------------------------------------------
// Arma del jugador (Tarea 7): sprite propio de 16x16, dos frames recortados
// de graphics/sima/weapons.png por graphics/sima/gen.py (generate_weapon) --
// esa hoja trae decenas de armas en rotaciones pensadas para un motor de 8
// direcciones con giro por software; no hay una celda limpia "mirando
// arriba/abajo/izquierda/derecha" para las 4 direcciones de SIMA, así que en
// vez de eso se recortaron 2 frames de UN solo mandoble en su única
// orientación diagonal (mango arriba-izquierda, punta abajo-derecha:
// FRAME_A liso, FRAME_B con el destello de impacto) y aquí se orienta con
// flips de OAM -- igual que el jugador reutiliza su frame de perfil para
// izquierda/derecha en vez de tener arte por separado.
//
// Reverificado en la tarea de sensación de movimiento (releyendo
// weapons.png celda a celda, con un script de componentes conexos en vez de
// a ojo): TODA la hoja (528x768, 33x48 celdas) es así -- dagas/espadas
// (filas 0-10), un shuriken/bumerán girando (filas 13-22, casi simétrico,
// no sirve para orientar), libros/pociones abriéndose (filas 24-38) y
// piquetas (filas 41-47) están cada una en una única familia de rotaciones
// diagonales de 3D-a-2D (animación de giro/lanzamiento), NINGUNA con una
// celda realmente horizontal o vertical. No hay arte cardinal que rescatar;
// el implementador anterior no se lo perdió, no existe. Ver
// UpdateAttack más abajo para la solución de flips + arco de barrido
// cosmético elegida en su lugar.
// ---------------------------------------------------------------------

static const u32 sWeaponGfx[] = INCBIN_U32("graphics/sima/weapon.4bpp");

#define TAG_SIMA_WEAPON 0x6004

#define WEAPON_SHEET_FRAMES    2
#define WEAPON_TILES_PER_FRAME 4  // 16x16 = 2x2 tiles de hardware, igual que jugador/enemigos
#define FRAME_WEAPON_A (0 * WEAPON_TILES_PER_FRAME)  // mandoble liso (windup)
#define FRAME_WEAPON_B (1 * WEAPON_TILES_PER_FRAME)  // mandoble + destello (impacto)

static const struct OamData sOam_SimaWeapon = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 1,  // misma capa que jugador/enemigos
};

static const struct SpriteSheet sSheet_SimaWeapon = {
    sWeaponGfx, WEAPON_SHEET_FRAMES * WEAPON_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_WEAPON
};

// paletteTag = TAG_SIMA_PLAYER a propósito, igual que los enemigos: misma
// paleta única de sprites de SIMA para todo (el pack de armas ya viene en
// los mismos 4 tonos, verificado al recortar weapon.png).
static const struct SpriteTemplate sTmpl_SimaWeapon = {
    .tileTag = TAG_SIMA_WEAPON,
    .paletteTag = TAG_SIMA_PLAYER,
    .oam = &sOam_SimaWeapon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// Cadencia del golpe (Tarea 7): windup -> activo -> recuperación, sumando
// ATTACK_TOTAL_FRAMES antes de que le toque el turno a los enemigos (ver
// StartEnemyTurn, más abajo -- con el cambio a turnos, ya no hay "cooldown"
// que gestionar: mientras sAttackTimer > 0 el jugador está en
// SIMA_TURN_PLAYER_ATTACK y no puede volver a pulsar A hasta que ese turno
// termine y vuelva a SIMA_TURN_PLAYER_INPUT). Números elegidos para que el
// golpe se LEA (el arma es visible unos frames antes de dañar, un telégrafo
// mínimo). Mismo espíritu que el resto de tiempos de esta tarea: "paso
// deliberado, no arcade".
#define ATTACK_WINDUP_FRAMES   3  // arma visible (FRAME_A), sin dañar todavía
#define ATTACK_ACTIVE_FRAMES   4  // arma visible (FRAME_B), caja de golpe activa
#define ATTACK_RECOVERY_FRAMES 9  // arma oculta, hasta que le toca el turno a los enemigos
#define ATTACK_TOTAL_FRAMES (ATTACK_WINDUP_FRAMES + ATTACK_ACTIVE_FRAMES + ATTACK_RECOVERY_FRAMES)

// NÚMERO DE GUSTO (mejora de sensación, ver UpdateAttack): desplazamiento en
// píxeles del arco de barrido cosmético del arma, perpendicular a la
// dirección del golpe. 3px es visible sin desalinear el arma de su casilla
// adyacente al punto de parecer flotando fuera de sitio.
#define ATTACK_SWEEP_PX 3

// Tamaño de una casilla de la rejilla de sala, en píxeles (ver SIMA_ROOM_W/H
// en sima_rooms.h). No vive ahí porque es un detalle de cómo se ANIMA el
// movimiento (deslizamiento en píxeles), no de la geometría de la sala.
#define SIMA_TILE_PX 16

// NÚMEROS DE GUSTO -- LOS TIEMPOS DEL TURNO (ajustables jugando, ver el
// informe de esta tarea). Antes (tiempo real) el jugador cruzaba una casilla
// en 8 frames a 2px/frame (PLAYER_SPEED); se mantiene exactamente esa
// cadencia de deslizamiento aquí (SIMA_PLAYER_SLIDE_FRAMES=8,
// SIMA_PLAYER_SLIDE_SPEED=16/8=2px/frame) para que el "paso" se siga
// sintiendo igual de decidido -- lo único que cambia es que ahora el
// deslizamiento SIEMPRE llega exactamente a la casilla siguiente (nunca se
// para a medias) y dispara el turno de los enemigos al terminar.
#define SIMA_PLAYER_SLIDE_FRAMES 8
#define SIMA_PLAYER_SLIDE_SPEED (SIMA_TILE_PX / SIMA_PLAYER_SLIDE_FRAMES)  // 2 px/frame

// Los enemigos se deslizan con la MISMA cadencia que el jugador a propósito:
// un turno se "lee" como dos deslizamientos simétricos (el tuyo, luego el
// suyo), no como el jugador rápido y los enemigos arrastrándose o al revés.
#define SIMA_ENEMY_SLIDE_FRAMES 8
#define SIMA_ENEMY_SLIDE_SPEED (SIMA_TILE_PX / SIMA_ENEMY_SLIDE_FRAMES)  // 2 px/frame

// Frames entre pasos del ciclo de caminata: con un deslizamiento de
// SIMA_PLAYER_SLIDE_FRAMES=8, esto da exactamente UN cambio de pose a mitad
// de casilla (dos poses distintas por casilla cruzada, igual que en tiempo
// real).
#define WALK_ANIM_PERIOD 4

// Nota: nunca inicializar estos estaticos con un valor no-cero inline (p.
// ej. "= MAX_SPRITES") -- el linker moderno de este repo descarta la
// seccion .data resultante (probado: rompe con "defined in discarded
// section .data"). sPlayerActive (BSS, arranca en FALSE) hace de guarda en
// vez de comparar sPlayerSpriteId contra un centinela.
static bool8 sPlayerActive;
static u8 sPlayerSpriteId;
static u8 sPlayerFloor;
static s16 sPlayerX;   // esquina superior izquierda del sprite, en píxeles de pantalla
static s16 sPlayerY;
static u8 sPlayerFacing;
static bool8 sPlayerMoving;
static u8 sPlayerAnimStep;    // 0/1: alterna entre los dos frames intermedios del paso
static u8 sPlayerAnimTimer;

// Vida y daño por contacto (Tarea 6). sPlayerHP arranca en 0 en .bss (misma
// regla de estáticos que el resto del archivo) pero SimaActors_InitPlayer lo
// fija a SIMA_PLAYER_MAX_HP antes de que nada pueda leerlo, así que nunca se
// observa ese 0 transitorio.
static u8 sPlayerHP;
// Parpadeo de "te acaban de golpear" (puramente visual, ver UpdatePlayerSprite).
// Con turnos, YA NO hace falta que esto bloquee nada -- lo que antes evitaba
// que el mismo contacto continuo hiciera daño varias veces seguidas ahora lo
// hace sPlayerHitThisTurn (un golpe como mucho por turno de enemigos, sin
// importar cuántos se solapen). Este timer solo cuenta cuántos frames sigue
// parpadeando el sprite.
static u8 sPlayerInvulnTimer;
#define SIMA_HIT_FLASH_FRAMES 20   // NÚMERO DE GUSTO: ~0.33s a 60Hz de parpadeo tras un golpe

// ---------------------------------------------------------------------
// Maquina de estados del turno. Compartida entre SimaActors_UpdatePlayer y
// SimaActors_UpdateEnemies (viven en el mismo archivo): cual de las dos hace
// algo en un frame dado depende de sTurnPhase. Arranca en
// SIMA_TURN_PLAYER_INPUT (valor 0, igual que el resto de estaticos de este
// archivo arrancan en su "reposo" via BSS) tanto por el orden del enum como
// por el reinicio explicito en SimaActors_InitPlayer/WarpToFloor.
// ---------------------------------------------------------------------
enum SimaTurnPhase
{
    SIMA_TURN_PLAYER_INPUT,   // esperando direccion o A; el jugador puede actuar
    SIMA_TURN_PLAYER_MOVE,    // jugador deslizandose a la casilla destino
    SIMA_TURN_PLAYER_ATTACK,  // golpe en curso (reusa sAttackTimer/UpdateAttack)
    SIMA_TURN_ENEMY_STEP,     // turno de los enemigos: se deslizan (y el jugador puede estar en pleno empujon)
};

static u8 sTurnPhase;

// Deslizamiento del jugador en curso (SIMA_TURN_PLAYER_MOVE). sPlayerSlideDX/DY
// es el delta POR FRAME (signo * SIMA_PLAYER_SLIDE_SPEED, siempre en un solo
// eje: no hay diagonales por turnos); sPlayerSlideTargetX/Y es la casilla de
// destino en píxeles, a la que se hace snap exacto en el último frame para
// no arrastrar redondeo.
static s16 sPlayerSlideDX;
static s16 sPlayerSlideDY;
static s16 sPlayerSlideTargetX;
static s16 sPlayerSlideTargetY;
static u8 sPlayerSlideTimer;

// Empujon al recibir daño (mejora de sensacion, pedida por el dueño del
// proyecto: "que lo haga retroceder o algo así"). Con el cambio a turnos:
// desplazamiento de UNA casilla en direccion contraria al enemigo que
// conectó, si esa casilla está libre -- si no, no se mueve (ver
// StartPlayerKnockback). Igual que el deslizamiento normal: delta por frame
// + destino en píxeles + cronómetro, con snap exacto al terminar.
static s16 sPlayerKnockbackDX;
static s16 sPlayerKnockbackDY;
static s16 sPlayerKnockbackTargetX;
static s16 sPlayerKnockbackTargetY;
static u8 sPlayerKnockbackTimer;
// NÚMERO DE GUSTO: más corto que un paso normal (4 frames < 8) para que el
// golpe se sienta más brusco/inmediato que un movimiento voluntario -- con
// SIMA_TILE_PX=16, eso da 16/4=4px/frame, el doble de rápido que el paso
// normal. DEBE ser <= SIMA_ENEMY_SLIDE_FRAMES (el empujón se resuelve
// siempre DENTRO del turno de los enemigos, nunca lo alarga).
#define SIMA_KNOCKBACK_SLIDE_FRAMES 4
#define SIMA_KNOCKBACK_SLIDE_SPEED (SIMA_TILE_PX / SIMA_KNOCKBACK_SLIDE_FRAMES)  // 4 px/frame

// Ataque (Tarea 7). sWeaponActive es la misma guarda de presupuesto de
// sprites que sPlayerActive (por si CreateSprite se queda sin hueco).
// sAttackTimer en 0 significa "sin golpe en curso"; 1..ATTACK_TOTAL_FRAMES
// mientras el golpe está en curso (ver UpdateAttack). sAttackFacing fija la
// dirección del golpe al iniciarlo, no la lee de sPlayerFacing cada frame:
// con turnos el jugador no puede girar a mitad de un golpe de todos modos
// (SIMA_TURN_PLAYER_ATTACK no lee input), pero fijarla explícita documenta
// la intención.
static bool8 sWeaponActive;
static u8 sWeaponSpriteId;
static u8 sAttackTimer;
static u8 sAttackFacing;

static void UpdatePlayerSprite(void);
static void UpdatePlayerInput(void);
static void UpdatePlayerSlide(void);
static void UpdateAttack(void);
static void AdvancePlayerKnockback(void);
static void StartPlayerKnockback(s8 enemyTileX, s8 enemyTileY);
static void StartEnemyTurn(void);
static void AdvanceEnemyStepPhase(void);

// Función pura (turnos): la casilla a la que el jugador se movería un paso
// desde (x, y) [casillas de sala, no píxeles] mirando `facing`. Separada del
// input y de los sprites para que el harness in-ROM (src/phantom_test.c)
// pueda ejercitarla sin pulsar nada, igual que antes hacía
// SimaActors_BoxFits -- pero ahora en casillas, no en una caja de píxeles:
// con movimiento por rejilla, SimaRoom_IsSolid sobre la casilla destino
// basta. Devuelve FALSE sin mover outX/outY (se quedan en la posición de
// partida) si la casilla destino está bloqueada -- ese es el caso "girarse
// es gratis, no consume turno" del brief: quien llama solo tiene que mirar
// el valor de retorno para decidir si arranca un deslizamiento o no.
bool8 SimaActors_PlayerStepTarget(u8 floor, s8 x, s8 y, u8 facing, s8 *outX, s8 *outY)
{
    s8 nx = x;
    s8 ny = y;

    switch (facing)
    {
    case SIMA_FACING_UP:
        ny--;
        break;
    case SIMA_FACING_DOWN:
        ny++;
        break;
    case SIMA_FACING_LEFT:
        nx--;
        break;
    case SIMA_FACING_RIGHT:
        nx++;
        break;
    }

    if (SimaRoom_IsSolid(floor, nx, ny))
    {
        *outX = x;
        *outY = y;
        return FALSE;
    }

    *outX = nx;
    *outY = ny;
    return TRUE;
}

// Función pura (Tarea 6): resta `amount` de `hp` saturando en 0. Separada de
// cualquier estado (recibe/devuelve el valor, no toca sPlayerHP) para que el
// harness in-ROM la ejercite sin sprites -- un underflow en u8 aquí daría
// 255 de vida y haría al jugador inmortal justo cuando debería morir.
u8 SimaActors_ApplyDamage(u8 hp, u8 amount)
{
    if (amount >= hp)
        return 0;
    return hp - amount;
}

u8 SimaActors_GetPlayerHP(void)
{
    return sPlayerHP;
}

bool8 SimaActors_IsPlayerDead(void)
{
    return sPlayerHP == 0;
}

// Pura, sin sprites: ¿está el turno del jugador completamente resuelto (de
// nuevo esperando input, con el sprite asentado exactamente en su casilla,
// sin deslizamiento ni golpe en curso)? src/sima.c la usa para no comprobar
// la escalera a mitad de un deslizamiento (podría "adelantarse" a la casilla
// de llegada antes de tiempo si se mirase el centro del sprite en pleno
// movimiento).
bool8 SimaActors_IsPlayerIdle(void)
{
    return sTurnPhase == SIMA_TURN_PLAYER_INPUT;
}

// Función pura (Tarea 7): casilla de 16x16 (esquina superior izquierda, en
// píxeles) que amenaza el arma cuando el jugador -- parado en (playerX,
// playerY) -- ataca mirando `facing`. Siempre la casilla ADYACENTE (un salto
// de 16px en el eje de la dirección), nunca la propia del jugador: así un
// golpe no puede autolesionar, y solo alcanza a un enemigo que esté de
// verdad delante, no a uno que solo comparta casilla por detrás o al lado.
// Separada de todo estado para que el harness in-ROM la ejercite sin
// sprites, igual que SimaActors_PlayerStepTarget.
void SimaActors_WeaponHitbox(u8 facing, s16 playerX, s16 playerY, s16 *outX, s16 *outY)
{
    s16 x = playerX;
    s16 y = playerY;

    switch (facing)
    {
    case SIMA_FACING_UP:
        y -= 16;
        break;
    case SIMA_FACING_DOWN:
        y += 16;
        break;
    case SIMA_FACING_LEFT:
        x -= 16;
        break;
    case SIMA_FACING_RIGHT:
        x += 16;
        break;
    }

    *outX = x;
    *outY = y;
}

void SimaActors_InitPlayer(u8 floor)
{
    s8 spawnX, spawnY;

    SimaRoom_GetSpawn(floor, &spawnX, &spawnY);

    sPlayerFloor = floor;
    sPlayerX = (s16)spawnX * SIMA_TILE_PX;
    sPlayerY = (s16)spawnY * SIMA_TILE_PX;
    sPlayerFacing = SIMA_FACING_DOWN;
    sPlayerMoving = FALSE;
    sPlayerAnimStep = 0;
    sPlayerAnimTimer = 0;
    sPlayerHP = SIMA_PLAYER_MAX_HP;   // vida solo se fija al montar el modo, no en cada piso (ver WarpToFloor)
    sPlayerInvulnTimer = 0;
    sAttackTimer = 0;   // sin golpe en curso (Tarea 7)
    sPlayerKnockbackTimer = 0;   // sin empujon en curso
    sPlayerSlideTimer = 0;       // sin deslizamiento en curso
    sTurnPhase = SIMA_TURN_PLAYER_INPUT;   // reinicio explicito -- ver la nota de sCurrentFloor en src/sima.c

    LoadSpriteSheet(&sSheet_SimaPlayer);
    LoadSpritePalette(&sPal_SimaPlayer);

    // CreateSprite posiciona por el CENTRO del sprite, no por la esquina
    // superior izquierda (ver CalcCenterToCornerVec en src/sprite.c): +8 en
    // cada eje porque el sprite es 16x16.
    sPlayerSpriteId = CreateSprite(&sTmpl_SimaPlayer, sPlayerX + 8, sPlayerY + 8, 0);
    sPlayerActive = (sPlayerSpriteId != MAX_SPRITES);

    if (sPlayerActive)
        UpdatePlayerSprite();

    // Arma (Tarea 7): un sprite más, creado una sola vez aquí (mismo motivo
    // que sPlayerActive/sEnemyAlive -- LoadSpriteSheet no es idempotente,
    // ver la nota de cabecera del archivo). Arranca invisible: solo se
    // muestra durante windup/activo de un golpe (ver UpdateAttack). Con
    // jugador + 3 enemigos + arma van 5 de los 64 sprites de MAX_SPRITES.
    LoadSpriteSheet(&sSheet_SimaWeapon);
    // Paleta ya cargada arriba (sPal_SimaPlayer); LoadSpritePalette es
    // idempotente por tag, pero el arma ni la vuelve a pedir -- reutiliza la
    // misma carga del jugador via paletteTag en sTmpl_SimaWeapon.
    sWeaponSpriteId = CreateSprite(&sTmpl_SimaWeapon, sPlayerX + 8, sPlayerY + 8, 0);
    sWeaponActive = (sWeaponSpriteId != MAX_SPRITES);
    if (sWeaponActive)
        gSprites[sWeaponSpriteId].invisible = TRUE;
}

void SimaActors_WarpToFloor(u8 floor)
{
    s8 spawnX, spawnY;

    if (!sPlayerActive)
        return;  // sin sprite que recolocar (ver el guard de SimaActors_UpdatePlayer)

    SimaRoom_GetSpawn(floor, &spawnX, &spawnY);

    // Mismo estado que fija SimaActors_InitPlayer, salvo que aqui NO se toca
    // el sprite (sheet/paleta/CreateSprite): se reutiliza el que ya existe,
    // solo se le cambian piso/posicion/facing y se sincroniza con
    // UpdatePlayerSprite.
    sPlayerFloor = floor;
    sPlayerX = (s16)spawnX * SIMA_TILE_PX;
    sPlayerY = (s16)spawnY * SIMA_TILE_PX;
    sPlayerFacing = SIMA_FACING_DOWN;
    sPlayerMoving = FALSE;
    sPlayerAnimStep = 0;
    sPlayerAnimTimer = 0;
    // sPlayerHP NO se resetea aquí a propósito: la vida es del intento, no
    // del piso -- bajar un piso con un corazón no debería devolverte los
    // otros dos. El parpadeo de golpe sí se corta: no tiene sentido
    // arrastrar frames de "acabo de recibir un golpe" al piso nuevo.
    sPlayerInvulnTimer = 0;
    // Golpe/deslizamiento/empujon en curso tampoco se arrastran al piso
    // nuevo, misma razon: aparecer en el spawn nuevo a mitad de una
    // animacion de un piso distinto seria confuso. El turno vuelve siempre a
    // PLAYER_INPUT.
    sAttackTimer = 0;
    sPlayerSlideTimer = 0;
    sPlayerKnockbackTimer = 0;
    sTurnPhase = SIMA_TURN_PLAYER_INPUT;
    if (sWeaponActive)
        gSprites[sWeaponSpriteId].invisible = TRUE;

    UpdatePlayerSprite();
}

// Despacha segun la fase del turno: solo UNA de estas ramas hace algo en un
// frame dado. Esto es, literalmente, "nada se mueve si tu no te mueves": en
// SIMA_TURN_PLAYER_INPUT, si no hay input valido, esta funcion no toca
// sPlayerX/sPlayerY en absoluto (ver UpdatePlayerInput).
void SimaActors_UpdatePlayer(void)
{
    if (!sPlayerActive)
        return;  // SimaActors_InitPlayer no se llamó, o CreateSprite se quedó sin presupuesto (MAX_SPRITES)

    switch (sTurnPhase)
    {
    case SIMA_TURN_PLAYER_INPUT:
        UpdatePlayerInput();
        break;
    case SIMA_TURN_PLAYER_MOVE:
        UpdatePlayerSlide();
        break;
    case SIMA_TURN_PLAYER_ATTACK:
        UpdateAttack();
        UpdatePlayerSprite();
        break;
    case SIMA_TURN_ENEMY_STEP:
        // El jugador no lee input durante el turno de los enemigos, pero SI
        // puede estar en pleno empujon si uno de ellos acaba de conectar
        // (ver StartPlayerKnockback, llamada desde StartEnemyTurn -- misma
        // fase, mismo archivo). AdvancePlayerKnockback no hace nada si no
        // hay empujon en marcha.
        AdvancePlayerKnockback();
        UpdatePlayerSprite();
        break;
    }
}

// SIMA_TURN_PLAYER_INPUT: lee A (ataque) y D-pad (movimiento). JOY_HELD para
// el D-pad, no JOY_NEW -- decision deliberada: mantener pulsada una
// direccion camina turno tras turno a su cadencia natural (cada turno solo
// avanza cuando el anterior termina del todo, asi que HELD no puede "colar"
// un segundo paso a mitad de uno ya en marcha), en vez de exigir soltar y
// volver a pulsar por cada casilla. A si sigue usando JOY_NEW (igual que
// antes de esta tarea): mantenerlo pulsado no encadena golpes.
static void UpdatePlayerInput(void)
{
    u8 newFacing;
    s8 curX, curY, nextX, nextY;

    if (JOY_NEW(A_BUTTON))
    {
        sAttackFacing = sPlayerFacing;
        sAttackTimer = 1;
        sTurnPhase = SIMA_TURN_PLAYER_ATTACK;
        UpdateAttack();
        UpdatePlayerSprite();
        return;
    }

    if (JOY_HELD(DPAD_UP))
        newFacing = SIMA_FACING_UP;
    else if (JOY_HELD(DPAD_DOWN))
        newFacing = SIMA_FACING_DOWN;
    else if (JOY_HELD(DPAD_LEFT))
        newFacing = SIMA_FACING_LEFT;
    else if (JOY_HELD(DPAD_RIGHT))
        newFacing = SIMA_FACING_RIGHT;
    else
    {
        // Nada pulsado: nadie se mueve (ni siquiera se gira). Es el caso que
        // demuestra que el turno funciona -- ver la verificacion por
        // memoria del informe de esta tarea.
        sPlayerMoving = FALSE;
        UpdatePlayerSprite();
        return;
    }

    sPlayerFacing = newFacing;   // girar es gratis, pase lo que pase con el paso (ver mas abajo)

    curX = (s8)(sPlayerX / SIMA_TILE_PX);
    curY = (s8)(sPlayerY / SIMA_TILE_PX);

    if (!SimaActors_PlayerStepTarget(sPlayerFloor, curX, curY, newFacing, &nextX, &nextY))
    {
        // Casilla bloqueada: el jugador ya se giro (arriba) pero NO arranca
        // un deslizamiento y sTurnPhase se queda en PLAYER_INPUT -- esto es,
        // literalmente, "girarse es gratis, no consume turno" del brief.
        sPlayerMoving = FALSE;
        UpdatePlayerSprite();
        return;
    }

    sPlayerSlideDX = (s16)(nextX - curX) * SIMA_PLAYER_SLIDE_SPEED;
    sPlayerSlideDY = (s16)(nextY - curY) * SIMA_PLAYER_SLIDE_SPEED;
    sPlayerSlideTargetX = (s16)nextX * SIMA_TILE_PX;
    sPlayerSlideTargetY = (s16)nextY * SIMA_TILE_PX;
    sPlayerSlideTimer = 0;
    sPlayerMoving = TRUE;
    sTurnPhase = SIMA_TURN_PLAYER_MOVE;

    UpdatePlayerSprite();
}

// SIMA_TURN_PLAYER_MOVE: avanza el deslizamiento del jugador hacia la
// casilla destino (SIMA_PLAYER_SLIDE_SPEED px/frame, SIMA_PLAYER_SLIDE_FRAMES
// frames en total). Al llegar, snap exacto a la casilla (sin arrastrar
// redondeo) y arranca el turno de los enemigos -- el jugador NO vuelve a
// leer input hasta que ese turno termine.
static void UpdatePlayerSlide(void)
{
    sPlayerX += sPlayerSlideDX;
    sPlayerY += sPlayerSlideDY;
    sPlayerSlideTimer++;

    sPlayerAnimTimer++;
    if (sPlayerAnimTimer >= WALK_ANIM_PERIOD)
    {
        sPlayerAnimTimer = 0;
        sPlayerAnimStep ^= 1;
    }

    if (sPlayerSlideTimer >= SIMA_PLAYER_SLIDE_FRAMES)
    {
        sPlayerX = sPlayerSlideTargetX;
        sPlayerY = sPlayerSlideTargetY;
        sPlayerMoving = FALSE;
        sPlayerAnimTimer = 0;
        sPlayerAnimStep = 0;
        StartEnemyTurn();   // el paso del jugador consume turno
    }

    UpdatePlayerSprite();
}

void SimaActors_GetPlayerTile(s8 *x, s8 *y)
{
    // Centro del sprite (no la esquina superior izquierda): es la casilla
    // que "ocupa" el jugador a efectos de lógica de tareas posteriores
    // (escaleras, disparadores, enemigos). Con turnos, sPlayerX/sPlayerY
    // solo son múltiplos exactos de SIMA_TILE_PX cuando SimaActors_IsPlayerIdle()
    // es TRUE -- src/sima.c ya gatea con eso antes de mirar la escalera.
    *x = (s8)((sPlayerX + 8) / SIMA_TILE_PX);
    *y = (s8)((sPlayerY + 8) / SIMA_TILE_PX);
}

// Arranca el empujón (mejora de sensación, ahora por turnos): un
// desplazamiento de UNA casilla en dirección contraria al enemigo que
// conectó -- si esa casilla está libre. Si está bloqueada, no hay empujón en
// absoluto (el brief es explícito: "si no, no se mueve"). Llamada desde
// StartEnemyTurn (mismo archivo, misma fase) en el instante exacto en que un
// enemigo ataca, con la casilla de ESE enemigo -- nunca en diagonal, porque
// un paso de rejilla es siempre de un solo eje (ver SimaActors_EnemyStepTarget):
// a diferencia de la version en tiempo real, aqui no hace falta resolver el
// caso "el enemigo estaba en diagonal".
static void StartPlayerKnockback(s8 enemyTileX, s8 enemyTileY)
{
    s8 px = (s8)(sPlayerX / SIMA_TILE_PX);
    s8 py = (s8)(sPlayerY / SIMA_TILE_PX);
    s8 dirX = 0, dirY = 0;
    s8 targetX, targetY;

    if (enemyTileX < px)
        dirX = 1;
    else if (enemyTileX > px)
        dirX = -1;
    else if (enemyTileY < py)
        dirY = 1;
    else if (enemyTileY > py)
        dirY = -1;

    if (dirX == 0 && dirY == 0)
        return;  // caso degenerado (mismo tile que el jugador): no debería pasar, pero sin dirección no hay a dónde empujar

    targetX = (s8)(px + dirX);
    targetY = (s8)(py + dirY);

    if (SimaRoom_IsSolid(sPlayerFloor, targetX, targetY))
        return;   // casilla de destino bloqueada: sin empujón, tal como pide el brief

    sPlayerKnockbackDX = (s16)dirX * SIMA_KNOCKBACK_SLIDE_SPEED;
    sPlayerKnockbackDY = (s16)dirY * SIMA_KNOCKBACK_SLIDE_SPEED;
    sPlayerKnockbackTargetX = (s16)targetX * SIMA_TILE_PX;
    sPlayerKnockbackTargetY = (s16)targetY * SIMA_TILE_PX;
    sPlayerKnockbackTimer = SIMA_KNOCKBACK_SLIDE_FRAMES;
}

// Avanza el empujón un frame, si hay uno en marcha (no-op si no). Llamada
// desde SimaActors_UpdatePlayer mientras sTurnPhase == SIMA_TURN_ENEMY_STEP.
// SIMA_KNOCKBACK_SLIDE_FRAMES <= SIMA_ENEMY_SLIDE_FRAMES por diseño (ver su
// definición), así que el empujón siempre termina dentro del turno de los
// enemigos -- nunca lo alarga ni se lo come.
static void AdvancePlayerKnockback(void)
{
    if (sPlayerKnockbackTimer == 0)
        return;

    sPlayerX += sPlayerKnockbackDX;
    sPlayerY += sPlayerKnockbackDY;
    sPlayerKnockbackTimer--;

    if (sPlayerKnockbackTimer == 0)
    {
        sPlayerX = sPlayerKnockbackTargetX;   // snap exacto, sin arrastrar redondeo
        sPlayerY = sPlayerKnockbackTargetY;
    }
}

// Escribe en el sprite el frame/flip que corresponde al facing y estado de
// movimiento actuales, y sincroniza su posición en pantalla con
// sPlayerX/sPlayerY. Único punto que toca gSprites[sPlayerSpriteId]: todas
// las fases del turno pasan por aquí para no duplicar la tabla de frames.
static void UpdatePlayerSprite(void)
{
    struct Sprite *sprite = &gSprites[sPlayerSpriteId];
    u16 frameTile;
    bool8 hFlip = FALSE;

    // Ciclo de paso de 2 frames (alterna STEP_A/STEP_B, o STEP_A/STEP_C en
    // el de perfil): suficiente para leerse como caminata sin meter una
    // máquina de 3 estados. FRAME_SIDE_STEP_B (la pose "de paso" intermedia
    // de player.png) queda sin usar a propósito -- STEP_A/STEP_C son las dos
    // zancadas más distintas entre sí, dan más contraste alternando.
    switch (sPlayerFacing)
    {
    case SIMA_FACING_UP:
        frameTile = !sPlayerMoving ? FRAME_UP_IDLE
                    : (sPlayerAnimStep ? FRAME_UP_STEP_B : FRAME_UP_STEP_A);
        break;
    case SIMA_FACING_LEFT:
        frameTile = !sPlayerMoving ? FRAME_SIDE_IDLE
                    : (sPlayerAnimStep ? FRAME_SIDE_STEP_C : FRAME_SIDE_STEP_A);
        hFlip = TRUE;
        break;
    case SIMA_FACING_RIGHT:
        frameTile = !sPlayerMoving ? FRAME_SIDE_IDLE
                    : (sPlayerAnimStep ? FRAME_SIDE_STEP_C : FRAME_SIDE_STEP_A);
        break;
    case SIMA_FACING_DOWN:
    default:
        frameTile = !sPlayerMoving ? FRAME_DOWN_IDLE
                    : (sPlayerAnimStep ? FRAME_DOWN_STEP_B : FRAME_DOWN_STEP_A);
        break;
    }

    sprite->oam.tileNum = sprite->sheetTileStart + frameTile;
    // Sin sistema de ANIMCMD de por medio (ver el comentario de sTmpl_SimaPlayer),
    // asi que el flip se escribe a mano: con affineMode OFF, los bits 3/4 de
    // matrixNum SON el h-flip/v-flip (ver struct OamData en include/gba/types.h),
    // no hace falta pasar por SetSpriteOamFlipBits.
    sprite->oam.matrixNum = hFlip ? ST_OAM_HFLIP : 0;
    sprite->x = sPlayerX + 8;
    sprite->y = sPlayerY + 8;
    // Parpadeo de "te acaban de golpear" (puramente visual, ver el
    // comentario junto a sPlayerInvulnTimer): se apaga y enciende cada 4
    // frames mientras dure.
    sprite->invisible = (sPlayerInvulnTimer > 0) && ((sPlayerInvulnTimer / 4) & 1);
}

// Avanza el golpe en curso (Tarea 7): decide qué frame del arma mostrar (o
// si se oculta, en recuperación), la coloca sobre la casilla adyacente a
// sAttackFacing (SimaActors_WeaponHitbox) y cuenta el frame. NO comprueba
// enemigos -- eso lo hace SimaActors_UpdateEnemies (más abajo en este mismo
// archivo, con AttackHitboxActive/SimaActors_WeaponHitbox), que ya recorre
// los enemigos cada frame y tiene sus posiciones a mano; duplicar ese bucle
// aquí sería la misma información dos veces. sAttackTimer es la ÚNICA
// fuente de verdad de la fase del golpe: UpdateEnemies solo LEE su valor a
// través de AttackHitboxActive, nunca lo toca. Al terminar (recuperación
// cumplida), el golpe consume el turno: le toca a los enemigos.
static void UpdateAttack(void)
{
    s16 hitX, hitY;

    if (!sWeaponActive)
    {
        // Sin sprite de arma (CreateSprite se quedó sin presupuesto): no hay
        // nada que animar ni golpe que reproducir, pero tampoco hay que
        // dejar al jugador congelado para siempre esperando un golpe que
        // nunca se resuelve -- el turno pasa igualmente a los enemigos.
        sAttackTimer = 0;
        StartEnemyTurn();
        return;
    }

    SimaActors_WeaponHitbox(sAttackFacing, sPlayerX, sPlayerY, &hitX, &hitY);
    // Arco de barrido (mejora de sensación, ver el comentario de arriba de
    // este bloque): desplazamiento COSMÉTICO perpendicular a la dirección
    // del golpe, hacia un lado durante el windup y hacia el lado contrario
    // durante el frame activo -- simula que el arma "viene de un lado y
    // termina en el otro" en vez de solo aparecer/desaparecer en el mismo
    // sitio. No toca hitX/hitY (la caja real de golpe, comprobada por
    // SimaActors_UpdateEnemies vía AttackHitboxActive/SimaActors_WeaponHitbox,
    // sigue siendo exactamente la casilla adyacente completa) -- es puro
    // dibujo, no cambia a qué enemigos alcanza el golpe. NÚMERO DE GUSTO:
    // ATTACK_SWEEP_PX, ver su definición.
    {
        s16 sweepX = 0, sweepY = 0;
        s16 sweep = (sAttackTimer <= ATTACK_WINDUP_FRAMES) ? -ATTACK_SWEEP_PX : ATTACK_SWEEP_PX;

        switch (sAttackFacing)
        {
        case SIMA_FACING_UP:
        case SIMA_FACING_DOWN:
            sweepX = sweep;   // golpe vertical: el barrido se ve en X
            break;
        case SIMA_FACING_LEFT:
        case SIMA_FACING_RIGHT:
            sweepY = sweep;   // golpe horizontal: el barrido se ve en Y
            break;
        }
        gSprites[sWeaponSpriteId].x = hitX + 8 + sweepX;
        gSprites[sWeaponSpriteId].y = hitY + 8 + sweepY;
    }

    if (sAttackTimer <= ATTACK_WINDUP_FRAMES)
    {
        // Windup: el arma ya se ve (telégrafo del golpe) pero todavía no daña.
        gSprites[sWeaponSpriteId].invisible = FALSE;
        gSprites[sWeaponSpriteId].oam.tileNum = gSprites[sWeaponSpriteId].sheetTileStart + FRAME_WEAPON_A;
    }
    else if (sAttackTimer <= ATTACK_WINDUP_FRAMES + ATTACK_ACTIVE_FRAMES)
    {
        // Activo: el destello de impacto, y AttackHitboxActive (usado por
        // SimaActors_UpdateEnemies) empieza a devolver TRUE en esta misma
        // ventana -- ver la comprobación exacta ahí, es el mismo rango.
        gSprites[sWeaponSpriteId].invisible = FALSE;
        gSprites[sWeaponSpriteId].oam.tileNum = gSprites[sWeaponSpriteId].sheetTileStart + FRAME_WEAPON_B;
    }
    else
    {
        // Recuperación: el arma ya se guardó, pero el golpe sigue en curso
        // (el turno todavía no ha pasado a los enemigos) hasta que
        // sAttackTimer llegue a ATTACK_TOTAL_FRAMES.
        gSprites[sWeaponSpriteId].invisible = TRUE;
    }

    // Orientación por flip de OAM, no por arte nuevo (ver el comentario de
    // sTmpl_SimaWeapon): el mandoble recortado apunta en su dibujo original
    // de mango arriba-izquierda a punta abajo-derecha (↘). Con solo hFlip y
    // vFlip hay 4 combinaciones posibles, cada una apuntando a una de las 4
    // diagonales: sin flip ↘, hFlip ↙, vFlip ↗, ambos ↖. Cada diagonal
    // "sirve" para dos de las cuatro direcciones cardinales (↘ vale para
    // ABAJO o DERECHA, ↙ para ABAJO o IZQUIERDA, ↗ para ARRIBA o DERECHA, ↖
    // para ARRIBA o IZQUIERDA) -- CON UNA SOLA COMBINACIÓN NO ALCANZAN LAS 4
    // DIRECCIONES SIN REPETIR. Esta asignación usa las 4 combinaciones, una
    // por dirección, eligiendo para cada una la diagonal que SÍ la tiene
    // como componente:
    //   ABAJO  -> sin flip (↘: abajo+derecha)
    //   DERECHA -> vFlip   (↗: arriba+derecha)
    //   ARRIBA -> ambos    (↖: arriba+izquierda)
    //   IZQUIERDA -> hFlip (↙: abajo+izquierda)
    {
        bool8 hFlip = (sAttackFacing == SIMA_FACING_LEFT) || (sAttackFacing == SIMA_FACING_UP);
        bool8 vFlip = (sAttackFacing == SIMA_FACING_RIGHT) || (sAttackFacing == SIMA_FACING_UP);
        gSprites[sWeaponSpriteId].oam.matrixNum = (hFlip ? ST_OAM_HFLIP : 0) | (vFlip ? ST_OAM_VFLIP : 0);
    }

    sAttackTimer++;
    if (sAttackTimer > ATTACK_TOTAL_FRAMES)
    {
        sAttackTimer = 0;   // golpe terminado
        StartEnemyTurn();   // el golpe consume turno: ahora les toca a los enemigos
    }
}

// Función pura (Tarea 7): ¿está la caja de golpe del arma activa AHORA
// MISMO? Envuelve el mismo rango de sAttackTimer que UpdateAttack usa para
// elegir FRAME_WEAPON_B, para que ambas lecturas de "¿está golpeando?"
// nunca se puedan desincronizar (una sola definición de la ventana activa).
// No pura respecto al reloj de la partida (lee sAttackTimer, estado), pero
// no depende de sprites ni de ningún enemigo -- SimaActors_UpdateEnemies la
// usa antes de comprobar la caja contra cada enemigo.
static bool8 AttackHitboxActive(void)
{
    return sAttackTimer > ATTACK_WINDUP_FRAMES
        && sAttackTimer <= ATTACK_WINDUP_FRAMES + ATTACK_ACTIVE_FRAMES;
}

// ---------------------------------------------------------------------
// Enemigos: rata, murciélago y slime en las casillas de SimaRoom_GetEnemy
// (los '*' del editor visual). Por turnos, igual que el jugador: cuando
// StartEnemyTurn se llama (al terminar el paso o el golpe del jugador),
// cada enemigo vivo calcula UNA casilla de destino hacia el jugador
// (SimaActors_EnemyStepTarget) y se desliza hacia ella durante
// SIMA_ENEMY_SLIDE_FRAMES frames -- o, si esa casilla resulta ser la del
// propio jugador, no se mueve: es un ataque, no un paso (ver StartEnemyTurn
// más abajo). Comparten la paleta única del jugador (sPlayerPal, arriba) y
// solo usan dos frames de animación "idle" de cada hoja (celdas (0,0)/(0,1)):
// ninguna de las tres criaturas tiene una pose direccional clara en el pack
// de origen, a diferencia del jugador, así que no hay ciclo de caminata por
// dirección -- esa "respiración" es puramente cosmética y sigue su propio
// reloj SIEMPRE, no está atada al turno (no es una POSICIÓN, así que no
// rompe "nada se mueve si tú no te mueves").
// ---------------------------------------------------------------------

static const u32 sRatGfx[] = INCBIN_U32("graphics/sima/rat.4bpp");
static const u32 sBatGfx[] = INCBIN_U32("graphics/sima/bat.4bpp");
static const u32 sSlimeGfx[] = INCBIN_U32("graphics/sima/slime.4bpp");

#define TAG_SIMA_RAT   0x6001
#define TAG_SIMA_BAT   0x6002
#define TAG_SIMA_SLIME 0x6003

#define ENEMY_SHEET_CELLS     24  // hoja de 4x6 celdas de 16x16 (64x96 px): ver graphics/sima/gen.py ASSETS
#define ENEMY_TILES_PER_FRAME  4  // 16x16 = 2x2 tiles de hardware de 8x8, igual que el jugador
#define ENEMY_FRAME_IDLE_A (0 * ENEMY_TILES_PER_FRAME)  // celda (0,0) de la hoja
#define ENEMY_FRAME_IDLE_B (1 * ENEMY_TILES_PER_FRAME)  // celda (0,1): "respira" para las tres criaturas
// Tarea 7: pose de muerte (aplastada/panza arriba en las tres hojas, visto
// a ojo -- rat.png/bat.png/slime.png comparten layout de 4x6). Dos celdas
// adyacentes de la misma fila, no una sola: un frame estático se lee como
// una imagen congelada más que como "acaba de morir"; alternar entre dos
// poses casi iguales durante SIMA_ENEMY_DEATH_FRAMES da un tembleque mínimo
// sin necesitar arte nuevo.
#define ENEMY_FRAME_DEATH_A (17 * ENEMY_TILES_PER_FRAME)  // celda (4,1)
#define ENEMY_FRAME_DEATH_B (18 * ENEMY_TILES_PER_FRAME)  // celda (4,2)

enum SimaEnemyKind
{
    SIMA_ENEMY_RAT,
    SIMA_ENEMY_BAT,
    SIMA_ENEMY_SLIME,
    SIMA_ENEMY_KIND_COUNT,
};

static const struct OamData sOam_SimaEnemy = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 1,  // igual que el jugador: por delante de BG0, detrás del HUD (BG1)
};

static const struct SpriteSheet sSheet_SimaRat = {
    sRatGfx, ENEMY_SHEET_CELLS * ENEMY_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_RAT
};
static const struct SpriteSheet sSheet_SimaBat = {
    sBatGfx, ENEMY_SHEET_CELLS * ENEMY_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_BAT
};
static const struct SpriteSheet sSheet_SimaSlime = {
    sSlimeGfx, ENEMY_SHEET_CELLS * ENEMY_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_SLIME
};

// paletteTag = TAG_SIMA_PLAYER a propósito: los enemigos reutilizan la misma
// paleta única de sprites que el jugador (ver sPal_SimaPlayer, arriba), tal
// como fija el spec de assets ("una paleta para todos los sprites").
static const struct SpriteTemplate sTmpl_SimaRat = {
    .tileTag = TAG_SIMA_RAT, .paletteTag = TAG_SIMA_PLAYER, .oam = &sOam_SimaEnemy,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};
static const struct SpriteTemplate sTmpl_SimaBat = {
    .tileTag = TAG_SIMA_BAT, .paletteTag = TAG_SIMA_PLAYER, .oam = &sOam_SimaEnemy,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};
static const struct SpriteTemplate sTmpl_SimaSlime = {
    .tileTag = TAG_SIMA_SLIME, .paletteTag = TAG_SIMA_PLAYER, .oam = &sOam_SimaEnemy,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// Indexado por (i % SIMA_ENEMY_KIND_COUNT) en SimaActors_InitEnemies: con los
// 3 enemigos del piso 1 da exactamente uno de cada especie, en orden fijo
// (determinista, no aleatorio -- importa para la captura de depuración).
static const struct SpriteTemplate *const sEnemyTemplates[SIMA_ENEMY_KIND_COUNT] = {
    &sTmpl_SimaRat, &sTmpl_SimaBat, &sTmpl_SimaSlime,
};

// Tope de enemigos que este archivo sabe manejar a la vez: los arrays de
// abajo se dimensionan a esto, no a SimaRoom_GetEnemyCount (que es un valor
// de EJECUCIÓN, no una constante de compilación). Coincide hoy con
// SIMA_ROOM_MAX_ENEMIES (src/sima_rooms_data.h, generado); si el editor
// visual algún día coloca más de 3 enemigos en un piso, el clamp en
// SimaActors_InitEnemies recorta en vez de desbordar estos arrays.
#define SIMA_MAX_ENEMIES 3

#define SIMA_ENEMY_ANIM_PERIOD   16   // frames entre los dos frames de "respirar" (cosmético, no ligado al turno)
#define SIMA_ENEMY_CONTACT_DAMAGE 1

// Tarea 7: un enemigo muere de UN golpe -- no hay vida propia por enemigo.
// Encaja con la regla del proyecto de "sin grinding" (CLAUDE.md): un sistema
// de aguante por enemigo solo tendría sentido si hubiera algo que farmear
// para subirlo, y aquí no lo hay. También es lo que literalmente pide el
// brief ("un enemigo tocado por el arma muere"), sin condición.
//
// sEnemyDeathTimer > 0 mientras el cadáver sigue en pantalla reproduciendo
// ENEMY_FRAME_DEATH_A/B; llega a 0 y SimaActors_UpdateEnemies destruye el
// sprite. Deliberadamente SEPARADO de sEnemyAlive: el enemigo deja de contar
// como "vivo" (para SimaActors_StairsUnlocked) en el instante mismo del
// golpe, no cuando termina la animación -- la escalera no debería esperar a
// que acabe un efecto cosmético para desbloquearse.
#define SIMA_ENEMY_DEATH_FRAMES       24   // ~0.4s a 60Hz: cuánto se ve el cadáver antes de destruir el sprite
#define SIMA_ENEMY_DEATH_ANIM_PERIOD   6   // alterna DEATH_A/DEATH_B cada 6 frames

static bool8 sEnemyAlive[SIMA_MAX_ENEMIES];
static u8 sEnemyDeathTimer[SIMA_MAX_ENEMIES];  // 0 = no está muriendo (vivo, o ya destruido)
static u8 sEnemySpriteId[SIMA_MAX_ENEMIES];
static s16 sEnemyX[SIMA_MAX_ENEMIES];   // esquina superior izquierda del sprite, en píxeles
static s16 sEnemyY[SIMA_MAX_ENEMIES];
static u8 sEnemyFloor;
static u8 sEnemyCount;      // cuántos de los SIMA_MAX_ENEMIES slots están colocados en el piso actual
static u8 sEnemyAnimTimer;
static u8 sEnemyAnimStep;

// Turno de los enemigos (SIMA_TURN_ENEMY_STEP). sEnemyMoving[i] indica si el
// enemigo i está deslizándose este turno (FALSE si atacó sin moverse, si se
// quedó bloqueado en ambos ejes, o si está muerto/muriendo); sEnemySlideDX/DY
// es su delta por frame, sEnemyTargetX/Y su casilla de destino en píxeles
// (snap exacto al terminar). sEnemyStepTimer es UN solo cronómetro
// compartido por los tres -- todos se deslizan la misma cantidad de frames,
// aunque alguno se quede quieto.
static bool8 sEnemyMoving[SIMA_MAX_ENEMIES];
static s16 sEnemySlideDX[SIMA_MAX_ENEMIES];
static s16 sEnemySlideDY[SIMA_MAX_ENEMIES];
static s16 sEnemyTargetX[SIMA_MAX_ENEMIES];
static s16 sEnemyTargetY[SIMA_MAX_ENEMIES];
static u8 sEnemyStepTimer;

// Un solo golpe por turno de enemigos, sin importar cuántos ataquen a la vez
// (mismo espíritu que la ventana de invulnerabilidad de la versión en tiempo
// real, pero medido en turnos, no en frames): StartEnemyTurn la pone a FALSE
// al empezar el turno, y el primer enemigo cuyo paso aterriza en la casilla
// del jugador la sube a TRUE.
static bool8 sPlayerHitThisTurn;

// Función pura (turnos): la casilla a la que un enemigo en (ex, ey) daría su
// paso hacia el jugador en (px, py), en el piso `floor`. Elige el eje que
// más lo acerca (empate -> vertical, misma prioridad que el facing del
// jugador); si esa casilla está bloqueada por un muro prueba el otro eje; si
// los dos lo están, se queda quieto (outX/outY quedan en ex/ey). NO
// distingue "el destino es la casilla del jugador" de "el destino es suelo
// libre" -- esa decisión (moverse de verdad vs. atacar sin moverse) la toma
// StartEnemyTurn comparando el resultado contra (px, py), porque es ahí
// donde vive el estado de vida/knockback. Es exactamente la misma regla para
// "un enemigo que llega a la casilla del jugador" y "uno que ya está
// adyacente y avanza contra él" del brief: con pasos de una sola casilla,
// adyacente-y-avanza ES llegar.
void SimaActors_EnemyStepTarget(u8 floor, s8 ex, s8 ey, s8 px, s8 py, s8 *outX, s8 *outY)
{
    s8 dx = px - ex;
    s8 dy = py - ey;
    s8 adx = (dx < 0) ? -dx : dx;
    s8 ady = (dy < 0) ? -dy : dy;
    s8 stepX = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    s8 stepY = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;

    *outX = ex;
    *outY = ey;

    if (adx > ady && stepX != 0)
    {
        if (!SimaRoom_IsSolid(floor, ex + stepX, ey))
        {
            *outX = ex + stepX;
            return;
        }
        if (stepY != 0 && !SimaRoom_IsSolid(floor, ex, ey + stepY))
            *outY = ey + stepY;
        return;
    }

    if (stepY != 0)
    {
        if (!SimaRoom_IsSolid(floor, ex, ey + stepY))
        {
            *outY = ey + stepY;
            return;
        }
        if (stepX != 0 && !SimaRoom_IsSolid(floor, ex + stepX, ey))
            *outX = ex + stepX;
        return;
    }

    if (stepX != 0 && !SimaRoom_IsSolid(floor, ex + stepX, ey))
        *outX = ex + stepX;
}

// Coloca los enemigos del piso leyendo SimaRoom_GetEnemy. Sin guarda de
// idempotencia para LoadSpriteSheet (a diferencia de lo que advierte la
// nota de cabecera de este archivo sobre cargas dobles): igual que
// SimaActors_InitPlayer, esta función solo se llama UNA VEZ por sesión del
// modo (CB2_InitSima, case 1), justo después de que ResetSpriteData vacíe
// la tabla de sprites -- recargar aquí siempre es correcto, nunca una fuga.
void SimaActors_InitEnemies(u8 floor)
{
    u8 i, count;

    LoadSpriteSheet(&sSheet_SimaRat);
    LoadSpriteSheet(&sSheet_SimaBat);
    LoadSpriteSheet(&sSheet_SimaSlime);
    // Misma paleta que el jugador; LoadSpritePalette SÍ es idempotente por
    // tag (a diferencia de LoadSpriteSheet), así que si SimaActors_InitPlayer
    // ya la cargó esta llamada no hace nada.
    LoadSpritePalette(&sPal_SimaPlayer);

    sEnemyFloor = floor;
    count = SimaRoom_GetEnemyCount(floor);
    if (count > SIMA_MAX_ENEMIES)
        count = SIMA_MAX_ENEMIES;  // red de seguridad, ver el comentario de SIMA_MAX_ENEMIES
    sEnemyCount = count;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        if (i < count)
        {
            s8 ex, ey;
            SimaRoom_GetEnemy(floor, i, &ex, &ey);
            sEnemyX[i] = (s16)ex * SIMA_TILE_PX;
            sEnemyY[i] = (s16)ey * SIMA_TILE_PX;
            sEnemySpriteId[i] = CreateSprite(sEnemyTemplates[i % SIMA_ENEMY_KIND_COUNT],
                                              sEnemyX[i] + 8, sEnemyY[i] + 8, 1);
            // Si CreateSprite se queda sin presupuesto (MAX_SPRITES), este
            // slot no cuenta como vivo: ni bloquea la escalera para siempre
            // (sería peor que dejarla pasar) ni intenta animar un sprite que
            // no existe. No debería pasar con el presupuesto de esta tarea
            // (jugador + 3 enemigos + HUD, ver el brief), pero es la misma
            // guarda que ya usa SimaActors_InitPlayer con sPlayerActive.
            sEnemyAlive[i] = (sEnemySpriteId[i] != MAX_SPRITES);
        }
        else
        {
            sEnemyAlive[i] = FALSE;
        }
        // Reinicio explícito (Tarea 7), misma razón que sCurrentFloor en
        // CB2_InitSima: si el modo se remonta en la misma sesión de ROM
        // (PHANTOM_DEBUG_SIMA), un cadáver de la sesión anterior no debe
        // seguir "muriendo" en la nueva.
        sEnemyDeathTimer[i] = 0;
        sEnemyMoving[i] = FALSE;
    }

    sEnemyAnimTimer = 0;
    sEnemyAnimStep = 0;
    sEnemyStepTimer = 0;
    sPlayerHitThisTurn = FALSE;
}

// Arranca el turno de los enemigos: llamada UNA vez, en el frame exacto en
// que termina el paso o el golpe del jugador (ver UpdatePlayerSlide/UpdateAttack).
// Para cada enemigo vivo (ni muerto ni en pleno cadáver) calcula su casilla
// de destino con SimaActors_EnemyStepTarget y decide:
//   - destino == casilla del jugador  -> ataque: no se mueve, daña (una vez
//     por turno, sPlayerHitThisTurn) y empuja al jugador (StartPlayerKnockback).
//   - destino == su propia casilla    -> bloqueado en ambos ejes, se queda quieto.
//   - cualquier otro destino          -> movimiento real: arranca su deslizamiento.
static void StartEnemyTurn(void)
{
    u8 i;
    s8 px = (s8)(sPlayerX / SIMA_TILE_PX);
    s8 py = (s8)(sPlayerY / SIMA_TILE_PX);

    sPlayerHitThisTurn = FALSE;
    sEnemyStepTimer = 0;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        s8 ex, ey, nx, ny;

        sEnemyMoving[i] = FALSE;

        if (sEnemyDeathTimer[i] > 0 || !sEnemyAlive[i])
            continue;   // cadáver en curso, o ya destruido: no le toca turno

        ex = (s8)(sEnemyX[i] / SIMA_TILE_PX);
        ey = (s8)(sEnemyY[i] / SIMA_TILE_PX);
        SimaActors_EnemyStepTarget(sEnemyFloor, ex, ey, px, py, &nx, &ny);

        if (nx == px && ny == py)
        {
            // Ataque: el paso del enemigo aterriza en la casilla del
            // jugador. No se mueve el sprite del enemigo -- solo el jugador
            // retrocede, vía knockback. Un solo golpe por turno.
            if (!sPlayerHitThisTurn)
            {
                sPlayerHP = SimaActors_ApplyDamage(sPlayerHP, SIMA_ENEMY_CONTACT_DAMAGE);
                sPlayerInvulnTimer = SIMA_HIT_FLASH_FRAMES;
                StartPlayerKnockback(ex, ey);
                sPlayerHitThisTurn = TRUE;
            }
        }
        else if (nx != ex || ny != ey)
        {
            sEnemyMoving[i] = TRUE;
            sEnemyTargetX[i] = (s16)nx * SIMA_TILE_PX;
            sEnemyTargetY[i] = (s16)ny * SIMA_TILE_PX;
            sEnemySlideDX[i] = (s16)(nx - ex) * SIMA_ENEMY_SLIDE_SPEED;
            sEnemySlideDY[i] = (s16)(ny - ey) * SIMA_ENEMY_SLIDE_SPEED;
        }
        // else: nx==ex && ny==ey -> bloqueado en ambos ejes, se queda quieto
        // (sEnemyMoving[i] ya es FALSE).
    }

    sTurnPhase = SIMA_TURN_ENEMY_STEP;
}

// Avanza un frame el deslizamiento de todos los enemigos en marcha. Al
// llegar (sEnemyStepTimer == SIMA_ENEMY_SLIDE_FRAMES), snap exacto a la
// casilla destino y el turno vuelve al jugador.
static void AdvanceEnemyStepPhase(void)
{
    u8 i;

    sEnemyStepTimer++;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        if (!sEnemyMoving[i])
            continue;
        sEnemyX[i] += sEnemySlideDX[i];
        sEnemyY[i] += sEnemySlideDY[i];
    }

    if (sEnemyStepTimer >= SIMA_ENEMY_SLIDE_FRAMES)
    {
        for (i = 0; i < SIMA_MAX_ENEMIES; i++)
        {
            if (sEnemyMoving[i])
            {
                sEnemyX[i] = sEnemyTargetX[i];   // snap exacto, sin arrastrar redondeo
                sEnemyY[i] = sEnemyTargetY[i];
                sEnemyMoving[i] = FALSE;
            }
        }
        sTurnPhase = SIMA_TURN_PLAYER_INPUT;   // el turno vuelve al jugador
    }
}

void SimaActors_UpdateEnemies(void)
{
    u8 i;
    // Golpe del jugador (Tarea 7): se calcula UNA vez por frame, no por
    // enemigo. Se comprueba TODOS los frames en que el arma está activa
    // (SIMA_TURN_PLAYER_ATTACK, antes de que le toque mover a nadie) --
    // igual que en la versión en tiempo real. Con turnos, la comparación es
    // EXACTA por casilla (hitX/hitY y sEnemyX[i]/sEnemyY[i] son siempre
    // múltiplos de SIMA_TILE_PX aquí: el arma solo se activa con el jugador
    // quieto en SIMA_TURN_PLAYER_ATTACK, y ningún enemigo puede estar a
    // medio deslizar mientras tanto porque son fases mutuamente
    // excluyentes) en vez de la AABB de 12x12 que hacía falta con
    // movimiento libre (BoxesOverlap, eliminada con esta tarea).
    bool8 attackHit = AttackHitboxActive();
    s16 hitX = 0, hitY = 0;

    if (attackHit)
        SimaActors_WeaponHitbox(sAttackFacing, sPlayerX, sPlayerY, &hitX, &hitY);

    if (sPlayerInvulnTimer > 0)
        sPlayerInvulnTimer--;

    // Respiración (cosmética): su propio reloj, siempre corriendo, no ligado
    // al turno -- ver el comentario grande al principio de esta sección.
    sEnemyAnimTimer++;
    if (sEnemyAnimTimer >= SIMA_ENEMY_ANIM_PERIOD)
    {
        sEnemyAnimTimer = 0;
        sEnemyAnimStep ^= 1;
    }

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        struct Sprite *sprite = &gSprites[sEnemySpriteId[i]];

        // Cadáver en curso (Tarea 7): no se mueve, no daña por contacto, solo
        // anima la muerte y cuenta atrás hasta que toca destruir el sprite.
        // Va ANTES del guard de sEnemyAlive porque un enemigo muriendo ya
        // tiene sEnemyAlive en FALSE (ver más abajo) pero su sprite sigue
        // vivo unos frames más.
        if (sEnemyDeathTimer[i] > 0)
        {
            sEnemyDeathTimer[i]--;
            sprite->oam.tileNum = sprite->sheetTileStart +
                (((sEnemyDeathTimer[i] / SIMA_ENEMY_DEATH_ANIM_PERIOD) & 1)
                     ? ENEMY_FRAME_DEATH_A : ENEMY_FRAME_DEATH_B);
            if (sEnemyDeathTimer[i] == 0)
                DestroySprite(sprite);
            continue;
        }

        if (!sEnemyAlive[i])
            continue;  // ya destruido del todo (o nunca llegó a existir, ver SimaActors_InitEnemies)

        // Golpe del arma: un enemigo tocado muere de un golpe (ver el
        // comentario junto a SIMA_ENEMY_DEATH_FRAMES sobre por qué un solo
        // golpe y no una barra de vida). sEnemyAlive baja a FALSE AQUÍ
        // MISMO -- no cuando termina la animación -- para que
        // SimaActors_StairsUnlocked/GetAliveEnemyCount reaccionen en el
        // frame exacto del golpe.
        if (attackHit && hitX == sEnemyX[i] && hitY == sEnemyY[i])
        {
            sEnemyAlive[i] = FALSE;
            sEnemyDeathTimer[i] = SIMA_ENEMY_DEATH_FRAMES;
            continue;
        }

        sprite->oam.tileNum = sprite->sheetTileStart +
            (sEnemyAnimStep ? ENEMY_FRAME_IDLE_B : ENEMY_FRAME_IDLE_A);
    }

    // Movimiento por turnos: SOLO avanza mientras estamos en el turno de los
    // enemigos (ver StartEnemyTurn, llamada al terminar el turno del
    // jugador). El resto de las fases, este bloque no hace nada -- por eso
    // "sin pulsar nada nadie se mueve" (verificado por memoria, ver el
    // informe de esta tarea).
    if (sTurnPhase == SIMA_TURN_ENEMY_STEP)
        AdvanceEnemyStepPhase();

    // Sincroniza la posición en pantalla de cada enemigo vivo que no esté en
    // pleno cadáver -- hecho DESPUÉS de un posible avance de posición en
    // este mismo frame, para que se vea de inmediato y no un frame tarde.
    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        if (sEnemyDeathTimer[i] > 0 || !sEnemyAlive[i])
            continue;
        gSprites[sEnemySpriteId[i]].x = sEnemyX[i] + 8;
        gSprites[sEnemySpriteId[i]].y = sEnemyY[i] + 8;
    }
}

u8 SimaActors_GetAliveEnemyCount(void)
{
    u8 i, count = 0;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
        if (sEnemyAlive[i])
            count++;

    return count;
}

// Pura, sin sprites (Tarea 6, cambio de diseño): la escalera está cerrada
// mientras quede algún enemigo vivo, abierta con 0. Aislada en su propia
// función de una línea a propósito -- la decisión tomada HOY es que la
// escalera APARECE DE GOLPE al morir el último (no "visible pero apagada"
// hasta entonces); si eso cambia, este es el único sitio a tocar, junto con
// UpdateStairsVisibility en src/sima.c (que decide CUÁNDO repintar la
// celda a partir de lo que esta función responde, no CÓMO se ve).
//
// Es también lo que hace posible el jefe invencible del piso 3 (Tarea 8,
// fuera de alcance aquí): si su enemigo nunca muere, esta función nunca
// devuelve TRUE para ese piso y su escalera no aparece jamás.
bool8 SimaActors_StairsUnlocked(u8 aliveEnemyCount)
{
    return aliveEnemyCount == 0;
}
