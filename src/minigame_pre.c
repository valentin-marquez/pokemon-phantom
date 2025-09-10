#include "global.h"
#include "random.h"
#include "gba/m4a_internal.h"
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
#include "window.h"
#include "text.h"
#include "menu.h"
#include "text_window.h"
#include "string_util.h"
#include "battle.h"
#include "title_screen.h"
#include "menu_helpers.h"
#include "main_menu.h"
#include "constants/rgb.h"
#include "constants/songs.h"
#include "constants/characters.h"
#include "minigame_spaceship.h"
#include "minigame_pre.h"
#include <string.h>

#define WIN_STORY 0
#define FONT_WHITE 0
#define BASE_BLOCK 0x200

// Gráficos del boy para la escena pre-minijuego
static const u32 gPreGameBackgroundGfx[] = INCBIN_U32("graphics/minigame_spaceship/boy.4bpp.lz");
static const u32 gPreGameBackgroundTilemap[] = INCBIN_U32("graphics/minigame_spaceship/boy.bin.lz");
static const u16 gPreGameBackgroundPal[] = INCBIN_U16("graphics/minigame_spaceship/boy.gbapal");

typedef struct
{
    u8 currentTextLine;
    bool8 isTextPrinting;
    u16 textSpeed;
    u8 fadeState;
} PreMinigameState;

static EWRAM_DATA PreMinigameState *sPreMinigameState = NULL;

static void Task_HandlePreStorySequence(u8 taskId);
static void Task_TransitionToMainGame(u8 taskId);

static const struct BgTemplate sPreMinigameBgTemplates[] = {
    {.bg = 0,
     .charBaseIndex = 2,
     .mapBaseIndex = 31,
     .screenSize = 0,
     .paletteMode = 0,
     .priority = 1,
     .baseTile = 0},
    {.bg = 1,
     .charBaseIndex = 0,
     .mapBaseIndex = 28,
     .screenSize = 0,
     .paletteMode = 0,
     .priority = 0,
     .baseTile = 0}
};

static const struct WindowTemplate sPreStoryTextWindowTemplate[] = {
    [WIN_STORY] = {
        .bg = 1,
        .tilemapLeft = 2,
        .tilemapTop = 13,
        .width = 26,
        .height = 6,
        .paletteNum = 15,
        .baseBlock = BASE_BLOCK,
    },
};

static const u8 sText_Train[] = _("La pantalla de la consola portatil\n"
                                  "se ilumino en la oscuridad del\n"
                                  "compartimento del tren.\p");

static const u8 sText_Boy[] = _("Un joven, con el rostro parcialmente\n"
                                "oculto por la capucha de su chaqueta,\n"
                                "sostenia el dispositivo con firmeza.\p");

static const u8 sText_Preparation[] = _("Era hora de prepararse para\n"
                                        "la mision más importante\n"
                                        "de su vida...\p");

static const u8 *const sPreStoryText[] = {
    sText_Train,
    sText_Boy,
    sText_Preparation,
};

#define PRE_STORY_TEXT_COUNT (sizeof(sPreStoryText) / sizeof(sPreStoryText[0]))

static void VBlankCB_PreMinigame(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_PreMinigameMain(void)
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

void AddTransparentTextPrinter(bool8 allowSkippingDelayWithButtonPress)
{
    gTextFlags.canABSpeedUpPrint = allowSkippingDelayWithButtonPress;
    // La función correcta es: AddTextPrinterParameterized(windowId, fontId, str, x, y, speed, callback)
    AddTextPrinterParameterized(WIN_STORY, FONT_NORMAL, gStringVar4, 0, 0, 0, NULL);
}

static void SetupGraphicsAndWindows(void)
{
    // Inicializar BGs
    InitBgsFromTemplates(0, sPreMinigameBgTemplates, ARRAY_COUNT(sPreMinigameBgTemplates));
    SetBgTilemapBuffer(0, AllocZeroed(BG_SCREEN_SIZE));
    SetBgTilemapBuffer(1, AllocZeroed(BG_SCREEN_SIZE));

    // Cargar gráficos del background
    LZ77UnCompVram(gPreGameBackgroundGfx, (void *)BG_CHAR_ADDR(2));
    LZ77UnCompVram(gPreGameBackgroundTilemap, (void *)BG_SCREEN_ADDR(31));
    LoadPalette(gPreGameBackgroundPal, BG_PLTT_ID(11), PLTT_SIZE_4BPP);

    // Configurar sistema de texto
    InitWindows(sPreStoryTextWindowTemplate);
    DeactivateAllTextPrinters();
    LoadPalette(GetOverworldTextboxPalettePtr(), BG_PLTT_ID(15), PLTT_SIZE_4BPP);

    // Inicializar ventana de texto correctamente
    FillWindowPixelBuffer(WIN_STORY, PIXEL_FILL(0));
    PutWindowTilemap(WIN_STORY);
    CopyWindowToVram(WIN_STORY, COPYWIN_FULL);

    ShowBg(0);
    ShowBg(1);
}

static void AddTextToWindow(const u8 *text)
{
    FillWindowPixelBuffer(WIN_STORY, PIXEL_FILL(0));
    StringExpandPlaceholders(gStringVar4, text);
    AddTransparentTextPrinter(TRUE);
    CopyWindowToVram(WIN_STORY, COPYWIN_FULL);
}

static void Task_HandlePreStorySequence(u8 taskId)
{
    switch (gTasks[taskId].data[0])
    {
    case 0:
        if (!gPaletteFade.active)
        {
            AddTextToWindow(sPreStoryText[0]);
            gTasks[taskId].data[0]++;
        }
        break;

    case 1:
        if (!IsTextPrinterActive(WIN_STORY) &&
            (JOY_NEW(A_BUTTON) || JOY_NEW(B_BUTTON)))
        {
            sPreMinigameState->currentTextLine++;
            if (sPreMinigameState->currentTextLine >= PRE_STORY_TEXT_COUNT)
            {
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].data[0] = 2;
            }
            else
            {
                AddTextToWindow(sPreStoryText[sPreMinigameState->currentTextLine]);
            }
        }
        break;

    case 2:
        if (!gPaletteFade.active)
        {
            RemoveWindow(WIN_STORY);
            FreeAllWindowBuffers();
            FREE_AND_SET_NULL(sPreMinigameState);
            DestroyTask(taskId);
            CreateTask(Task_TransitionToMainGame, 0);
        }
        break;
    }
}

static void Task_TransitionToMainGame(u8 taskId)
{
    // Transición directa al minijuego principal
    SetMainCallback2(CB2_InitMinigameSpaceship);
    DestroyTask(taskId);
}

void CB2_InitPreMinigame(void)
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

        // Inicializar estado
        sPreMinigameState = Alloc(sizeof(PreMinigameState));
        if (sPreMinigameState != NULL)
        {
            memset(sPreMinigameState, 0, sizeof(PreMinigameState));
        }
        gMain.state++;
        break;

    case 1:
        SetupGraphicsAndWindows();
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_OBJ_1D_MAP);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);

        // Configurar callbacks
        SetVBlankCallback(VBlankCB_PreMinigame);
        CreateTask(Task_HandlePreStorySequence, 0);
        SetMainCallback2(CB2_PreMinigameMain);

        PlayBGM(MUS_INTRO);
        gMain.state = 0;
        break;
    }
}
