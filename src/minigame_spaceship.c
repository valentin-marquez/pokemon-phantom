// Include headers in logical groups
#include "global.h"
#include "random.h"
// Core GBA headers
#include "gba/m4a_internal.h"
// Game system headers
#include "malloc.h"
#include "bg.h"
#include "decompress.h"
#include "gpu_regs.h"
#include "main.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "task.h"
// Game feature headers
#include "battle.h"
#include "title_screen.h"
#include "menu_helpers.h"
#include "main_menu.h"
// Constants
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/characters.h"
// Minigame headers
#include "minigame_spaceship.h"
#include "minigame_countdown.h"
#include "minigame_pre.h"
#include <string.h>

// Backgrounds del minijuego spaceship - 256x256px (ajustados a 240x160 en pantalla)
static const u32 gSpaceshipBg0Gfx[] = INCBIN_U32("graphics/minigame_spaceship/bg_0.4bpp.lz");
static const u32 gSpaceshipBg0Tilemap[] = INCBIN_U32("graphics/minigame_spaceship/bg_0.bin.lz");
static const u16 gSpaceshipBg0Pal[] = INCBIN_U16("graphics/minigame_spaceship/bg_0.gbapal");

static const u32 gSpaceshipBg1Gfx[] = INCBIN_U32("graphics/minigame_spaceship/bg_1.4bpp.lz");
static const u32 gSpaceshipBg1Tilemap[] = INCBIN_U32("graphics/minigame_spaceship/bg_1.bin.lz");

static const u32 gSpaceshipBg2Gfx[] = INCBIN_U32("graphics/minigame_spaceship/bg_2.4bpp.lz");
static const u32 gSpaceshipBg2Tilemap[] = INCBIN_U32("graphics/minigame_spaceship/bg_2.bin.lz");

static const u32 gSpaceshipBg3Gfx[] = INCBIN_U32("graphics/minigame_spaceship/bg_3.4bpp.lz");
static const u32 gSpaceshipBg3Tilemap[] = INCBIN_U32("graphics/minigame_spaceship/bg_3.bin.lz");

// Gráficos del jugador (nave) - Un solo sprite de 32x32 con 4 frames de 16x16
// Organización:
// +----------------+----------------+
// |  Frame 1 (idle)|  Frame 2 (up)  |
// |    (16x16)     |    (16x16)     |
// +----------------+----------------+
// | Frame 3 (idle) | Frame 4 (down) |
// |    (16x16)     |    (16x16)     |
// +----------------+----------------+
static const u32 gPlayerShipGfx[] = INCBIN_U32("graphics/minigame_spaceship/player_ship.4bpp.lz");
static const u16 gPlayerShipPal[] = INCBIN_U16("graphics/minigame_spaceship/player_ship.gbapal");

// Tags específicos para los sprites del jugador (evita conflicto con countdown)
#define TAG_PLAYER_SHIP  0x2000

typedef struct
{
    u8 gameState;
    u16 score;
    u8 timeElapsed;
    bool8 isGameActive;
    bool8 showCountdown;
    s16 bg0ScrollX;  // Parallax scrolling para BG0 (más rápido)
    s16 bg1ScrollX;  // Parallax scrolling para BG1 (medio)
    s16 bg2ScrollX;  // Parallax scrolling para BG2 (más lento)
    s16 bg2LogicalScrollX;  // Control lógico para BG2 (puede ser mayor que el ancho del tilemap)
    u8 bg2FrameCounter;     // Contador para movimiento cada 2 frames
    
    // Datos del jugador
    u8 playerSpriteId;      // ID del sprite del jugador
    s16 playerX;            // Posición X del jugador
    s16 playerY;            // Posición Y del jugador
    u8 currentAnimationFrame; // Frame actual de animación
    u8 animationCounter;     // Contador para animación idle
} SpaceshipGameState;

static EWRAM_DATA SpaceshipGameState *sSpaceshipState = NULL;

