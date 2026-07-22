#include "global.h"
#include "sima.h"
#include "sima_rooms.h"
#include "sprite.h"
#include "decompress.h"
#include "main.h"

// Jugador de SIMA (Tarea 4): sprite de 16x16 que se mueve en píxeles (tiempo
// real, no roguelike por turnos) sobre la sala de la Tarea 3, con colisión
// por casilla consultando SimaRoom_IsSolid en las cuatro esquinas de una
// caja más chica que el sprite completo. src/sima.c llama a
// SimaActors_InitPlayer una vez al montar el modo y a
// SimaActors_UpdatePlayer cada frame desde CB2_SimaMain.
//
// Tarea 6 añade a este mismo archivo: vida del jugador con daño saturado en
// 0 (SimaActors_ApplyDamage), y los enemigos (rata/murciélago/slime) con
// movimiento simple hacia el jugador, daño por contacto e invulnerabilidad
// breve. También el predicado puro que decide si la escalera está abierta
// (SimaActors_StairsUnlocked) -- el corazón del cambio de diseño de esta
// tarea: la escalera no aparece hasta que caen todos los enemigos del piso.

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
// ATTACK_TOTAL_FRAMES antes de poder volver a atacar. Números elegidos para
// que el golpe se LEA (el arma es visible unos frames antes de dañar, un
// telégrafo mínimo) sin ser espameable: JOY_NEW(A) sólo arranca un golpe
// nuevo con sAttackTimer en 0, así que mantener A pulsado no encadena
// ataques -- hay que soltar y volver a pulsar, y aun así el jugador espera
// ATTACK_TOTAL_FRAMES completos (~0.27s a 60Hz) entre golpes. Mismo espíritu
// que PLAYER_SPEED = 1: "paso deliberado, no arcade".
#define ATTACK_WINDUP_FRAMES   3  // arma visible (FRAME_A), sin dañar todavía
#define ATTACK_ACTIVE_FRAMES   4  // arma visible (FRAME_B), caja de golpe activa
#define ATTACK_RECOVERY_FRAMES 9  // arma oculta, cooldown antes de poder atacar de nuevo
#define ATTACK_TOTAL_FRAMES (ATTACK_WINDUP_FRAMES + ATTACK_ACTIVE_FRAMES + ATTACK_RECOVERY_FRAMES)

// Caja de colisión, más chica que el sprite completo de 16x16 y centrada en
// él: deja 2px de margen a cada lado para que doblar una esquina en un
// pasillo de una sola casilla (16px) no exija alineación a píxel exacto
// (con la caja completa, un jugador 1-2px desalineado en el eje que no se
// está moviendo se traba contra la esquina del muro vecino). Ver el informe
// de la Tarea 4 para más detalle de esta elección.
#define COLLISION_W 12
#define COLLISION_H 12
#define COLLISION_MARGIN_X ((16 - COLLISION_W) / 2)
#define COLLISION_MARGIN_Y ((16 - COLLISION_H) / 2)

#define PLAYER_SPEED 1        // px/frame: paso deliberado, no arcade -- encaja con el tono del juego
#define WALK_ANIM_PERIOD 8    // frames entre pasos del ciclo de caminata

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
static u8 sPlayerInvulnTimer;  // frames restantes sin poder recibir otro golpe

// Ataque (Tarea 7). sWeaponActive es la misma guarda de presupuesto de
// sprites que sPlayerActive (por si CreateSprite se queda sin hueco).
// sAttackTimer en 0 significa "listo para atacar"; 1..ATTACK_TOTAL_FRAMES
// mientras el golpe está en curso (ver UpdateAttack). sAttackFacing fija la
// dirección del golpe al iniciarlo, no la lee de sPlayerFacing cada frame:
// el jugador no puede girar a mitad de un golpe (ver el guard en
// SimaActors_UpdatePlayer), pero fijarla explícita documenta la intención y
// evita depender de ese guard si algún día cambia.
static bool8 sWeaponActive;
static u8 sWeaponSpriteId;
static u8 sAttackTimer;
static u8 sAttackFacing;

static void UpdatePlayerSprite(void);
static void UpdateAttack(void);

