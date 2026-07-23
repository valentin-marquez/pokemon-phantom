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
#include "sima.h"
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

// Narración de apertura. Registro: segunda persona, presente, precisión
// sensorial y cero afecto (el "narrador clínico" de
// docs/design/informe-fear-and-hunger.md §2.1). Sin adjetivos de valoración:
// la voz describe el camarote igual que describiría una atrocidad.
//
// La escena es un CAMAROTE DE BARCO, no un tren: quien cierra la secuencia es
// el capitán anunciando la llegada a la isla, así que el viaje es por mar de
// principio a fin.
//
// La litera vacía planta a Carlos sin nombrarlo -- cuando el shmup muestre el
// récord "CAR", el jugador ata el cabo solo.
// El \p final NO es decorativo y NO debe quitarse: es CHAR_PROMPT_CLEAR, que
// pone al printer a esperar pulsación y, de paso, dibuja la flecha ▼
// parpadeante del motor (TextPrinterInitDownArrowCounters). Esa flecha es la
// señal de "pulsa para continuar" que el jugador ya conoce de cualquier
// diálogo del juego -- por eso esta escena NO necesita ni tutorial ni
// auto-avance. Ojo: mientras el printer espera en el \p, IsTextPrinterActive()
// sigue devolviendo TRUE (ver el gate de Task_HandlePreStorySequence).
static const u8 sText_Cabin[] = _("Huele a óxido y a diésel.\n"
                                  "Enciendes la consola.\p");

static const u8 sText_Screen[] = _("La pantalla se ilumina en\n"
                                   "la oscuridad del camarote.\p");

static const u8 sText_Bunk[] = _("La litera de arriba está\n"
                                 "hecha. Nadie la ha usado.\p");

static const u8 sText_OneMore[] = _("Faltan horas para la isla.\n"
                                    "Te da para una partida más.\p");

static const u8 *const sPreStoryText[] = {
    sText_Cabin,
    sText_Screen,
    sText_Bunk,
    sText_OneMore,
};

// Oscurecimiento de la IMAGEN (paleta 11) al entrar en cada mensaje:
// 16 = negro absoluto, 0 = luz plena. El texto (paleta 15) nunca se toca, así
// que se lee blanco sobre negro total antes del primer escalón.
//
// El escalón cae en el mensaje que NOMBRA la luz ("La pantalla se ilumina"):
// el efecto es la frase, no un adorno alrededor de ella.
//
// Tramos de 5 unidades a propósito: con gPaletteFade.deltaY = 2 eso son 3
// transiciones por escalón (16->14->12->11), o sea los tres duran lo mismo.
// Tramos desiguales darían escalones de duración distinta.
static const u8 sStoryLightLevel[] = { 16, 11, 6, 0 };

#define PRE_IMAGE_PALETTE_MASK  (1u << 11)   // BG_PLTT_ID(11): el dibujo
#define PRE_TEXT_PALETTE_MASK   (1u << 15)   // BG_PLTT_ID(15): la narración
// Cada escalón dura 3 * (PRE_LIGHT_FADE_DELAY + 2) = 24 fr ~ 0,4 s.
// (Ver la aritmética de UpdateNormalPaletteFade en src/palette.c: `delay`
// frames de espera + 1 de mezcla BG + 1 de mezcla OBJ por cada paso de 2.)
#define PRE_LIGHT_FADE_DELAY    6

#define PRE_STORY_TEXT_COUNT (sizeof(sPreStoryText) / sizeof(sPreStoryText[0]))

// ShowStoryLine indexa las DOS tablas con el mismo índice: si divergen, lee
// fuera de sStoryLightLevel. Que reviente en compilación, no en la GBA.
STATIC_ASSERT(ARRAY_COUNT(sStoryLightLevel) == PRE_STORY_TEXT_COUNT,
              sStoryLightLevelMatchesTextCount);

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
    // Imprescindible desde que la narración usa velocidad real: es quien hace
    // avanzar el printer letra a letra y quien atiende la espera del \p (con su
    // flecha ▼). Antes no hacía falta porque con speed 0 AddTextPrinter
    // renderizaba la cadena entera de una y dejaba el printer inactivo.
    RunTextPrinters();
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

// Colores de la narración, en el orden que espera AddTextPrinterParameterized3:
// {fondo, tinta, sombra}. Índices sobre gMessageBox_Pal, que es lo que carga
// GetOverworldTextboxPalettePtr() en la paleta 15 (es FIJA: no depende del marco
// que el jugador elija en OPTIONS, así que estos índices son estables).
//   0 = transparente -> deja ver la imagen de fondo
//   1 = blanco puro
//   2 = gris oscuro (lum 98) -> sombra que despega la tinta clara del dibujo
//
// El diálogo vanilla usa {1, 2, 3}: tinta gris sobre fondo BLANCO, que es
// justo lo que pintaba el recuadro blanco encima del dibujo del chico.
static const u8 sNarrationTextColors[3] = {
    TEXT_COLOR_TRANSPARENT,
    TEXT_COLOR_WHITE,
    TEXT_COLOR_DARK_GRAY,
};

