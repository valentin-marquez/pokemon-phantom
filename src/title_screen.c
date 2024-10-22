#include "global.h"
#include "battle.h"
#include "title_screen.h"
#include "sprite.h"
#include "gba/m4a_internal.h"
#include "clear_save_data_menu.h"
#include "decompress.h"
#include "event_data.h"
#include "intro.h"
#include "m4a.h"
#include "main.h"
#include "main_menu.h"
#include "palette.h"
#include "reset_rtc_screen.h"
#include "berry_fix_program.h"
#include "sound.h"
#include "sprite.h"
#include "task.h"
#include "scanline_effect.h"
#include "gpu_regs.h"
#include "trig.h"
#include "graphics.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/rgb.h"

enum {
    TAG_VERSION = 1000,
    TAG_PRESS_START_COPYRIGHT,
    TAG_PRESS_START_OWNER_NAME,
    TAG_LOGO_SHINE,
};


#define VERSION_BANNER_RIGHT_TILEOFFSET 64
#define VERSION_BANNER_LEFT_X 98
#define VERSION_BANNER_RIGHT_X 162
#define VERSION_BANNER_Y 2
#define VERSION_BANNER_Y_GOAL 66
#define START_BANNER_X 128

#define CLEAR_SAVE_BUTTON_COMBO (B_BUTTON | SELECT_BUTTON | DPAD_UP)
#define RESET_RTC_BUTTON_COMBO (B_BUTTON | SELECT_BUTTON | DPAD_LEFT)
#define BERRY_UPDATE_BUTTON_COMBO (B_BUTTON | SELECT_BUTTON)
#define A_B_START_SELECT (A_BUTTON | B_BUTTON | START_BUTTON | SELECT_BUTTON)

#define STAR_COLOR_BACKGROUND 2
#define STAR_COLOR_DIM       3
#define STAR_COLOR_MEDIUM    4
#define STAR_COLOR_BRIGHT    5

// Espacios libres para almacenar copias de colores
#define COLOR_BACKUP_START   11
#define MAX_SEQUENCE_LENGTH  6


static void MainCB2(void);
static void Task_TitleScreenPhase1(u8);
static void Task_TitleScreenPhase2(u8);
static void Task_TitleScreenPhase3(u8);
static void CB2_GoToMainMenu(void);
static void CB2_GoToClearSaveDataScreen(void);
static void CB2_GoToResetRtcScreen(void);
static void CB2_GoToBerryFixScreen(void);
static void CB2_GoToCopyrightScreen(void);

static void UpdateStarColors();
static void CreateStarAnimationTask(void);
static void Task_AnimateStars(u8);



static void SpriteCB_PressStartOwnerNameBanner(struct Sprite *sprite);
static void SpriteCB_PokemonLogoShine(struct Sprite *sprite);

// const rom data
static const u16 sUnusedUnknownPal[] = INCBIN_U16("graphics/title_screen/unused.gbapal");

static const u32 sTitleScreenRayquazaGfx[] = INCBIN_U32("graphics/title_screen/rayquaza.4bpp.lz");
static const u32 sTitleScreenRayquazaTilemap[] = INCBIN_U32("graphics/title_screen/rayquaza.bin.lz");
static const u32 sTitleScreenLogoShineGfx[] = INCBIN_U32("graphics/title_screen/logo_shine.4bpp.lz");
static const u32 sTitleScreenCloudsGfx[] = INCBIN_U32("graphics/title_screen/clouds.4bpp.lz");


// background de la portada del juego, esta destinado a estar en el bg0 y usar la paleta E( 14 )
static const u32 gBackgroundGfx[] = INCBIN_U32("graphics/new_title_screen/bg0_tiles.4bpp.lz");
static const u32 gBackgroundTilemap[] = INCBIN_U32("graphics/new_title_screen/bg0_tiles.bin.lz");
static const u16 gBackgroundPal[] = INCBIN_U16("graphics/new_title_screen/bg0_tiles.gbapal");


const u32 gTitleScreenPokemonLogoGfx[]     = INCBIN_U32("graphics/new_title_screen/pokemon_logo.8bpp.lz");
const u32 gTitleScreenPokemonLogoTilemap[] = INCBIN_U32("graphics/new_title_screen/pokemon_logo.bin.lz");