// Función pura: ¿cabe la caja de colisión del jugador (16x16 con el margen
// de arriba) en la posición (x, y) [esquina superior izquierda del sprite,
// en píxeles] sin superponerse a un muro? Separada de
// SimaActors_UpdatePlayer (que además lee input y mueve sprites) para que
// el harness in-ROM (src/phantom_test.c) pueda ejercitarla sin inputs, igual
// que Test_SimaRoomsValid ejercita SimaRoom_IsSolid.
bool8 SimaActors_BoxFits(u8 floor, s16 x, s16 y)
{
    s16 left = x + COLLISION_MARGIN_X;
    s16 top = y + COLLISION_MARGIN_Y;
    s16 right = left + COLLISION_W - 1;
    s16 bottom = top + COLLISION_H - 1;

    if (SimaRoom_IsSolid(floor, (s8)(left / 16), (s8)(top / 16)))
        return FALSE;
    if (SimaRoom_IsSolid(floor, (s8)(right / 16), (s8)(top / 16)))
        return FALSE;
    if (SimaRoom_IsSolid(floor, (s8)(left / 16), (s8)(bottom / 16)))
        return FALSE;
    if (SimaRoom_IsSolid(floor, (s8)(right / 16), (s8)(bottom / 16)))
        return FALSE;
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

// Función pura (Tarea 7): casilla de 16x16 (misma convención de esquina
// superior izquierda que SimaActors_BoxFits) que amenaza el arma cuando el
// jugador -- con su caja en (playerX, playerY) -- ataca mirando `facing`.
// Siempre la casilla ADYACENTE (un salto de 16px en el eje de la dirección),
// nunca la propia del jugador: así un golpe no puede autolesionar, y solo
// alcanza a un enemigo que esté de verdad delante, no a uno que solo
// comparta casilla por detrás o al lado. Separada de todo estado (no lee
// sPlayerX/sPlayerY ni sAttackFacing) para que el harness in-ROM la
// ejercite sin sprites, igual que SimaActors_BoxFits.
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
    sPlayerX = (s16)spawnX * 16;
    sPlayerY = (s16)spawnY * 16;
    sPlayerFacing = SIMA_FACING_DOWN;
    sPlayerMoving = FALSE;
    sPlayerAnimStep = 0;
    sPlayerAnimTimer = 0;
    sPlayerHP = SIMA_PLAYER_MAX_HP;   // vida solo se fija al montar el modo, no en cada piso (ver WarpToFloor)
    sPlayerInvulnTimer = 0;
    sAttackTimer = 0;   // listo para atacar (Tarea 7)

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
    sPlayerX = (s16)spawnX * 16;
    sPlayerY = (s16)spawnY * 16;
    sPlayerFacing = SIMA_FACING_DOWN;
    sPlayerMoving = FALSE;
    sPlayerAnimStep = 0;
    sPlayerAnimTimer = 0;
    // sPlayerHP NO se resetea aquí a propósito: la vida es del intento, no
    // del piso -- bajar un piso con un corazón no debería devolverte los
    // otros dos. La invulnerabilidad sí se corta: no tiene sentido arrastrar
    // frames de "acabo de recibir un golpe" al piso nuevo.
    sPlayerInvulnTimer = 0;
    // Golpe en curso tampoco se arrastra al piso nuevo, por la misma razón:
    // aparecer en el spawn nuevo con el arma ya afuera (o a mitad de
    // cooldown de un golpe que ni siquiera se vio) sería confuso.
    sAttackTimer = 0;
    if (sWeaponActive)
        gSprites[sWeaponSpriteId].invisible = TRUE;

    UpdatePlayerSprite();
}

