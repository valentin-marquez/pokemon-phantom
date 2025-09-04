// Include headers in logical groups
#include "global.h"
#include "random.h"
// Core GBA headers
#include "gba/m4a_internal.h"
// Game system headers
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
#include "main_menu.h"
// Constants
#include "constants/rgb.h"
#include "constants/songs.h"

#include "minigame_ship.h"

// Al inicio del archivo, podemos agregar defines para la posición
#define PRESS_START_X 86 // Posición X central
#define PRESS_START_Y 114 // Posición Y

#define NUM_PRESS_START_FRAMES 5

// Background animation defines
#define CLOUD_SCROLL_SPEED 1
#define CLOUD_SCROLL_DELAY 2

// Star animation defines
#define STAR_COLOR_BACKGROUND 2
#define STAR_COLOR_DIM 3
#define STAR_COLOR_MEDIUM 4
#define STAR_COLOR_BRIGHT 5
#define MAX_SEQUENCE_LENGTH 6

// Input button combinations
#define CLEAR_SAVE_BUTTON_COMBO (B_BUTTON | SELECT_BUTTON | DPAD_UP)
#define RESET_RTC_BUTTON_COMBO (B_BUTTON | SELECT_BUTTON | DPAD_LEFT)

// Glitch effect constants
#define GLITCH_UPDATE_FRAMES 2
#define GLITCH_MAX_OFFSET 3
#define GLITCH_SEGMENTS 4
#define GLITCH_DURATION 8
#define GLITCH_INTERVAL 180

// Forward declarations of static functions
static void MainCB2(void);
static void VBlankCB(void);
static void Task_TitleScreenMain(u8 taskId);
static void SetMainTitleScreen(void);
static void ResetGpuRegs(void);
static void ResetBgPositions(void);
static void UpdateCloudPosition(void);
static void CreateStarAnimationTask(void);
static void Task_AnimateStars(u8 taskId);
static void InitStarAnimation(void);
static void InitColorSequences(void);
static void UpdateStarColors(void);
static void LoadGraphicsResources(void);
static void CreatePressStartSprites(void);
static void SpriteCB_PressStart(struct Sprite *sprite);
static void Task_FadeOutToMinigame(u8 taskId);
static void CB2_TaskFadeOutToMinigame(void);

static void Task_WaitForStartPress(u8 taskId);

// Updated graphics resources - using new generated files
static const u32 gTitleScreenCloudsGfx[] = INCBIN_U32("graphics/new_title_screen/clouds.4bpp.lz");
static const u32 gTitleScreenCloudsTilemap[] = INCBIN_U32("graphics/new_title_screen/clouds.bin.lz");
static const u16 gTitleScreenCloudsPal[] = INCBIN_U16("graphics/new_title_screen/clouds.gbapal");

static const u32 gBackgroundGfx[] = INCBIN_U32("graphics/new_title_screen/background.4bpp.lz");
static const u32 gBackgroundTilemap[] = INCBIN_U32("graphics/new_title_screen/background.bin.lz");
static const u16 gBackgroundPal[] = INCBIN_U16("graphics/new_title_screen/background.gbapal");

static const u32 gSiluetaGfx[] = INCBIN_U32("graphics/new_title_screen/silueta.4bpp.lz");
static const u32 gSiluetaTilemap[] = INCBIN_U32("graphics/new_title_screen/silueta.bin.lz");
static const u16 gSiluetaPal[] = INCBIN_U16("graphics/new_title_screen/silueta.gbapal");

static const u32 gTitleScreenPokemonLogoGfx[] = INCBIN_U32("graphics/new_title_screen/logo.4bpp.lz");
static const u32 gTitleScreenPokemonLogoTilemap[] = INCBIN_U32("graphics/new_title_screen/logo.bin.lz");
static const u16 gTitleScreenPokemonLogoPal[] = INCBIN_U16("graphics/new_title_screen/logo.gbapal");

static const u32 gPressStartGfx[] = INCBIN_U32("graphics/new_title_screen/press_start.4bpp.lz");
static const u16 gPressStartPal[] = INCBIN_U16("graphics/new_title_screen/press_start.gbapal");

// Data structures
enum
{
    TAG_PRESS_START,
};

struct ColorSequence
{
    u8 sequence[MAX_SEQUENCE_LENGTH];
    u8 length;
    u8 currentIndex;
};