// Press Start y Owner Name, sprite no necesita tilemap, ademas ahora es 32x8 los segementos donde los primeros 5 segmentos son para el press start y la animacion tradicional de parpadeo, luego viene el owner name en un solo segmento que seria el segmento 6 
const u16 gTitleScreenPressStartPal[]      = INCBIN_U16("graphics/new_title_screen/press_start.gbapal");
const u32 gTitleScreenPressStartGfx[]      = INCBIN_U32("graphics/new_title_screen/press_start.4bpp.lz");


// Used to blend "Emerald Version" as it passes over over the Pokémon banner.
// Also used by the intro to blend the Game Freak name/logo in and out as they appear and disappear
const u16 gTitleScreenAlphaBlend[64] =
{
    BLDALPHA_BLEND(16, 0),
    BLDALPHA_BLEND(16, 1),
    BLDALPHA_BLEND(16, 2),
    BLDALPHA_BLEND(16, 3),
    BLDALPHA_BLEND(16, 4),
    BLDALPHA_BLEND(16, 5),
    BLDALPHA_BLEND(16, 6),
    BLDALPHA_BLEND(16, 7),
    BLDALPHA_BLEND(16, 8),
    BLDALPHA_BLEND(16, 9),
    BLDALPHA_BLEND(16, 10),
    BLDALPHA_BLEND(16, 11),
    BLDALPHA_BLEND(16, 12),
    BLDALPHA_BLEND(16, 13),
    BLDALPHA_BLEND(16, 14),
    BLDALPHA_BLEND(16, 15),
    BLDALPHA_BLEND(15, 16),
    BLDALPHA_BLEND(14, 16),
    BLDALPHA_BLEND(13, 16),
    BLDALPHA_BLEND(12, 16),
    BLDALPHA_BLEND(11, 16),
    BLDALPHA_BLEND(10, 16),
    BLDALPHA_BLEND(9, 16),
    BLDALPHA_BLEND(8, 16),
    BLDALPHA_BLEND(7, 16),
    BLDALPHA_BLEND(6, 16),
    BLDALPHA_BLEND(5, 16),
    BLDALPHA_BLEND(4, 16),
    BLDALPHA_BLEND(3, 16),
    BLDALPHA_BLEND(2, 16),
    BLDALPHA_BLEND(1, 16),
    BLDALPHA_BLEND(0, 16),
    [32 ... 63] = BLDALPHA_BLEND(0, 16)
};

enum StarColorIndices {
    COLOR_IDX_BACKGROUND = 2,
    COLOR_IDX_DIM = 3,
    COLOR_IDX_MEDIUM = 4,
    COLOR_IDX_BRIGHT = 5
};

struct ColorSequence {
    u8 sequence[MAX_SEQUENCE_LENGTH];
    u8 length;
    u8 currentIndex;
};

struct StarAnimationState {
    u16 savedColors[4];           // Guarda los colores originales (2,3,4,5)
    struct ColorSequence sequences[3]; // Secuencias para colores 3,4,5
    u8 frameCounter;
    u8 transitionProgress;
    bool8 animationActive;
};

static struct StarAnimationState sStarAnim;