void SimaActors_UpdatePlayer(void)
{
    u8 newFacing = sPlayerFacing;
    s16 dx = 0, dy = 0;
    bool8 moved = FALSE;

    if (!sPlayerActive)
        return;  // SimaActors_InitPlayer no se llamó, o CreateSprite se quedó sin presupuesto (MAX_SPRITES)

    // Tarea 7: un golpe congela al jugador -- ni se mueve ni cambia de
    // facing mientras el arma está en el aire (windup/activo/recuperación).
    // Evita decidir qué pasa si el jugador gira a mitad de un golpe, y
    // encaja con PLAYER_SPEED = 1 ("paso deliberado, no arcade"): un golpe
    // es un compromiso breve, no algo que se cancela con el D-pad.
    if (sAttackTimer > 0)
    {
        UpdateAttack();
        UpdatePlayerSprite();
        return;
    }

    // JOY_NEW (no JOY_HELD): mantener A pulsado no encadena golpes, hay que
    // soltar y volver a pulsar. Junto con el guard de arriba (que ya impide
    // esto mientras sAttackTimer > 0), es lo que evita machacar A.
    if (JOY_NEW(A_BUTTON))
    {
        // Ataca hacia donde el jugador YA miraba, no hacia el D-pad de este
        // mismo frame: más predecible que "moverte y atacar" a la vez.
        sAttackFacing = sPlayerFacing;
        sAttackTimer = 1;
        UpdateAttack();
        UpdatePlayerSprite();
        return;
    }

    // Un solo eje por frame: arriba/abajo tiene prioridad sobre izq/der si
    // se pulsan varias direcciones a la vez. Evita el movimiento diagonal,
    // que ni el arte (4 direcciones, no 8) ni la lógica de colisión de abajo
    // (una casilla por eje) contemplan.
    if (JOY_HELD(DPAD_UP))
    {
        newFacing = SIMA_FACING_UP;
        dy = -PLAYER_SPEED;
    }
    else if (JOY_HELD(DPAD_DOWN))
    {
        newFacing = SIMA_FACING_DOWN;
        dy = PLAYER_SPEED;
    }
    else if (JOY_HELD(DPAD_LEFT))
    {
        newFacing = SIMA_FACING_LEFT;
        dx = -PLAYER_SPEED;
    }
    else if (JOY_HELD(DPAD_RIGHT))
    {
        newFacing = SIMA_FACING_RIGHT;
        dx = PLAYER_SPEED;
    }

    if (dx != 0 || dy != 0)
    {
        s16 nx = sPlayerX + dx;
        s16 ny = sPlayerY + dy;

        // Colisión comprobada ANTES de aplicar el desplazamiento: si no
        // cabe, el jugador igual gira a mirar hacia el muro (newFacing ya
        // quedó fijado arriba) pero no se desplaza ni anima el paso.
        if (SimaActors_BoxFits(sPlayerFloor, nx, ny))
        {
            sPlayerX = nx;
            sPlayerY = ny;
            moved = TRUE;
        }
    }

    if (moved)
    {
        sPlayerAnimTimer++;
        if (sPlayerAnimTimer >= WALK_ANIM_PERIOD)
        {
            sPlayerAnimTimer = 0;
            sPlayerAnimStep ^= 1;
        }
    }
    else
    {
        sPlayerAnimTimer = 0;
        sPlayerAnimStep = 0;
    }

    sPlayerFacing = newFacing;
    sPlayerMoving = moved;

    UpdatePlayerSprite();
}

void SimaActors_GetPlayerTile(s8 *x, s8 *y)
{
    // Centro del sprite (no la esquina de la caja de colisión): es la
    // casilla que "ocupa" el jugador a efectos de lógica de tareas
    // posteriores (escaleras, disparadores, enemigos).
    *x = (s8)((sPlayerX + 8) / 16);
    *y = (s8)((sPlayerY + 8) / 16);
}