enum StarColorIndices
{
    COLOR_IDX_BACKGROUND = 2,
    COLOR_IDX_DIM = 3,
    COLOR_IDX_MEDIUM = 4,
    COLOR_IDX_BRIGHT = 5
};

struct StarAnimationState
{
    u16 savedColors[4];
    struct ColorSequence sequences[3];
    u8 frameCounter;
    u8 transitionProgress;
    bool8 animationActive;
};

struct GlitchEffect
{
    u16 timer;
    u16 duration;
    u8 active;
    s16 offsets[GLITCH_SEGMENTS];
    u16 savedSegments[GLITCH_SEGMENTS][32];
};

// Static variables
static s16 sCloudHorizontalOffset;
static u16 sCloudScrollDelay;
static struct StarAnimationState sStarAnim;
static struct GlitchEffect sGlitchEffect;
static u8 sPressStartSpriteId;

// Animation frame data for Press Start sprite
static const union AnimCmd sAnim_PressStart_0[] = {
    ANIMCMD_FRAME(0, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_1[] = {
    ANIMCMD_FRAME(4, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_2[] = {
    ANIMCMD_FRAME(8, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_3[] = {
    ANIMCMD_FRAME(12, 4),
    ANIMCMD_END,
};
static const union AnimCmd sAnim_PressStart_4[] = {
    ANIMCMD_FRAME(16, 4),
    ANIMCMD_END,
};

static const union AnimCmd *const sPressStartAnimTable[] = {
    sAnim_PressStart_0,
    sAnim_PressStart_1,
    sAnim_PressStart_2,
    sAnim_PressStart_3,
    sAnim_PressStart_4,
};

// Updated Background templates for 4 BGs
static const struct BgTemplate sTitleScreenBgTemplates[] = {
    // BG0 - Logo (highest priority, front)
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0, // Highest priority (front)
        .baseTile = 0,
    },
    // BG1 - Clouds (scrolling)
    {
        .bg = 1,
        .charBaseIndex = 1,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0,
    },
    // BG2 - Silueta (static silhouette)
    {
        .bg = 2,
        .charBaseIndex = 2,
        .mapBaseIndex = 29,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 2,
        .baseTile = 0,
    },
    // BG3 - Background (animated stars, lowest priority)
    {
        .bg = 3,
        .charBaseIndex = 3,
        .mapBaseIndex = 28,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 3, // Lowest priority (back)
        .baseTile = 0,
    },
};

// OAM data
static const struct OamData sOamData_PressStart = {
    .y = 0,
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
};

// sprite template data
static const struct SpriteTemplate sPressStartSpriteTemplate = {
    .tileTag = TAG_PRESS_START,
    .paletteTag = TAG_PRESS_START,
    .oam = &sOamData_PressStart,
    .anims = sPressStartAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_PressStart,
};

// Sprite sheet and palette
static const struct CompressedSpriteSheet sSpriteSheet_PressStart[] = {
    {
        .data = gPressStartGfx,
        .size = 0x600,
        .tag = TAG_PRESS_START,
    },
    {},
};

static const struct SpritePalette sSpritePalette_PressStart[] = {
    {
        .data = gPressStartPal,
        .tag = TAG_PRESS_START,
    },
    {},
};

static void ResetGpuRegs(void)
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

static void ResetBgPositions(void)
{
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
    SetGpuReg(REG_OFFSET_BG2HOFS, 0);
    SetGpuReg(REG_OFFSET_BG2VOFS, 0);
    SetGpuReg(REG_OFFSET_BG3HOFS, 0);
    SetGpuReg(REG_OFFSET_BG3VOFS, 0);
}

static void InitGlitchEffect(void)
{
    int i;

    sGlitchEffect.timer = 0;
    sGlitchEffect.duration = 0;
    sGlitchEffect.active = FALSE;
    for (i = 0; i < GLITCH_SEGMENTS; i++)
    {
        sGlitchEffect.offsets[i] = 0;
    }
}

static void TriggerGlitchEffect(void)
{
    int i;
    int j;

    if (!sGlitchEffect.active)
    {
        sGlitchEffect.active = TRUE;
        sGlitchEffect.duration = 0;

        // Save original logo segments (BG0 now)
        for (i = 0; i < GLITCH_SEGMENTS; i++)
        {
            for (j = 0; j < 32; j++)
            {
                sGlitchEffect.savedSegments[i][j] = ((u16 *)BG_SCREEN_ADDR(31))[i * 32 + j];
            }
        }
    }
}

static void UpdateGlitchEffect(void)
{
    int i;
    int j;
    int destPos;

    if (!sGlitchEffect.active)
        return;

    sGlitchEffect.timer++;

    if (sGlitchEffect.timer % GLITCH_UPDATE_FRAMES == 0)
    {
        sGlitchEffect.duration++;

        // Generate random offsets for each segment
        for (i = 0; i < GLITCH_SEGMENTS; i++)
        {
            if (Random() % 3 == 0) // 33% chance to glitch each segment
            {
                sGlitchEffect.offsets[i] = (Random() % (GLITCH_MAX_OFFSET * 2)) - GLITCH_MAX_OFFSET;
            }

            // Apply offset to segment (BG0 now)
            for (j = 0; j < 32; j++)
            {
                destPos = j + sGlitchEffect.offsets[i];
                if (destPos >= 0 && destPos < 32)
                {
                    ((u16 *)BG_SCREEN_ADDR(31))[i * 32 + j] = sGlitchEffect.savedSegments[i][destPos];
                }
            }
        }
    }

    // Reset effect after duration
    if (sGlitchEffect.duration >= GLITCH_DURATION)
    {
        sGlitchEffect.active = FALSE;
        sGlitchEffect.duration = 0;

        // Restore original logo (BG0)
        for (i = 0; i < GLITCH_SEGMENTS; i++)
        {
            for (j = 0; j < 32; j++)
            {
                ((u16 *)BG_SCREEN_ADDR(31))[i * 32 + j] = sGlitchEffect.savedSegments[i][j];
            }
        }
    }
}

static void UpdateCloudPosition(void)
{
    sCloudScrollDelay++;
    if (sCloudScrollDelay >= CLOUD_SCROLL_DELAY)
    {
        sCloudScrollDelay = 0;
        sCloudHorizontalOffset += CLOUD_SCROLL_SPEED;

        // Cuando el offset llegue a 256 (ancho de la pantalla), reiniciamos a 0
        if (sCloudHorizontalOffset >= 256)
            sCloudHorizontalOffset = 0;

        SetGpuReg(REG_OFFSET_BG1HOFS, sCloudHorizontalOffset); // BG1 for clouds
    }
}

// Sprite data
#define sTimer data[0]
#define sVisible data[1]

static void SpriteCB_PressStart(struct Sprite *sprite)
{
    sprite->sTimer++;
    if (sprite->sTimer >= 16)
    {
        sprite->sTimer = 0;
        sprite->invisible = sprite->sVisible;
        sprite->sVisible = !sprite->sVisible;
    }
}

static void CreatePressStartSprites(void)
{
    u8 i;
    s16 x = PRESS_START_X;
    s16 y = PRESS_START_Y;

    LoadCompressedSpriteSheet(&sSpriteSheet_PressStart[0]);
    LoadSpritePalette(&sSpritePalette_PressStart[0]);

    x -= (NUM_PRESS_START_FRAMES * 16);

    for (i = 0; i < NUM_PRESS_START_FRAMES; i++)
    {
        u8 spriteId = CreateSprite(&sPressStartSpriteTemplate, x + (i * 32), y, 0);
        if (spriteId != MAX_SPRITES)
        {
            StartSpriteAnim(&gSprites[spriteId], i);
            gSprites[spriteId].sTimer = 0;
            gSprites[spriteId].sVisible = TRUE;
        }
    }
}

void CB2_InitTitleScreen(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        ResetTasks();
        ResetSpriteData();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetGpuRegs();

        sCloudHorizontalOffset = 0;
        sCloudScrollDelay = 0;

        DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
        DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
        DmaFill16(3, 0, (void *)PLTT, PLTT_SIZE);

        InitGlitchEffect();

        ResetBgsAndClearDma3BusyFlags(FALSE);
        InitBgsFromTemplates(0, sTitleScreenBgTemplates, ARRAY_COUNT(sTitleScreenBgTemplates));
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON);
        gMain.state++;
        break;

    case 1:
        LoadGraphicsResources();
        gMain.state++;
        break;

    case 2:
        CreateStarAnimationTask();
        CreatePressStartSprites();
        gMain.state++;
        break;

    case 3:
        ResetBgPositions();
        CreateTask(Task_TitleScreenMain, 4);
        SetVBlankCallback(VBlankCB);
        SetMainCallback2(MainCB2);
        gMain.state = 0;
        break;
    }
}

static void LoadGraphicsResources(void)
{
    // Load Logo (BG0 - Front)
    LZ77UnCompVram(gTitleScreenPokemonLogoGfx, (void *)(BG_CHAR_ADDR(0)));
    LZ77UnCompVram(gTitleScreenPokemonLogoTilemap, (void *)(BG_SCREEN_ADDR(31)));
    LoadPalette(gTitleScreenPokemonLogoPal, BG_PLTT_ID(0), 16 * sizeof(u16));

    // Load Clouds (BG1 - Scrolling)
    LZ77UnCompVram(gTitleScreenCloudsGfx, (void *)(BG_CHAR_ADDR(1)));
    LZ77UnCompVram(gTitleScreenCloudsTilemap, (void *)(BG_SCREEN_ADDR(30)));
    LoadPalette(gTitleScreenCloudsPal, BG_PLTT_ID(1), 16 * sizeof(u16));

    // Load Silueta (BG2 - Static)
    LZ77UnCompVram(gSiluetaGfx, (void *)(BG_CHAR_ADDR(2)));
    LZ77UnCompVram(gSiluetaTilemap, (void *)(BG_SCREEN_ADDR(29)));
    LoadPalette(gSiluetaPal, BG_PLTT_ID(2), 16 * sizeof(u16));

    // Load Background with stars (BG3 - Animated, Back)
    LZ77UnCompVram(gBackgroundGfx, (void *)(BG_CHAR_ADDR(3)));
    LZ77UnCompVram(gBackgroundTilemap, (void *)(BG_SCREEN_ADDR(28)));
    LoadPalette(gBackgroundPal, BG_PLTT_ID(3), 16 * sizeof(u16));
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void Task_TitleScreenMain(u8 taskId)
{
    UpdateCloudPosition();
    SetMainTitleScreen();

    // Trigger glitch periodically
    if (gMain.vblankCounter1 % GLITCH_INTERVAL == 0)
    {
        TriggerGlitchEffect();
    }

    UpdateGlitchEffect();

    // Keep this task running every frame so clouds/glitch animate continuously.
    // Handle input to start the game here instead of destroying the task.
    if (JOY_NEW(A_BUTTON) || JOY_NEW(START_BUTTON))
    {
        FadeOutBGM(4);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_WHITEALPHA);
        SetMainCallback2(CB2_TaskFadeOutToMinigame);
    }
}

static void Task_WaitForStartPress(u8 taskId)
{
    if (JOY_NEW(A_BUTTON) || JOY_NEW(START_BUTTON))
    {
        FadeOutBGM(4);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_WHITEALPHA);
        SetMainCallback2(CB2_TaskFadeOutToMinigame);
    }
}

static void CB2_TaskFadeOutToMinigame(void)
{
    if (!UpdatePaletteFade())
    {
        SetMainCallback2(CB2_InitMinigameShip);
    }
}

static void SetMainTitleScreen(void)
{
    // Show all 4 BGs
    ShowBg(3); // Background stars (back)
    ShowBg(2); // Silueta 
    ShowBg(1); // Clouds (scrolling)
    ShowBg(0); // Logo (front)

    // Position logo if needed
    ChangeBgX(0, -4 * 256, BG_COORD_SET);
    ChangeBgY(0, -46 * 256, BG_COORD_SET);

    // Set up blending for clouds and logo
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_BG1 | BLDCNT_TGT2_BG0);
    SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(16, 8));
}