static void InitColorSequences(void)
{
    // Secuencia para color 3: 4,5,4,3,2
    sStarAnim.sequences[0].sequence[0] = COLOR_IDX_MEDIUM;  // 4
    sStarAnim.sequences[0].sequence[1] = COLOR_IDX_BRIGHT;  // 5
    sStarAnim.sequences[0].sequence[2] = COLOR_IDX_MEDIUM;  // 4
    sStarAnim.sequences[0].sequence[3] = COLOR_IDX_DIM;     // 3
    sStarAnim.sequences[0].sequence[4] = COLOR_IDX_BACKGROUND; // 2
    sStarAnim.sequences[0].length = 5;
    sStarAnim.sequences[0].currentIndex = 0;

    // Secuencia para color 4: 5,4,3,2,3,4
    sStarAnim.sequences[1].sequence[0] = COLOR_IDX_BRIGHT;    // 5
    sStarAnim.sequences[1].sequence[1] = COLOR_IDX_MEDIUM;    // 4
    sStarAnim.sequences[1].sequence[2] = COLOR_IDX_DIM;       // 3
    sStarAnim.sequences[1].sequence[3] = COLOR_IDX_BACKGROUND; // 2
    sStarAnim.sequences[1].sequence[4] = COLOR_IDX_DIM;       // 3
    sStarAnim.sequences[1].sequence[5] = COLOR_IDX_MEDIUM;    // 4
    sStarAnim.sequences[1].length = 6;
    sStarAnim.sequences[1].currentIndex = 0;

    // Secuencia para color 5: 4,3,2,3,4
    sStarAnim.sequences[2].sequence[0] = COLOR_IDX_MEDIUM;    // 4
    sStarAnim.sequences[2].sequence[1] = COLOR_IDX_DIM;       // 3
    sStarAnim.sequences[2].sequence[2] = COLOR_IDX_BACKGROUND; // 2
    sStarAnim.sequences[2].sequence[3] = COLOR_IDX_DIM;       // 3
    sStarAnim.sequences[2].sequence[4] = COLOR_IDX_MEDIUM;    // 4
    sStarAnim.sequences[2].length = 5;
    sStarAnim.sequences[2].currentIndex = 0;
}

void InitStarAnimation(void)
{
    // Guardar los colores originales
    sStarAnim.savedColors[0] = gPlttBufferUnfaded[BG_PLTT_ID(14) + COLOR_IDX_BACKGROUND];
    sStarAnim.savedColors[1] = gPlttBufferUnfaded[BG_PLTT_ID(14) + COLOR_IDX_DIM];
    sStarAnim.savedColors[2] = gPlttBufferUnfaded[BG_PLTT_ID(14) + COLOR_IDX_MEDIUM];
    sStarAnim.savedColors[3] = gPlttBufferUnfaded[BG_PLTT_ID(14) + COLOR_IDX_BRIGHT];
    
    InitColorSequences();
    
    sStarAnim.frameCounter = 0;
    sStarAnim.transitionProgress = 0;
    sStarAnim.animationActive = TRUE;
}



static const struct CompressedSpriteSheet sSpriteSheet_EmeraldVersion[] =
{
    {
        .data = gTitleScreenEmeraldVersionGfx,
        .size = 0x1000,
        .tag = TAG_VERSION
    },
    {},
};

static const struct OamData sOamData_PressStartOwnerName =
{
    .y = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x8),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x8),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
    .affineParam = 0,
};