static void Task_HandleSpaceshipGame(u8 taskId);
static void Task_StartCountdown(u8 taskId);
static void Task_GameplayLoop(u8 taskId);
static void LoadPlayerShip(void);
static void UpdatePlayerShip(void);
static void SetPlayerAnimationFrame(u8 frame);

static const struct BgTemplate sSpaceshipBgTemplates[] = {
    // BG0 - UI/HUD layer (front) - paleta 0
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0,
    },
    // BG1 - Game objects layer - paleta 0
    {
        .bg = 1,
        .charBaseIndex = 1,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0,
    },
    // BG2 - Mid background layer - paleta 0
    {
        .bg = 2,
        .charBaseIndex = 2,
        .mapBaseIndex = 29,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0,
    },
    // BG3 - Far background layer (back) - paleta 0
    {
        .bg = 3,
        .charBaseIndex = 3,
        .mapBaseIndex = 28,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 3,
        .baseTile = 0,
    }
};

// Definiciones para el sprite del jugador
#define PLAYER_WIDTH 16    // Cada frame es 16x16
#define PLAYER_HEIGHT 16   // Cada frame es 16x16
#define PLAYER_SPEED 2
#define PLAYER_FRAME_IDLE1 0
#define PLAYER_FRAME_UP 1
#define PLAYER_FRAME_IDLE2 2
#define PLAYER_FRAME_DOWN 3
#define PLAYER_FRAME_COUNT 4
#define PLAYER_ANIMATION_SPEED 30  // Frames entre cambios de animación idle

// Tiles por fila/columna para cada frame 16x16
#define TILES_PER_FRAME_WIDTH 2  // 16px / 8 = 2 tiles de ancho por frame
#define TILES_PER_FRAME_HEIGHT 2 // 16px / 8 = 2 tiles de alto por frame

// Desplazamiento en VRAM para evitar conflicto con countdown
// El countdown usa los primeros 32 tiles (0x000-0x3FF), así que empezamos en 0x400
#define PLAYER_VRAM_OFFSET (32 * 32)  // 32 tiles * 32 bytes/tile

// Datos OAM para el jugador - cambiado a 16x16 para mostrar cada frame individual
static const struct OamData sPlayerShipOam = {
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(16x16),
    .tileNum = 0, // Este valor se ignora porque usaremos animaciones
    .priority = 1,
    .paletteNum = 0,  // Se actualizará después de cargar la paleta
    .affineParam = 0
};

// Tabla de animaciones (necesaria para SpriteTemplate)
static const union AnimCmd sPlayerShipAnimCmd_0[] = {
    ANIMCMD_FRAME(0, 0),
    ANIMCMD_END
};

static const union AnimCmd *const sPlayerShipAnimTable[] = {
    sPlayerShipAnimCmd_0
};