static void InitColorSequences(void)
{
    // Secuencia para color 3: 4,5,4,3,2
    sStarAnim.sequences[0].sequence[0] = COLOR_IDX_MEDIUM;
    sStarAnim.sequences[0].sequence[1] = COLOR_IDX_BRIGHT;
    sStarAnim.sequences[0].sequence[2] = COLOR_IDX_MEDIUM;
    sStarAnim.sequences[0].sequence[3] = COLOR_IDX_DIM;
    sStarAnim.sequences[0].sequence[4] = COLOR_IDX_BACKGROUND;
    sStarAnim.sequences[0].length = 5;
    sStarAnim.sequences[0].currentIndex = 0;

    // Secuencia para color 4: 5,4,3,2,3,4
    sStarAnim.sequences[1].sequence[0] = COLOR_IDX_BRIGHT;
    sStarAnim.sequences[1].sequence[1] = COLOR_IDX_MEDIUM;
    sStarAnim.sequences[1].sequence[2] = COLOR_IDX_DIM;
    sStarAnim.sequences[1].sequence[3] = COLOR_IDX_BACKGROUND;
    sStarAnim.sequences[1].sequence[4] = COLOR_IDX_DIM;
    sStarAnim.sequences[1].sequence[5] = COLOR_IDX_MEDIUM;
    sStarAnim.sequences[1].length = 6;
    sStarAnim.sequences[1].currentIndex = 0;

    // Secuencia para color 5: 4,3,2,3,4
    sStarAnim.sequences[2].sequence[0] = COLOR_IDX_MEDIUM;
    sStarAnim.sequences[2].sequence[1] = COLOR_IDX_DIM;
    sStarAnim.sequences[2].sequence[2] = COLOR_IDX_BACKGROUND;
    sStarAnim.sequences[2].sequence[3] = COLOR_IDX_DIM;
    sStarAnim.sequences[2].sequence[4] = COLOR_IDX_MEDIUM;
    sStarAnim.sequences[2].length = 5;
    sStarAnim.sequences[2].currentIndex = 0;
}

