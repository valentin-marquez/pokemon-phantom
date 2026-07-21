#include "global.h"
#include "phantom_intro.h"
#include "main.h"
#include "sound.h"
#include "palette.h"
#include "bg.h"
#include "save.h"
#include "main_menu.h"
#include "overworld.h"
#include "minigame_ship.h"
#include "task.h"
#include "gpu_regs.h"
#include "sprite.h"
#include "decompress.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// Punto de entrada de la secuencia de intro (Pieza 2 lo sustituirá).
#define ENTRADA_INTRO CB2_InitMinigameShip

static MainCallback sGlassNextCB;   // a dónde ir al terminar
static u8 sGlassPhase;              // 0=impacto,1=sacudida,2=aguanta,3=fundido
static u8 sGlassTimer;

static void Task_PhantomGlass(u8 taskId);

// Grietas de impacto (motivo generado por graphics/phantom_intro/gen.py),
// mostradas como sprite durante la fase de aguante (case 2).
static const u32 sCrackGfx[] = INCBIN_U32("graphics/phantom_intro/crack.4bpp");
static const u16 sCrackPal[] = INCBIN_U16("graphics/phantom_intro/crack.gbapal");

#define TAG_CRACK 0x5000

static const struct OamData sOam_Crack = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .priority = 0,
};
static const struct SpriteSheet sSheet_Crack = { sCrackGfx, 64 * 64 / 2, TAG_CRACK };
static const struct SpritePalette sPal_Crack = { sCrackPal, TAG_CRACK };
static const struct SpriteTemplate sTmpl_Crack = {
    .tileTag = TAG_CRACK, .paletteTag = TAG_CRACK, .oam = &sOam_Crack,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
};

static bool8 sCrackShown;
static u8 sCrackSpriteId;

// Reproduce el vidrio impactado y, al terminar el fundido, salta a nextCB.
static void PhantomGlass_Start(MainCallback nextCB)
{
    sGlassNextCB = nextCB;
    sGlassPhase = 0;
    sGlassTimer = 0;
    FadeOutBGM(4);
    PlaySE(SE_ICE_BREAK);
    // Flash blanco: mezcla la pantalla hacia el blanco vía BLDY sobre todas las capas.
    SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_ALL | BLDCNT_EFFECT_LIGHTEN);
    SetGpuReg(REG_OFFSET_BLDY, 16);   // máximo blanco en el impacto
    LoadSpriteSheet(&sSheet_Crack);
    LoadSpritePalette(&sPal_Crack);
    sCrackShown = FALSE;
    if (FindTaskIdByFunc(Task_PhantomGlass) == TASK_NONE)
        // Prioridad > la de Task_TitleScreenMain (4): así corremos DESPUÉS de su
        // SetMainTitleScreen() cada frame y nuestro BLDCNT/BLDY no queda pisado.
        CreateTask(Task_PhantomGlass, 5);
}

#define GLASS_SHAKE_FRAMES 14
#define GLASS_HOLD_FRAMES  20
#define GLASS_SHAKE_MAX    3   // px

static void Task_PhantomGlass(u8 taskId)
{
    sGlassTimer++;
    switch (sGlassPhase)
    {
    case 0: // impacto: bajar el flash en ~4 frames
        if (sGlassTimer >= 4)
        {
            SetGpuReg(REG_OFFSET_BLDY, 0);
            sGlassPhase = 1;
            sGlassTimer = 0;
        }
        else
        {
            // Re-afirmar BLDCNT cada tick: Task_TitleScreenMain (prioridad 4) corre
            // antes que nosotros y su SetMainTitleScreen() lo pisa a "sin efecto"
            // cada frame; si no lo reescribimos aquí, el fundido de BLDY queda inerte.
            SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT1_ALL | BLDCNT_EFFECT_LIGHTEN);
            SetGpuReg(REG_OFFSET_BLDY, 16 - (sGlassTimer * 4));
        }
        break;
    case 1: // sacudida: jitter decreciente de las 4 BG
    {
        s32 amp = GLASS_SHAKE_MAX - (sGlassTimer * GLASS_SHAKE_MAX / GLASS_SHAKE_FRAMES);
        s32 dx = (sGlassTimer & 1) ? amp : -amp;
        s32 dy = (sGlassTimer & 2) ? amp : -amp;
        SetGpuReg(REG_OFFSET_BG0HOFS, dx);   SetGpuReg(REG_OFFSET_BG0VOFS, dy);
        SetGpuReg(REG_OFFSET_BG2HOFS, dx);   SetGpuReg(REG_OFFSET_BG2VOFS, dy);
        SetGpuReg(REG_OFFSET_BG3HOFS, dx);   SetGpuReg(REG_OFFSET_BG3VOFS, dy);
        if (sGlassTimer >= GLASS_SHAKE_FRAMES)
        {
            SetGpuReg(REG_OFFSET_BG0HOFS, 0); SetGpuReg(REG_OFFSET_BG0VOFS, 0);
            SetGpuReg(REG_OFFSET_BG2HOFS, 0); SetGpuReg(REG_OFFSET_BG2VOFS, 0);
            SetGpuReg(REG_OFFSET_BG3HOFS, 0); SetGpuReg(REG_OFFSET_BG3VOFS, 0);
            sCrackSpriteId = CreateSprite(&sTmpl_Crack, 120, 80, 0);  // centro de 240x160
            sCrackShown = TRUE;
            sGlassPhase = 2;
            sGlassTimer = 0;
        }
        break;
    }
    case 2: // aguanta: grietas visibles en el centro del vidrio
        if (sGlassTimer >= GLASS_HOLD_FRAMES)
        {
            BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
            sGlassPhase = 3;
            sGlassTimer = 0;
        }
        break;
    case 3: // fundido a negro
        if (!gPaletteFade.active)
        {
            if (sCrackShown && sCrackSpriteId < MAX_SPRITES)
                DestroySprite(&gSprites[sCrackSpriteId]);
            FreeSpriteTilesByTag(TAG_CRACK);
            FreeSpritePaletteByTag(TAG_CRACK);
            DestroyTask(taskId);
            SetGpuReg(REG_OFFSET_BLDCNT, 0);
            SetMainCallback2(sGlassNextCB);
        }
        break;
    }
}

void PhantomIntro_OnStartPressed(void)
{
    if (gSaveFileStatus == SAVE_STATUS_OK)
        PhantomGlass_Start(CB2_ContinueSavedGame);  // menú se añade en Task 4
    else
        PhantomGlass_Start(ENTRADA_INTRO);
}
