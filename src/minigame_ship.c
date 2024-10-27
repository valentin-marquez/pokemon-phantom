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

#define WIN_STORY 0
#define FONT_WHITE 0
#define BASE_BLOCK 0x200 // Aumentado para evitar corrupción

static const u32 gPreGameBackgroundGfx[] = INCBIN_U32("graphics/minigame_spaceship/boy.4bpp.lz");
static const u32 gPreGameBackgroundTilemap[] = INCBIN_U32("graphics/minigame_spaceship/boy.bin.lz");
static const u16 gPreGameBackgroundPal[] = INCBIN_U16("graphics/minigame_spaceship/boy.gbapal");

typedef struct
{
    u8 currentTextLine;
    bool8 isTextPrinting;
    u16 textSpeed;
    u8 fadeState;
} MinigameState;

static EWRAM_DATA MinigameState *sMinigameState = NULL;

static void Task_HandleStorySequence(u8 taskId);
static void Task_closeMsgBox(u8 taskId);

static const struct BgTemplate sMinigameBgTemplates[] = {
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
     .baseTile = 0}};

static const struct WindowTemplate sStoryTextWindowTemplate[] = {
    [WIN_STORY] = {
        .bg = 1,
        .tilemapLeft = 2, // Moved slightly right for better margin
        .tilemapTop = 13, // Moved up to give more vertical space
        .width = 26,      // Maintained width for readability
        .height = 6,      // Increased height to allow more lines
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

static const u8 *const sStoryText[] = {
    sText_Train,
    sText_Boy,
};

#define STORY_TEXT_COUNT (sizeof(sStoryText) / sizeof(sStoryText[0]))

static void VBlankCB_Minigame(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_MinigameMain(void)
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
    void (*callback)(struct TextPrinterTemplate *, u16) = NULL;
    gTextFlags.canABSpeedUpPrint = allowSkippingDelayWithButtonPress;
    // Cambiado último parámetro a 0 para fondo transparente
    // u16 AddTextPrinterParameterized2(u8 windowId, u8 fontId, const u8 *str, u8 speed, void (*callback)(struct TextPrinterTemplate *, u16), u8 fgColor, u8 bgColor, u8 shadowColor)
    // ok vamos a hacer un efecto transparente con
    AddTextPrinterParameterized2(WIN_STORY, FONT_NORMAL, gStringVar4, GetPlayerTextSpeedDelay(), callback, TEXT_COLOR_WHITE, TEXT_COLOR_TRANSPARENT, TEXT_COLOR_DARK_GRAY);
}

static void SetupGraphicsAndWindows(void)
{
    // Inicializar BGs
    InitBgsFromTemplates(0, sMinigameBgTemplates, ARRAY_COUNT(sMinigameBgTemplates));
    SetBgTilemapBuffer(0, AllocZeroed(BG_SCREEN_SIZE));
    SetBgTilemapBuffer(1, AllocZeroed(BG_SCREEN_SIZE));

    // Cargar gráficos del background
    LZ77UnCompVram(gPreGameBackgroundGfx, (void *)BG_CHAR_ADDR(2));
    LZ77UnCompVram(gPreGameBackgroundTilemap, (void *)BG_SCREEN_ADDR(31));
    LoadPalette(gPreGameBackgroundPal, BG_PLTT_ID(11), PLTT_SIZE_4BPP);

    // Configurar sistema de texto
    InitWindows(sStoryTextWindowTemplate);
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

static void Task_HandleStorySequence(u8 taskId)
{
    switch (gTasks[taskId].data[0])
    {
    case 0:
        if (!gPaletteFade.active)
        {
            AddTextToWindow(sStoryText[0]);
            gTasks[taskId].data[0]++;
        }
        break;

    case 1:
        if (!RunTextPrintersAndIsPrinter0Active() &&
            (JOY_NEW(A_BUTTON) || JOY_NEW(B_BUTTON)))
        {
            sMinigameState->currentTextLine++;
            if (sMinigameState->currentTextLine >= STORY_TEXT_COUNT)
            {
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].data[0] = 2;
            }
            else
            {
                AddTextToWindow(sStoryText[sMinigameState->currentTextLine]);
            }
        }
        break;

    case 2:
        if (!gPaletteFade.active)
        {
            ClearDialogWindowAndFrame(WIN_STORY, TRUE);
            FreeAllWindowBuffers();
            FREE_AND_SET_NULL(sMinigameState);
            DestroyTask(taskId);
        }
        break;
    }
}

static void Task_closeMsgBox(u8 taskId)
{
    if (!RunTextPrintersAndIsPrinter0Active())
    {
        ClearDialogWindowAndFrame(WIN_STORY, TRUE);
        FreeAllWindowBuffers();
    }
}

void CB2_InitMinigameShip(void)
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
        sMinigameState = AllocZeroed(sizeof(MinigameState));
        gMain.state++;
        break;

    case 1:
        SetupGraphicsAndWindows();
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_OBJ_1D_MAP);
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);

        // Configurar callbacks
        SetVBlankCallback(VBlankCB_Minigame);
        CreateTask(Task_HandleStorySequence, 0);
        SetMainCallback2(CB2_MinigameMain);

        PlayBGM(MUS_INTRO);
        gMain.state = 0;
        break;
    }
}