static const union AnimCmd sAnim_PressStart_0[] =
{
    ANIMCMD_FRAME(0, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_1[] =
{
    ANIMCMD_FRAME(4, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_2[] =
{
    ANIMCMD_FRAME(8, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_3[] =
{
    ANIMCMD_FRAME(12, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_4[] =
{
    ANIMCMD_FRAME(16, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_OwnerName_0[] =
{
    ANIMCMD_FRAME(20, 4),
    ANIMCMD_END,
};


// The "Press Start" and copyright graphics are each 5 32x8 segments long
#define NUM_PRESS_START_FRAMES 5
#define NUM_OWNER_NAME_FRAMES 1

static const union AnimCmd *const sStartOwnerNameAnimTable[NUM_PRESS_START_FRAMES + NUM_OWNER_NAME_FRAMES] =
{
    sAnim_PressStart_0,
    sAnim_PressStart_1,
    sAnim_PressStart_2,
    sAnim_PressStart_3,
    sAnim_PressStart_4,
    [NUM_PRESS_START_FRAMES] = sAnim_OwnerName_0,
};


static const struct SpriteTemplate sStartOwnerNameSpriteTemplate =
{
    .tileTag = TAG_PRESS_START_OWNER_NAME,
    .paletteTag = TAG_PRESS_START_OWNER_NAME,
    .oam = &sOamData_PressStartOwnerName,
    .anims = sStartOwnerNameAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_PressStartOwnerNameBanner,
};

static const struct CompressedSpriteSheet sSpriteSheet_PressStart[] =
{
    {
        .data = gTitleScreenPressStartGfx,
        .size = 0x600,  // Ajusta este tamaño basado en tu nuevo gráfico
        .tag = TAG_PRESS_START_OWNER_NAME
    },
    {},
};

static const struct SpritePalette sSpritePalette_PressStart[] =
{
    {
        .data = gTitleScreenPressStartPal,
        .tag = TAG_PRESS_START_OWNER_NAME
    },
    {},
};


static const struct OamData sPokemonLogoShineOamData =
{
    .y = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
    .affineParam = 0,
};

static const union AnimCmd sPokemonLogoShineAnimSequence[] =
{
    ANIMCMD_FRAME(0, 4),
    ANIMCMD_END,
};

static const union AnimCmd *const sPokemonLogoShineAnimTable[] =
{
    sPokemonLogoShineAnimSequence,
};

static const struct SpriteTemplate sPokemonLogoShineSpriteTemplate =
{
    .tileTag = TAG_LOGO_SHINE,
    .paletteTag = TAG_PRESS_START_COPYRIGHT,
    .oam = &sPokemonLogoShineOamData,
    .anims = sPokemonLogoShineAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_PokemonLogoShine,
};

static const struct CompressedSpriteSheet sPokemonLogoShineSpriteSheet[] =
{
    {
        .data = sTitleScreenLogoShineGfx,
        .size = 0x800,
        .tag = TAG_LOGO_SHINE
    },
    {},
};

// Task data for the main title screen tasks (Task_TitleScreenPhase#)
#define tCounter    data[0]
#define tSkipToNext data[1]
#define tPointless  data[2] // Incremented but never used to do anything.
#define tBg2Y       data[3]
#define tBg1Y       data[4]


// Sprite data for SpriteCB_PressStartOwnerNameBanner
#define sAnimate data[0]
#define sTimer   data[1]

static void SpriteCB_PressStartOwnerNameBanner(struct Sprite *sprite)
{
    if (sprite->sAnimate == TRUE)
    {
        // Alternate between hidden and shown every 16th frame
        if (++sprite->sTimer & 16)
            sprite->invisible = FALSE;
        else
            sprite->invisible = TRUE;
    }
    else
    {
        sprite->invisible = FALSE;
    }
}

static void CreatePressStartBanner(s16 x, s16 y)
{
    u8 i;
    u8 spriteId;

    x -= 64;
    for (i = 0; i < NUM_PRESS_START_FRAMES; i++, x += 32)
    {
        spriteId = CreateSprite(&sStartOwnerNameSpriteTemplate, x, y, 0);
        StartSpriteAnim(&gSprites[spriteId], i);
        gSprites[spriteId].sAnimate = TRUE;
    }
}

static void CreateOwnerNameBanner(s16 x, s16 y)
{
    u8 spriteId = CreateSprite(&sStartOwnerNameSpriteTemplate, x, y, 0);
    StartSpriteAnim(&gSprites[spriteId], NUM_PRESS_START_FRAMES);
}

#undef sAnimate
#undef sTimer


// Defines for SpriteCB_PokemonLogoShine
enum {
    SHINE_MODE_SINGLE_NO_BG_COLOR,
    SHINE_MODE_DOUBLE,
    SHINE_MODE_SINGLE,
};

#define SHINE_SPEED  4

#define sMode     data[0]
#define sBgColor  data[1]

static void SpriteCB_PokemonLogoShine(struct Sprite *sprite)
{
    if (sprite->x < DISPLAY_WIDTH + 32)
    {
        // In any mode except SHINE_MODE_SINGLE_NO_BG_COLOR the background
        // color will change, in addition to the shine sprite moving.
        if (sprite->sMode != SHINE_MODE_SINGLE_NO_BG_COLOR)
        {
            u16 backgroundColor;

            if (sprite->x < DISPLAY_WIDTH / 2)
            {
                // Brighten background color
                if (sprite->sBgColor < 31)
                    sprite->sBgColor++;
                if (sprite->sBgColor < 31)
                    sprite->sBgColor++;
            }
            else
            {
                // Darken background color
                if (sprite->sBgColor != 0)
                    sprite->sBgColor--;
                if (sprite->sBgColor != 0)
                    sprite->sBgColor--;
            }

            backgroundColor = _RGB(sprite->sBgColor, sprite->sBgColor, sprite->sBgColor);

            // Flash the background green for 4 frames of movement.
            // Otherwise use the updating color.
            // edit: changed 
            gPlttBufferFaded[0] = backgroundColor;
        }

        sprite->x += SHINE_SPEED;
    }
    else
    {
        // Sprite has moved fully offscreen
        gPlttBufferFaded[0] = RGB_BLACK;
        DestroySprite(sprite);
    }
}

static void SpriteCB_PokemonLogoShine_Fast(struct Sprite *sprite)
{
    if (sprite->x < DISPLAY_WIDTH + 32)
        sprite->x += SHINE_SPEED * 2;
    else
        DestroySprite(sprite);
}

static void StartPokemonLogoShine(u8 mode)
{
    u8 spriteId;

    switch (mode)
    {
    case SHINE_MODE_SINGLE_NO_BG_COLOR:
    case SHINE_MODE_SINGLE:
        // Create one regular shine sprite.
        // If mode is SHINE_MODE_SINGLE it will also change the background color.
        spriteId = CreateSprite(&sPokemonLogoShineSpriteTemplate, 0, 68, 0);
        gSprites[spriteId].oam.objMode = ST_OAM_OBJ_WINDOW;
        gSprites[spriteId].sMode = mode;
        break;
    case SHINE_MODE_DOUBLE:
        // Create an invisible sprite with mode set to update the background color
        spriteId = CreateSprite(&sPokemonLogoShineSpriteTemplate, 0, 68, 0);
        gSprites[spriteId].oam.objMode = ST_OAM_OBJ_WINDOW;
        gSprites[spriteId].sMode = mode;
        gSprites[spriteId].invisible = TRUE;

        // Create two faster shine sprites
        spriteId = CreateSprite(&sPokemonLogoShineSpriteTemplate, 0, 68, 0);
        gSprites[spriteId].callback = SpriteCB_PokemonLogoShine_Fast;
        gSprites[spriteId].oam.objMode = ST_OAM_OBJ_WINDOW;

        spriteId = CreateSprite(&sPokemonLogoShineSpriteTemplate, -80, 68, 0);
        gSprites[spriteId].callback = SpriteCB_PokemonLogoShine_Fast;
        gSprites[spriteId].oam.objMode = ST_OAM_OBJ_WINDOW;
        break;
    }
}

#undef sMode
#undef sBgColor

static void VBlankCB(void)
{
    ScanlineEffect_InitHBlankDmaTransfer();
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
    SetGpuReg(REG_OFFSET_BG1VOFS, gBattle_BG1_Y);
}

void CB2_InitTitleScreen(void)
{
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        *((u16 *)PLTT) = RGB_WHITE;
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        SetGpuReg(REG_OFFSET_BG2CNT, 0);
        SetGpuReg(REG_OFFSET_BG1CNT, 0);
        SetGpuReg(REG_OFFSET_BG0CNT, 0);
        SetGpuReg(REG_OFFSET_BG2HOFS, 0);
        SetGpuReg(REG_OFFSET_BG2VOFS, 0);
        SetGpuReg(REG_OFFSET_BG1HOFS, 0);
        SetGpuReg(REG_OFFSET_BG1VOFS, 0);
        SetGpuReg(REG_OFFSET_BG0HOFS, 0);
        SetGpuReg(REG_OFFSET_BG0VOFS, 0);
        DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
        DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
        DmaFill16(3, 0, (void *)(PLTT + 2), PLTT_SIZE - 2);
        ResetPaletteFade();
        gMain.state = 1;
        break;
    case 1:
        LZ77UnCompVram(gTitleScreenPokemonLogoGfx, (void *)(BG_CHAR_ADDR(0)));
        LZ77UnCompVram(gTitleScreenPokemonLogoTilemap, (void *)(BG_SCREEN_ADDR(9)));
        LoadPalette(gTitleScreenBgPalettes, BG_PLTT_ID(0), 15 * PLTT_SIZE_4BPP);
        // bg0
        LZ77UnCompVram(gBackgroundGfx, (void *)(BG_CHAR_ADDR(2)));
        LZ77UnCompVram(gBackgroundTilemap, (void *)(BG_SCREEN_ADDR(26)));
        LoadPalette(gBackgroundPal, BG_PLTT_ID(14), 15 * PLTT_SIZE_4BPP);
        // bg1
        // LZ77UnCompVram(sTitleScreenCloudsGfx, (void *)(BG_CHAR_ADDR(3)));
        // LZ77UnCompVram(gTitleScreenCloudsTilemap, (void *)(BG_SCREEN_ADDR(27)));
        ScanlineEffect_Stop();
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        gReservedSpritePaletteCount = 9;
        // LoadCompressedSpriteSheet(&sSpriteSheet_EmeraldVersion[0]);
        LoadCompressedSpriteSheet(&sSpriteSheet_PressStart[0]);
        LoadCompressedSpriteSheet(&sPokemonLogoShineSpriteSheet[0]);
        LoadPalette(gTitleScreenEmeraldVersionPal, OBJ_PLTT_ID(0), PLTT_SIZE_4BPP);
        LoadSpritePalette(&sSpritePalette_PressStart[0]);
        gMain.state = 2;
        break;
    case 2:
        {
            u8 taskId = CreateTask(Task_TitleScreenPhase1, 0);
            LoadPalette(gBackgroundPal, BG_PLTT_ID(14), 16 * sizeof(u16));
            CreateStarAnimationTask();

            
            gTasks[taskId].tCounter = 256;
            gTasks[taskId].tSkipToNext = FALSE;
            gTasks[taskId].tPointless = -16;
            gTasks[taskId].tBg2Y = -32;
            gMain.state = 3;
            break;
        }
    case 3:
        BeginNormalPaletteFade(PALETTES_ALL, 1, 16, 0, RGB_WHITEALPHA);
        SetVBlankCallback(VBlankCB);
        gMain.state = 4;
        break;
    case 4:
        PanFadeAndZoomScreen(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, 0x100, 0);
        SetGpuReg(REG_OFFSET_BG2X_L, -69 * 256); // -29 - 40 = -69
        SetGpuReg(REG_OFFSET_BG2X_H, -1);

        // Posición Y (vertical)
        SetGpuReg(REG_OFFSET_BG2Y_L, -32 * 256); // Valor original
        SetGpuReg(REG_OFFSET_BG2Y_H, -1);        // Parte alta del valor de 32 bits


        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WIN1H, 0);
        SetGpuReg(REG_OFFSET_WIN1V, 0);
        SetGpuReg(REG_OFFSET_WININ, WININ_WIN0_BG_ALL | WININ_WIN0_OBJ | WININ_WIN1_BG_ALL | WININ_WIN1_OBJ);
        SetGpuReg(REG_OFFSET_WINOUT, WINOUT_WIN01_BG_ALL | WINOUT_WIN01_OBJ | WINOUT_WINOBJ_ALL);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG2 | BLDCNT_EFFECT_LIGHTEN);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 12);
        SetGpuReg(REG_OFFSET_BG0CNT, BGCNT_PRIORITY(3) | BGCNT_CHARBASE(2) | BGCNT_SCREENBASE(26) | BGCNT_16COLOR | BGCNT_TXT256x256);
        SetGpuReg(REG_OFFSET_BG1CNT, BGCNT_PRIORITY(2) | BGCNT_CHARBASE(3) | BGCNT_SCREENBASE(27) | BGCNT_16COLOR | BGCNT_TXT256x256);
        SetGpuReg(REG_OFFSET_BG2CNT, BGCNT_PRIORITY(1) | BGCNT_CHARBASE(0) | BGCNT_SCREENBASE(9) | BGCNT_256COLOR | BGCNT_AFF256x256);
        EnableInterrupts(INTR_FLAG_VBLANK);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_1
                                    | DISPCNT_OBJ_1D_MAP
                                    | DISPCNT_BG2_ON
                                    | DISPCNT_OBJ_ON
                                    | DISPCNT_WIN0_ON
                                    | DISPCNT_OBJWIN_ON);
        m4aSongNumStart(MUS_TITLE);
        gMain.state = 5;
        break;
    case 5:
        if (!UpdatePaletteFade())
        {
            StartPokemonLogoShine(SHINE_MODE_SINGLE_NO_BG_COLOR);
            ScanlineEffect_InitWave(0, DISPLAY_HEIGHT, 4, 4, 0, SCANLINE_EFFECT_REG_BG1HOFS, TRUE);
            SetMainCallback2(MainCB2);
        }
        break;
    }
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

// Shine the Pokémon logo two more times, and fade in the version banner
static void Task_TitleScreenPhase1(u8 taskId)
{
    // Skip to next phase when A, B, Start, or Select is pressed
    if (JOY_NEW(A_B_START_SELECT) || gTasks[taskId].tSkipToNext)
    {
        gTasks[taskId].tSkipToNext = TRUE;
        gTasks[taskId].tCounter = 0;
    }

    if (gTasks[taskId].tCounter != 0)
    {
        u16 frameNum = gTasks[taskId].tCounter;
        if (frameNum == 176)
            StartPokemonLogoShine(SHINE_MODE_DOUBLE);
        else if (frameNum == 64)
            StartPokemonLogoShine(SHINE_MODE_SINGLE);

        gTasks[taskId].tCounter--;
    }
    else
    {
        u8 spriteId;

        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_1 | DISPCNT_OBJ_1D_MAP | DISPCNT_BG2_ON | DISPCNT_OBJ_ON);
        SetGpuReg(REG_OFFSET_WININ, 0);
        SetGpuReg(REG_OFFSET_WINOUT, 0);
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_OBJ | BLDCNT_EFFECT_BLEND | BLDCNT_TGT2_ALL);
        SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(16, 0));
        SetGpuReg(REG_OFFSET_BLDY, 0);

        gTasks[taskId].tCounter = 144;
        gTasks[taskId].func = Task_TitleScreenPhase2;
    }
}

#undef sParentTaskId
#undef sAlphaBlendIdx

// Create "Press Start" and owner name banners, and slide Pokémon logo up
static void Task_TitleScreenPhase2(u8 taskId)
{
    u32 yPos;

    // Skip to next phase when A, B, Start, or Select is pressed
    if (JOY_NEW(A_B_START_SELECT) || gTasks[taskId].tSkipToNext)
    {
        gTasks[taskId].tSkipToNext = TRUE;
        gTasks[taskId].tCounter = 0;
    }

    if (gTasks[taskId].tCounter != 0)
    {
        gTasks[taskId].tCounter--;
    }
    else
    {
        gTasks[taskId].tSkipToNext = TRUE;
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_EFFECT_BLEND | BLDCNT_TGT2_BG0 | BLDCNT_TGT2_BD);
        SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(6, 15));
        SetGpuReg(REG_OFFSET_BLDY, 0);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_1
                                    | DISPCNT_OBJ_1D_MAP
                                    | DISPCNT_BG0_ON
                                    | DISPCNT_BG1_ON
                                    | DISPCNT_BG2_ON
                                    | DISPCNT_OBJ_ON);
        CreatePressStartBanner(64, 108);
        CreateOwnerNameBanner(208, 148);
        gTasks[taskId].tBg1Y = 0;
        gTasks[taskId].func = Task_TitleScreenPhase3;
    }

    if (!(gTasks[taskId].tCounter & 3) && gTasks[taskId].tPointless != 0)
        gTasks[taskId].tPointless++;
    if (!(gTasks[taskId].tCounter & 1) && gTasks[taskId].tBg2Y != 0)
        gTasks[taskId].tBg2Y++;

    // Slide Pokémon logo up
    yPos = gTasks[taskId].tBg2Y * 256;
    SetGpuReg(REG_OFFSET_BG2Y_L, yPos);
    SetGpuReg(REG_OFFSET_BG2Y_H, yPos / 0x10000);

    gTasks[taskId].data[5] = 15; // Unused
    gTasks[taskId].data[6] = 6;  // Unused
}