// Escribe en el sprite el frame/flip que corresponde al facing y estado de
// movimiento actuales, y sincroniza su posición en pantalla con
// sPlayerX/sPlayerY. Único punto que toca gSprites[sPlayerSpriteId]: tanto
// InitPlayer como UpdatePlayer pasan por aquí para no duplicar la tabla de
// frames.
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
    // Parpadeo de invulnerabilidad (Tarea 6): se apaga y enciende cada 4
    // frames mientras dure sPlayerInvulnTimer, la señal visual estándar de
    // "acabas de recibir un golpe y no puedes recibir otro todavía".
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
// través de AttackHitboxActive, nunca lo toca.
static void UpdateAttack(void)
{
    s16 hitX, hitY;

    if (!sWeaponActive)
    {
        // Sin sprite de arma (CreateSprite se quedó sin presupuesto): no hay
        // nada que animar ni golpe que reproducir, pero tampoco hay que
        // dejar al jugador congelado para siempre esperando un golpe que
        // nunca se resuelve.
        sAttackTimer = 0;
        return;
    }

    SimaActors_WeaponHitbox(sAttackFacing, sPlayerX, sPlayerY, &hitX, &hitY);
    gSprites[sWeaponSpriteId].x = hitX + 8;
    gSprites[sWeaponSpriteId].y = hitY + 8;

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
        // Recuperación: el arma ya se guardó, pero el jugador sigue sin
        // poder atacar hasta que sAttackTimer llegue a ATTACK_TOTAL_FRAMES.
        gSprites[sWeaponSpriteId].invisible = TRUE;
    }

    // Orientación por flip de OAM, no por arte nuevo (ver el comentario de
    // sTmpl_SimaWeapon): el mandoble recortado ya apunta abajo-derecha en su
    // dibujo original, así que ABAJO/DERECHA lo usan tal cual, ARRIBA lo
    // voltea en vertical y IZQUIERDA en horizontal. Es una aproximación (el
    // mismo golpe "sirve" para dos direcciones distintas) deliberada: la
    // posición del sprite -- sobre la casilla adyacente correcta -- es lo
    // que de verdad comunica hacia dónde se ataca; el flip es solo pulido.
    {
        bool8 hFlip = (sAttackFacing == SIMA_FACING_LEFT);
        bool8 vFlip = (sAttackFacing == SIMA_FACING_UP);
        gSprites[sWeaponSpriteId].oam.matrixNum = (hFlip ? ST_OAM_HFLIP : 0) | (vFlip ? ST_OAM_VFLIP : 0);
    }

    sAttackTimer++;
    if (sAttackTimer > ATTACK_TOTAL_FRAMES)
        sAttackTimer = 0;  // cooldown cumplido: listo para el siguiente golpe
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
// Enemigos (Tarea 6): rata, murciélago y slime en las casillas de
// SimaRoom_GetEnemy (los '*' del editor visual), con movimiento simple
// hacia el jugador y la misma colisión de pared que él (SimaActors_BoxFits
// -- mismo tamaño de sprite de 16x16, misma caja de 12x12). Comparten la
// paleta única del jugador (sPlayerPal, arriba) y solo usan dos frames de
// animación "idle" de cada hoja (celdas (0,0)/(0,1)): ninguna de las tres
// criaturas tiene una pose direccional clara en el pack de origen, a
// diferencia del jugador, así que no hay ciclo de caminata por dirección.
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

#define SIMA_ENEMY_SPEED          1   // px/frame, cuando le toca moverse (ver SIMA_ENEMY_MOVE_PERIOD)
#define SIMA_ENEMY_MOVE_PERIOD    2   // se mueve 1 de cada 2 frames: la mitad de rápido que el jugador
#define SIMA_ENEMY_ANIM_PERIOD   16   // frames entre los dos frames de "respirar"
#define SIMA_ENEMY_CONTACT_DAMAGE 1
#define SIMA_INVULN_FRAMES       60   // ~1s a 60Hz sin poder recibir otro golpe

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
static u8 sEnemyMoveTimer;

// AABB de dos cajas de colisión de 12x12 (COLLISION_W/H, igual que
// SimaActors_BoxFits): TRUE si el jugador y el enemigo se solapan DE
// VERDAD, no solo comparten casilla -- con la caja completa de 16x16 el
// contacto se sentiría antes de que los sprites llegaran a tocarse.
static bool8 BoxesOverlap(s16 ax, s16 ay, s16 bx, s16 by)
{
    s16 aLeft = ax + COLLISION_MARGIN_X;
    s16 aTop = ay + COLLISION_MARGIN_Y;
    s16 bLeft = bx + COLLISION_MARGIN_X;
    s16 bTop = by + COLLISION_MARGIN_Y;

    return aLeft < bLeft + COLLISION_W && bLeft < aLeft + COLLISION_W
        && aTop < bTop + COLLISION_H && bTop < aTop + COLLISION_H;
}

