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

enum SimaFacing
{
    SIMA_FACING_DOWN,
    SIMA_FACING_UP,
    SIMA_FACING_LEFT,
    SIMA_FACING_RIGHT,
};

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

static void UpdatePlayerSprite(void);

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

    LoadSpriteSheet(&sSheet_SimaPlayer);
    LoadSpritePalette(&sPal_SimaPlayer);

    // CreateSprite posiciona por el CENTRO del sprite, no por la esquina
    // superior izquierda (ver CalcCenterToCornerVec en src/sprite.c): +8 en
    // cada eje porque el sprite es 16x16.
    sPlayerSpriteId = CreateSprite(&sTmpl_SimaPlayer, sPlayerX + 8, sPlayerY + 8, 0);
    sPlayerActive = (sPlayerSpriteId != MAX_SPRITES);

    if (sPlayerActive)
        UpdatePlayerSprite();
}

void SimaActors_UpdatePlayer(void)
{
    u8 newFacing = sPlayerFacing;
    s16 dx = 0, dy = 0;
    bool8 moved = FALSE;

    if (!sPlayerActive)
        return;  // SimaActors_InitPlayer no se llamó, o CreateSprite se quedó sin presupuesto (MAX_SPRITES)

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
}