// Show Rayquaza silhouette and process main title screen input
static void Task_TitleScreenPhase3(u8 taskId)
{
    if (JOY_NEW(A_BUTTON) || JOY_NEW(START_BUTTON))
    {
        FadeOutBGM(4);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_WHITEALPHA);
        SetMainCallback2(CB2_GoToMainMenu);
    }
    else if (JOY_HELD(CLEAR_SAVE_BUTTON_COMBO) == CLEAR_SAVE_BUTTON_COMBO)
    {
        SetMainCallback2(CB2_GoToClearSaveDataScreen);
    }
    else if (JOY_HELD(RESET_RTC_BUTTON_COMBO) == RESET_RTC_BUTTON_COMBO
      && CanResetRTC() == TRUE)
    {
        FadeOutBGM(4);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        SetMainCallback2(CB2_GoToResetRtcScreen);
    }
    else if (JOY_HELD(BERRY_UPDATE_BUTTON_COMBO) == BERRY_UPDATE_BUTTON_COMBO)
    {
        FadeOutBGM(4);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        SetMainCallback2(CB2_GoToBerryFixScreen);
    }
    else
    {
        SetGpuReg(REG_OFFSET_BG2Y_L, 0);
        SetGpuReg(REG_OFFSET_BG2Y_H, 0);

        if ((gMPlayInfo_BGM.status & 0xFFFF) == 0)
        {
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_WHITEALPHA);
            SetMainCallback2(CB2_GoToCopyrightScreen);
        }
    }
}