// Movimiento simple (Tarea 6): un paso de un píxel por el eje más alejado
// del jugador, con la misma prioridad de un solo eje que el input del
// jugador (ver SimaActors_UpdatePlayer) para no necesitar diagonales.
// Reutiliza SimaActors_BoxFits para la colisión con muros: los enemigos son
// sprites de 16x16 igual que el jugador, así que su misma caja de 12x12
// centrada vale sin duplicar la aritmética.
static void MoveEnemyToward(u8 i, s16 targetX, s16 targetY)
{
    s16 toX = targetX - sEnemyX[i];
    s16 toY = targetY - sEnemyY[i];
    s16 adx = (toX < 0) ? -toX : toX;
    s16 ady = (toY < 0) ? -toY : toY;
    s16 dx = 0, dy = 0;
    s16 nx, ny;

    if (adx > ady && toX != 0)
        dx = (toX > 0) ? SIMA_ENEMY_SPEED : -SIMA_ENEMY_SPEED;
    else if (toY != 0)
        dy = (toY > 0) ? SIMA_ENEMY_SPEED : -SIMA_ENEMY_SPEED;
    else if (toX != 0)
        dx = (toX > 0) ? SIMA_ENEMY_SPEED : -SIMA_ENEMY_SPEED;

    if (dx == 0 && dy == 0)
        return;   // ya está en la misma posición que el objetivo

    nx = sEnemyX[i] + dx;
    ny = sEnemyY[i] + dy;
    if (SimaActors_BoxFits(sEnemyFloor, nx, ny))
    {
        sEnemyX[i] = nx;
        sEnemyY[i] = ny;
    }
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
            sEnemyX[i] = (s16)ex * 16;
            sEnemyY[i] = (s16)ey * 16;
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
    }

    sEnemyAnimTimer = 0;
    sEnemyAnimStep = 0;
    sEnemyMoveTimer = 0;
}

void SimaActors_UpdateEnemies(void)
{
    u8 i;
    bool8 moveNow;
    // Golpe del jugador (Tarea 7): se calcula UNA vez por frame, no por
    // enemigo -- AttackHitboxActive/SimaActors_WeaponHitbox no dependen de
    // qué enemigo se esté mirando, así que repetir la cuenta 3 veces sería
    // trabajo idéntico tirado.
    bool8 attackHit = AttackHitboxActive();
    s16 hitX = 0, hitY = 0;

    if (attackHit)
        SimaActors_WeaponHitbox(sAttackFacing, sPlayerX, sPlayerY, &hitX, &hitY);

    if (sPlayerInvulnTimer > 0)
        sPlayerInvulnTimer--;

    sEnemyAnimTimer++;
    if (sEnemyAnimTimer >= SIMA_ENEMY_ANIM_PERIOD)
    {
        sEnemyAnimTimer = 0;
        sEnemyAnimStep ^= 1;
    }

    sEnemyMoveTimer++;
    moveNow = (sEnemyMoveTimer >= SIMA_ENEMY_MOVE_PERIOD);
    if (moveNow)
        sEnemyMoveTimer = 0;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        struct Sprite *sprite;

        // Cadáver en curso (Tarea 7): no se mueve, no daña por contacto, solo
        // anima la muerte y cuenta atrás hasta que toca destruir el sprite.
        // Va ANTES del guard de sEnemyAlive porque un enemigo muriendo ya
        // tiene sEnemyAlive en FALSE (ver más abajo) pero su sprite sigue
        // vivo unos frames más.
        if (sEnemyDeathTimer[i] > 0)
        {
            sEnemyDeathTimer[i]--;
            sprite = &gSprites[sEnemySpriteId[i]];
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
        // frame exacto del golpe, igual que ya hacían antes de esta tarea.
        if (attackHit && BoxesOverlap(hitX, hitY, sEnemyX[i], sEnemyY[i]))
        {
            sEnemyAlive[i] = FALSE;
            sEnemyDeathTimer[i] = SIMA_ENEMY_DEATH_FRAMES;
            continue;  // sin movimiento ni daño de contacto en el frame en que muere
        }

        if (moveNow)
            MoveEnemyToward(i, sPlayerX, sPlayerY);

        // Daño por contacto: un solo golpe por ventana de invulnerabilidad,
        // sin importar cuántos enemigos se solapen con el jugador en el
        // mismo frame -- el guard de sPlayerInvulnTimer ya lo impide para
        // el resto de este bucle en cuanto el primero conecta.
        if (sPlayerInvulnTimer == 0 && BoxesOverlap(sPlayerX, sPlayerY, sEnemyX[i], sEnemyY[i]))
        {
            sPlayerHP = SimaActors_ApplyDamage(sPlayerHP, SIMA_ENEMY_CONTACT_DAMAGE);
            sPlayerInvulnTimer = SIMA_INVULN_FRAMES;
        }

        sprite = &gSprites[sEnemySpriteId[i]];
        sprite->oam.tileNum = sprite->sheetTileStart +
            (sEnemyAnimStep ? ENEMY_FRAME_IDLE_B : ENEMY_FRAME_IDLE_A);
        sprite->x = sEnemyX[i] + 8;
        sprite->y = sEnemyY[i] + 8;
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