void AddTransparentTextPrinter(bool8 allowSkippingDelayWithButtonPress)
{
    gTextFlags.canABSpeedUpPrint = allowSkippingDelayWithButtonPress;
    // OJO: AddTextPrinterParameterized (sin el 3) NO sirve aquí -- copia los
    // colores por defecto de la fuente (src/text.c:265-267), y FONT_NORMAL trae
    // bgColor = 1 (blanco). Aunque la ventana se rellene con PIXEL_FILL(0), el
    // printer repinta ese índice detrás de CADA glifo, y de ahí salía el
    // recuadro blanco con letras negras sobre la imagen. Hay que pasar los
    // colores explícitos para que el fondo sea realmente transparente.
    // La velocidad NO puede ser 0. Con speed 0 AddTextPrinter no "imprime":
    // vuelca la cadena en un bucle de 0x400 vueltas y marca el printer
    // INACTIVO en el acto (src/text.c:294-311). Como RENDER_STATE_CLEAR (el
    // estado del \p) devuelve siempre RENDER_UPDATE y nunca RENDER_FINISH, ese
    // bucle gira en vacío dibujando la flecha ▼ 1024 veces y sale por agotar
    // el contador: la flecha queda pintada pero CONGELADA, sin esperar a nadie,
    // y IsTextPrinterActive() da FALSE desde el primer frame.
    //
    // GetPlayerTextSpeedDelay() devuelve 8/4/1 según OPTIONS (nunca 0), así que
    // el texto sale letra a letra y el \p espera de verdad -- que es lo que
    // hace que esto se comporte como cualquier diálogo del juego.
    AddTextPrinterParameterized3(WIN_STORY, FONT_NORMAL, 0, 0, sNarrationTextColors,
                                 GetPlayerTextSpeedDelay(), gStringVar4);
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

// Imprime el mensaje `index` y, si le toca escalón, sube la luz de la imagen
// a la vez que se imprime (el fundido de paleta y el text printer son sistemas
// independientes, así que corren en paralelo sin estorbarse).
static void ShowStoryLine(u8 index)
{
    AddTextToWindow(sPreStoryText[index]);

    if (index > 0 && sStoryLightLevel[index] != sStoryLightLevel[index - 1])
    {
        BeginNormalPaletteFade(PRE_IMAGE_PALETTE_MASK, PRE_LIGHT_FADE_DELAY,
                               sStoryLightLevel[index - 1],
                               sStoryLightLevel[index], RGB_BLACK);
    }
}

static void Task_HandlePreStorySequence(u8 taskId)
{
    switch (gTasks[taskId].data[0])
    {
    case 0:
        if (!gPaletteFade.active)
        {
            ShowStoryLine(0);
            gTasks[taskId].data[0]++;
        }
        break;

    case 1:
        // UNA sola pulsación por mensaje. El \p del string ya deja al printer
        // esperando (con su flecha ▼), así que la A del jugador la consume él;
        // aquí solo hay que reaccionar a que haya terminado. NO añadir un
        // JOY_NEW propio: eso era el bug -- hacían falta DOS clics, el primero
        // borraba el texto sin subir la luz y el segundo recién iluminaba.
        //
        // El !gPaletteFade.active tampoco es decorativo: BeginNormalPaletteFade
        // hace `if (gPaletteFade.active) return FALSE;` -- no encola. Sin este
        // gate, encadenar mensajes rápido dispararía el siguiente escalón con
        // el anterior a medias, la llamada se descartaría EN SILENCIO y la
        // imagen se quedaría a media luz el resto de la escena.
        if (!IsTextPrinterActive(WIN_STORY) && !gPaletteFade.active)
        {
            sPreMinigameState->currentTextLine++;
            if (sPreMinigameState->currentTextLine >= PRE_STORY_TEXT_COUNT)
            {
                BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
                gTasks[taskId].data[0] = 2;
            }
            else
            {
                ShowStoryLine(sPreMinigameState->currentTextLine);
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
    // Transición directa a SIMA, el dungeon crawler de la consola de Carlos.
    SetMainCallback2(CB2_InitSima);
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

        // La escena abre a oscuras y la luz sube con la narración. Dos pasos
        // separados porque gPaletteFade es estado GLOBAL ÚNICO: no se pueden
        // animar dos fundidos a la vez.
        //   1) La imagen se clava a negro con BlendPalettes (instantáneo, no
        //      animado, así que no ocupa el gPaletteFade). Sobrevive a los
        //      fundidos posteriores porque estos solo tocan las paletas de su
        //      máscara, y al terminar no restauran nada (ver
        //      IsSoftwarePaletteFadeFinishing).
        //   2) Solo la paleta del texto entra desde negro.
        // Antes esto era BeginNormalPaletteFade(PALETTES_ALL, ...), que metía
        // la imagen entera de golpe y dejaba la escena sin ningún crescendo.
        BlendPalettes(PRE_IMAGE_PALETTE_MASK, 16, RGB_BLACK);
        BeginNormalPaletteFade(PRE_TEXT_PALETTE_MASK, 0, 16, 0, RGB_BLACK);

        // Configurar callbacks
        SetVBlankCallback(VBlankCB_PreMinigame);
        CreateTask(Task_HandlePreStorySequence, 0);
        SetMainCallback2(CB2_PreMinigameMain);

        PlayBGM(MUS_INTRO);
        gMain.state = 0;
        break;
    }
}