static void Task_AnimateStars(u8 taskId)
{
    UpdateStarColors();
}

void CreateStarAnimationTask(void)
{
    InitStarAnimation();
    CreateTask(Task_AnimateStars, 0);
}

static void CB2_GoToMainMenu(void)
{
    if (!UpdatePaletteFade())
        SetMainCallback2(CB2_InitMainMenu);
}

static void CB2_GoToCopyrightScreen(void)
{
    if (!UpdatePaletteFade())
        SetMainCallback2(CB2_InitCopyrightScreenAfterTitleScreen);
}

static void CB2_GoToClearSaveDataScreen(void)
{
    if (!UpdatePaletteFade())
        SetMainCallback2(CB2_InitClearSaveDataScreen);
}

static void CB2_GoToResetRtcScreen(void)
{
    if (!UpdatePaletteFade())
        SetMainCallback2(CB2_InitResetRtcScreen);
}

static void CB2_GoToBerryFixScreen(void)
{
    if (!UpdatePaletteFade())
    {
        m4aMPlayAllStop();
        SetMainCallback2(CB2_InitBerryFixProgram);
    }
}


static void UpdateStarColors(void)
{
    u8 currentColor;
    u8 currentSeqIdx;
    u8 nextSeqIdx;
    u8 fromColorIdx;
    u8 toColorIdx;
    u16 fromColor;
    u16 toColor;
    u16 interpColor;
    u8 i;

    if (!sStarAnim.animationActive)
        return;

    sStarAnim.frameCounter++;
    if (sStarAnim.frameCounter >= 2) // Ajusta este valor para controlar la velocidad
    {
        sStarAnim.frameCounter = 0;
        sStarAnim.transitionProgress++;
        
        if (sStarAnim.transitionProgress >= 32) // Duración de cada transición
        {
            sStarAnim.transitionProgress = 0;
            
            // Avanzar cada secuencia a su siguiente estado
            for (i = 0; i < 3; i++)
            {
                sStarAnim.sequences[i].currentIndex++;
                if (sStarAnim.sequences[i].currentIndex >= sStarAnim.sequences[i].length)
                    sStarAnim.sequences[i].currentIndex = 0;
            }
        }
    }

    // Actualizar cada color (3,4,5) 
    for (i = 0; i < 3; i++)
    {
        currentColor = i + COLOR_IDX_DIM; // Convertir índice a color actual (3,4,5)
        currentSeqIdx = sStarAnim.sequences[i].currentIndex;
        nextSeqIdx = (currentSeqIdx + 1) % sStarAnim.sequences[i].length;
        
        fromColorIdx = sStarAnim.sequences[i].sequence[currentSeqIdx];
        toColorIdx = sStarAnim.sequences[i].sequence[nextSeqIdx];
        
        fromColor = sStarAnim.savedColors[fromColorIdx - COLOR_IDX_BACKGROUND];
        toColor = sStarAnim.savedColors[toColorIdx - COLOR_IDX_BACKGROUND];
        
        // Calcular color interpolado
        interpColor = RGB(
            ((sStarAnim.transitionProgress * (GET_R(toColor) - GET_R(fromColor))) / 32) + GET_R(fromColor),
            ((sStarAnim.transitionProgress * (GET_G(toColor) - GET_G(fromColor))) / 32) + GET_G(fromColor),
            ((sStarAnim.transitionProgress * (GET_B(toColor) - GET_B(fromColor))) / 32) + GET_B(fromColor)
        );
        
        // Aplicar el color interpolado
        LoadPalette(&interpColor, BG_PLTT_ID(14) + currentColor, sizeof(u16));
    }
}