static void InitStarAnimation(void)
{
    // Now using BG palette 3 for background stars
    sStarAnim.savedColors[0] = gPlttBufferUnfaded[BG_PLTT_ID(3) + STAR_COLOR_BACKGROUND];
    sStarAnim.savedColors[1] = gPlttBufferUnfaded[BG_PLTT_ID(3) + STAR_COLOR_DIM];
    sStarAnim.savedColors[2] = gPlttBufferUnfaded[BG_PLTT_ID(3) + STAR_COLOR_MEDIUM];
    sStarAnim.savedColors[3] = gPlttBufferUnfaded[BG_PLTT_ID(3) + STAR_COLOR_BRIGHT];

    InitColorSequences();
    sStarAnim.frameCounter = 0;
    sStarAnim.transitionProgress = 0;
    sStarAnim.animationActive = TRUE;
}

static void Task_AnimateStars(u8 taskId)
{
    UpdateStarColors();
}

static void CreateStarAnimationTask(void)
{
    InitStarAnimation();
    CreateTask(Task_AnimateStars, 0);
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
    if (sStarAnim.frameCounter >= 2)
    {
        sStarAnim.frameCounter = 0;
        sStarAnim.transitionProgress++;

        if (sStarAnim.transitionProgress >= 32)
        {
            sStarAnim.transitionProgress = 0;

            for (i = 0; i < 3; i++)
            {
                sStarAnim.sequences[i].currentIndex++;
                if (sStarAnim.sequences[i].currentIndex >= sStarAnim.sequences[i].length)
                    sStarAnim.sequences[i].currentIndex = 0;
            }
        }
    }

    // Update each color (3,4,5) - now using BG palette 3
    for (i = 0; i < 3; i++)
    {
        currentColor = i + COLOR_IDX_DIM;
        currentSeqIdx = sStarAnim.sequences[i].currentIndex;
        nextSeqIdx = (currentSeqIdx + 1) % sStarAnim.sequences[i].length;

        fromColorIdx = sStarAnim.sequences[i].sequence[currentSeqIdx];
        toColorIdx = sStarAnim.sequences[i].sequence[nextSeqIdx];

        fromColor = sStarAnim.savedColors[fromColorIdx - COLOR_IDX_BACKGROUND];
        toColor = sStarAnim.savedColors[toColorIdx - COLOR_IDX_BACKGROUND];

        interpColor = RGB(
            ((sStarAnim.transitionProgress * (GET_R(toColor) - GET_R(fromColor))) / 32) + GET_R(fromColor),
            ((sStarAnim.transitionProgress * (GET_G(toColor) - GET_G(fromColor))) / 32) + GET_G(fromColor),
            ((sStarAnim.transitionProgress * (GET_B(toColor) - GET_B(fromColor))) / 32) + GET_B(fromColor));

        // Apply to BG palette 3 now
        LoadPalette(&interpColor, BG_PLTT_ID(3) + currentColor, sizeof(u16));
    }
}