// Template para crear sprites del jugador
static const struct SpriteTemplate sPlayerShipSpriteTemplate = {
    .tileTag = TAG_PLAYER_SHIP,
    .paletteTag = TAG_PLAYER_SHIP,
    .oam = &sPlayerShipOam,
    .anims = sPlayerShipAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static void VBlankCB_Spaceship(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
    
    // Establecer offsets horizontales para efecto parallax
    if (sSpaceshipState != NULL)
    {
        SetGpuReg(REG_OFFSET_BG0HOFS, sSpaceshipState->bg0ScrollX);
        SetGpuReg(REG_OFFSET_BG1HOFS, sSpaceshipState->bg1ScrollX);
        SetGpuReg(REG_OFFSET_BG2HOFS, sSpaceshipState->bg2ScrollX);
        // BG3 permanece estático (0 por defecto)
    }
}

static void CB2_SpaceshipMain(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void ResetGPURegisters(void)
{
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
    SetGpuReg(REG_OFFSET_BG2HOFS, 0);
    SetGpuReg(REG_OFFSET_BG2VOFS, 0);
    SetGpuReg(REG_OFFSET_BG3HOFS, 0);
    SetGpuReg(REG_OFFSET_BG3VOFS, 0);
}

static void LoadSpaceshipGraphics(void)
{
    // Inicializar BGs desde templates (método moderno)
    InitBgsFromTemplates(0, sSpaceshipBgTemplates, ARRAY_COUNT(sSpaceshipBgTemplates));
    
    // Asignar buffers para tilemaps
    SetBgTilemapBuffer(0, Alloc(BG_SCREEN_SIZE));
    SetBgTilemapBuffer(1, Alloc(BG_SCREEN_SIZE));
    SetBgTilemapBuffer(2, Alloc(BG_SCREEN_SIZE));
    SetBgTilemapBuffer(3, Alloc(BG_SCREEN_SIZE));

    // Cargar la paleta compartida en la paleta 0 (todos los BGs la usarán)
    LoadPalette(gSpaceshipBg0Pal, BG_PLTT_ID(0), PLTT_SIZE_4BPP);

    // Cargar gráficos usando LZ77 decompresión (método moderno)
    LZ77UnCompVram(gSpaceshipBg0Gfx, (void *)BG_CHAR_ADDR(0));
    LZ77UnCompVram(gSpaceshipBg1Gfx, (void *)BG_CHAR_ADDR(1));
    LZ77UnCompVram(gSpaceshipBg2Gfx, (void *)BG_CHAR_ADDR(2));
    LZ77UnCompVram(gSpaceshipBg3Gfx, (void *)BG_CHAR_ADDR(3));

    // Cargar tilemaps usando LZ77 decompresión (método moderno)
    LZ77UnCompVram(gSpaceshipBg0Tilemap, (void *)BG_SCREEN_ADDR(31));
    LZ77UnCompVram(gSpaceshipBg1Tilemap, (void *)BG_SCREEN_ADDR(30));
    LZ77UnCompVram(gSpaceshipBg2Tilemap, (void *)BG_SCREEN_ADDR(29));
    LZ77UnCompVram(gSpaceshipBg3Tilemap, (void *)BG_SCREEN_ADDR(28));
    
    // Mostrar todos los backgrounds
    ShowBg(0);
    ShowBg(1);
    ShowBg(2);
    ShowBg(3);
}

static void LoadPlayerShip(void)
{
    u16 *vramDest = (u16 *)(OBJ_VRAM0 + PLAYER_VRAM_OFFSET);
    const u16 *src = (const u16 *)gPlayerShipGfx; // LZ ya descomprimido más abajo
    u16 i, frame, tileInFrame;
    u16 srcTile, dstTile;

    // Cargar paleta del jugador
    struct SpritePalette playerShipPalette = {gPlayerShipPal, TAG_PLAYER_SHIP};
    LoadSpritePalette(&playerShipPalette);

    // Descomprimir gráficos a un buffer temporal (en VRAM o en RAM si prefieres)
    // Aquí descomprimimos directamente a destino final, pero luego reordenamos
    LZ77UnCompVram(gPlayerShipGfx, (void *)(OBJ_VRAM0 + PLAYER_VRAM_OFFSET));

    // Ahora reordenamos los tiles para que cada frame 16x16 tenga 4 tiles contiguos
    // El sprite original es 32x32 = 16 tiles (4x4)
    // Queremos reorganizarlos en 4 frames de 4 tiles cada uno, en este orden:
    // Frame 0 (idle1): tiles 0,1,4,5
    // Frame 1 (up):    tiles 2,3,6,7
    // Frame 2 (idle2): tiles 8,9,12,13
    // Frame 3 (down):  tiles 10,11,14,15

    // Buffer temporal para no sobreescribir mientras reordenamos
    u16 tempTiles[16 * 16]; // 16 tiles * 32 bytes/tile = 512 bytes, pero trabajamos con u16

    // Copiar todos los tiles originales a buffer temporal
    for (i = 0; i < 16; i++) // 16 tiles en total
    {
        for (u16 j = 0; j < 16; j++) // 16 halfwords por tile (32 bytes)
        {
            tempTiles[i * 16 + j] = vramDest[i * 16 + j];
        }
    }

    // Mapeo de qué tiles originales van a cada frame
    const u8 frameTileMap[4][4] = {
        {0, 1, 4, 5},   // Frame 0: idle1
        {2, 3, 6, 7},   // Frame 1: up
        {8, 9, 12, 13}, // Frame 2: idle2
        {10, 11, 14, 15} // Frame 3: down
    };

    // Reescribir VRAM con los tiles reordenados
    for (frame = 0; frame < PLAYER_FRAME_COUNT; frame++)
    {
        for (tileInFrame = 0; tileInFrame < 4; tileInFrame++)
        {
            srcTile = frameTileMap[frame][tileInFrame];
            dstTile = frame * 4 + tileInFrame; // Cada frame ocupa 4 tiles contiguos

            for (i = 0; i < 16; i++) // Copiar 32 bytes (16 halfwords) por tile
            {
                vramDest[dstTile * 16 + i] = tempTiles[srcTile * 16 + i];
            }
        }
    }

    // Crear el sprite
    sSpaceshipState->playerSpriteId = CreateSprite(&sPlayerShipSpriteTemplate, 0, 0, 0);

    // Obtener índice de paleta
    u8 paletteIndex = IndexOfSpritePaletteTag(TAG_PLAYER_SHIP);

    // Configurar OAM
    gSprites[sSpaceshipState->playerSpriteId].oam.shape = SPRITE_SHAPE(16x16);
    gSprites[sSpaceshipState->playerSpriteId].oam.size = SPRITE_SIZE(16x16);
    gSprites[sSpaceshipState->playerSpriteId].oam.paletteNum = paletteIndex;

    // Posición inicial
    sSpaceshipState->playerX = 40;
    sSpaceshipState->playerY = 80;
    sSpaceshipState->currentAnimationFrame = PLAYER_FRAME_IDLE1;
    sSpaceshipState->animationCounter = 0;

    // Establecer frame inicial
    SetPlayerAnimationFrame(PLAYER_FRAME_IDLE1);

    // Actualizar posición
    UpdatePlayerShip();
}
static void SetPlayerAnimationFrame(u8 frame)
{
    if (sSpaceshipState == NULL) return;

    u16 baseTile = PLAYER_VRAM_OFFSET / 32; // Tiles base en VRAM
    u16 frameTileNum = baseTile + (frame * 4); // Cada frame usa 4 tiles contiguos

    gSprites[sSpaceshipState->playerSpriteId].oam.tileNum = frameTileNum;
    sSpaceshipState->currentAnimationFrame = frame;
}

static void UpdatePlayerShip(void)
{
    if (sSpaceshipState == NULL) return;
    
    // Limitar posición del jugador dentro de la pantalla
    // Ajustamos los límites para que la nave de 16x16 no salga de la pantalla
    if (sSpaceshipState->playerY < 8) sSpaceshipState->playerY = 8;
    if (sSpaceshipState->playerY > 152) sSpaceshipState->playerY = 152; // 160 - 8
    
    // Actualizar posición en el sprite (centrar el sprite de 16x16)
    gSprites[sSpaceshipState->playerSpriteId].x = sSpaceshipState->playerX - (PLAYER_WIDTH / 2);
    gSprites[sSpaceshipState->playerSpriteId].y = sSpaceshipState->playerY - (PLAYER_HEIGHT / 2);
}

static void Task_StartCountdown(u8 taskId)
{
    switch (gTasks[taskId].data[0])
    {
    case 0:
        if (!gPaletteFade.active)
        {
            // Iniciar countdown usando el sistema de countdown existente
            StartMinigameCountdown(0x1000, 0x1001, 120, 80, 0);
            gTasks[taskId].data[0]++;
        }
        break;

    case 1:
        if (!IsMinigameCountdownRunning())
        {
            // El countdown ha terminado, iniciar el juego
            sSpaceshipState->gameState = SPACESHIP_STATE_GAMEPLAY;
            sSpaceshipState->isGameActive = TRUE;
            DestroyTask(taskId);
            CreateTask(Task_GameplayLoop, 0);
        }
        break;
    }
}

static void Task_GameplayLoop(u8 taskId)
{
    // Lógica principal del juego spaceship
    switch (gTasks[taskId].data[0])
    {
    case 0:
        // Inicialización del gameplay
        PlayBGM(MUS_GAME_CORNER);
        // Reiniciar contadores de scroll
        sSpaceshipState->bg0ScrollX = 0;
        sSpaceshipState->bg1ScrollX = 0;
        sSpaceshipState->bg2ScrollX = 0;
        sSpaceshipState->bg2LogicalScrollX = 0;
        sSpaceshipState->bg2FrameCounter = 0;
        
        // Inicializar jugador
        LoadPlayerShip();
        
        gTasks[taskId].data[0]++;
        break;

    case 1:
        // Loop principal del juego
        if (sSpaceshipState->isGameActive)
        {
            // Actualizar efecto parallax (capas más cercanas se mueven más rápido)
            sSpaceshipState->bg0ScrollX += 3;  // BG0 (más cercano) se mueve más rápido
            
            // BG1 se mueve a velocidad media
            sSpaceshipState->bg1ScrollX += 2;
            
            // BG2 se mueve cada 2 frames para simular movimiento lento (efecto planeta)
            sSpaceshipState->bg2FrameCounter++;
            if (sSpaceshipState->bg2FrameCounter >= 2) {
                sSpaceshipState->bg2LogicalScrollX += 1;
                sSpaceshipState->bg2FrameCounter = 0;
                
                // Calcular el offset real para el hardware (módulo 256)
                sSpaceshipState->bg2ScrollX = sSpaceshipState->bg2LogicalScrollX % 256;
                
                // Reiniciar el desplazamiento lógico solo después de que el fondo haya salido completamente de la pantalla
                // 496 = 256 (ancho del tilemap) + 240 (ancho de la pantalla)
                if (sSpaceshipState->bg2LogicalScrollX >= 496) {
                    sSpaceshipState->bg2LogicalScrollX = 0;
                    sSpaceshipState->bg2ScrollX = 0;
                }
            }
            
            // Reiniciar scroll para BG0 y BG1 cuando el fondo sale de pantalla (256px de ancho)
            if (sSpaceshipState->bg0ScrollX >= 256) sSpaceshipState->bg0ScrollX -= 256;
            if (sSpaceshipState->bg1ScrollX >= 256) sSpaceshipState->bg1ScrollX -= 256;
            
            // Actualizar jugador
            {
                // Manejar movimiento vertical
                if (gMain.heldKeys & DPAD_UP) {
                    sSpaceshipState->playerY -= PLAYER_SPEED;
                    SetPlayerAnimationFrame(PLAYER_FRAME_UP);
                }
                else if (gMain.heldKeys & DPAD_DOWN) {
                    sSpaceshipState->playerY += PLAYER_SPEED;
                    SetPlayerAnimationFrame(PLAYER_FRAME_DOWN);
                }
                else {
                    // Alternar entre frames idle
                    sSpaceshipState->animationCounter++;
                    if (sSpaceshipState->animationCounter >= PLAYER_ANIMATION_SPEED) {
                        sSpaceshipState->animationCounter = 0;
                        sSpaceshipState->currentAnimationFrame = 
                            (sSpaceshipState->currentAnimationFrame == PLAYER_FRAME_IDLE1) ? 
                            PLAYER_FRAME_IDLE2 : PLAYER_FRAME_IDLE1;
                        SetPlayerAnimationFrame(sSpaceshipState->currentAnimationFrame);
                    }
                }
                
                // Actualizar posición del jugador
                UpdatePlayerShip();
            }
            
            // Aquí iría la lógica principal del juego spaceship
            // Por ahora, solo un placeholder que termina después de unos segundos
            gTasks[taskId].data[1]++;
            
            if (gTasks[taskId].data[1] > 1800) // ~30 segundos a 60 FPS
            {
                sSpaceshipState->isGameActive = FALSE;
                gTasks[taskId].data[0] = 2;
            }
            
            // Detectar input para salir
            if (JOY_NEW(START_BUTTON))
            {
                sSpaceshipState->isGameActive = FALSE;
                gTasks[taskId].data[0] = 2;
            }
        }
        break;

    case 2:
        // Terminar el juego
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].data[0]++;
        break;

    case 3:
        if (!gPaletteFade.active)
        {
            // Limpiar recursos del jugador
            if (sSpaceshipState != NULL) {
                if (sSpaceshipState->playerSpriteId != MAX_SPRITES) {
                    DestroySprite(&gSprites[sSpaceshipState->playerSpriteId]);
                }
                
                // Liberar la paleta del jugador
                FreeSpritePaletteByTag(TAG_PLAYER_SHIP);
            }
            
            // Regresar al título o menú principal
            SetMainCallback2(CB2_InitTitleScreen);
            DestroyTask(taskId);
        }
        break;
    }
}

static void Task_HandleSpaceshipGame(u8 taskId)
{
    switch (sSpaceshipState->gameState)
    {
    case SPACESHIP_STATE_INIT:
        // Estado inicial, comenzar countdown
        sSpaceshipState->gameState = SPACESHIP_STATE_GAMEPLAY;
        DestroyTask(taskId);
        CreateTask(Task_StartCountdown, 0);
        break;

    case SPACESHIP_STATE_GAMEPLAY:
        // El gameplay está siendo manejado por otra task
        break;

    case SPACESHIP_STATE_EXIT:
        // Salir del minijuego
        DestroyTask(taskId);
        break;
    }
}

void CB2_InitMinigameSpaceship(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        ResetGPURegisters();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetSpriteData();
        ResetTasks();
        ResetBgsAndClearDma3BusyFlags(0);

        // Limpiar memoria
        DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
        DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
        DmaFill16(3, 0, (void *)PLTT, PLTT_SIZE);

        // Inicializar estado del juego
        sSpaceshipState = Alloc(sizeof(SpaceshipGameState));
        if (sSpaceshipState != NULL)
        {
            memset(sSpaceshipState, 0, sizeof(SpaceshipGameState));
            sSpaceshipState->gameState = SPACESHIP_STATE_INIT;
            sSpaceshipState->isGameActive = FALSE;
            sSpaceshipState->playerSpriteId = MAX_SPRITES;  // Indicador de sprite no creado
            // Los valores de scroll se inicializan en 0 por memset
        }
        gMain.state++;
        break;

    case 1:
        LoadSpaceshipGraphics();
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON | DISPCNT_BG3_ON | DISPCNT_OBJ_1D_MAP);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);

        // Configurar callbacks
        SetVBlankCallback(VBlankCB_Spaceship);
        CreateTask(Task_HandleSpaceshipGame, 0);
        SetMainCallback2(CB2_SpaceshipMain);

        gMain.state = 0;
        break;
    }
}

// Funciones auxiliares públicas
void MinigameSpaceship_StopAllEffects(void)
{
    if (sSpaceshipState != NULL)
    {
        sSpaceshipState->isGameActive = FALSE;
    }
}

bool8 MinigameSpaceship_IsActive(void)
{
    return (sSpaceshipState != NULL && sSpaceshipState->isGameActive);
}