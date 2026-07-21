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
#include "title_screen.h"
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
    // Idempotente: si el vidrio ya está en marcha, ignorar reentradas (p. ej.
    // doble pulsación de Start durante el efecto). Sin esto se orfana el sprite
    // de grieta y se fuga el tile-range (LoadSpriteSheet NO es idempotente).
    if (FindTaskIdByFunc(Task_PhantomGlass) != TASK_NONE)
        return;

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
        // BG3 (estrellas, la capa más al fondo) NUNCA recibe HOFS != 0: su
        // tilemap solo cubre el área visible en horizontal, y CUALQUIER
        // desplazamiento horizontal (positivo o negativo) envuelve a una
        // columna de tiles sin autoría en el borde opuesto (se ve como un
        // tirón rojo sólido -- verificado barriendo BG0HOFS/BG2HOFS/BG3HOFS
        // en -3..+3 con capturas: solo BG3 lo produce, y en ambos signos).
        // El VOFS vertical de BG3 sí es seguro en todo el rango. BG0/BG2
        // toleran HOFS y VOFS en cualquier signo (probado, sin artefacto).
        SetGpuReg(REG_OFFSET_BG0HOFS, dx);   SetGpuReg(REG_OFFSET_BG0VOFS, dy);
        SetGpuReg(REG_OFFSET_BG2HOFS, dx);   SetGpuReg(REG_OFFSET_BG2VOFS, dy);
        SetGpuReg(REG_OFFSET_BG3HOFS, 0);    SetGpuReg(REG_OFFSET_BG3VOFS, dy);
        if (sGlassTimer >= GLASS_SHAKE_FRAMES)
        {
            SetGpuReg(REG_OFFSET_BG0HOFS, 0); SetGpuReg(REG_OFFSET_BG0VOFS, 0);
            SetGpuReg(REG_OFFSET_BG2HOFS, 0); SetGpuReg(REG_OFFSET_BG2VOFS, 0);
            SetGpuReg(REG_OFFSET_BG3HOFS, 0); SetGpuReg(REG_OFFSET_BG3VOFS, 0);
            sCrackSpriteId = CreateSprite(&sTmpl_Crack, 120, 80, 0);  // centro de 240x160
            sCrackShown = TRUE;
            PlaySE(SE_ICE_CRACK);   // el propio crujido, justo cuando aparecen las grietas
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

// --- Menú Nueva partida / Continuar (overlay por sprites, camino con save) ---
//
// Motivo generado por graphics/phantom_intro/gen.py: dos bloques de texto
// ("NUEVA"/"PARTIDA" y "CONTINUAR") + un cursor '>', apilados VERTICALMENTE en
// menu.png con el ancho completo de un sprite 64x32. Ver el comentario en
// gen.py: eso evita que el barrido de tiles de gbagfx intercale las filas de
// dos sprites de 64 de ancho (lo que pasaría con una hoja lado-a-lado).
static const u32 sMenuGfx[] = INCBIN_U32("graphics/phantom_intro/menu.4bpp");
static const u16 sMenuPal[] = INCBIN_U16("graphics/phantom_intro/menu.gbapal");

#define TAG_MENU 0x5001

// Offsets de tile (unidades de tile 4bpp, no bytes) dentro de la hoja; deben
// coincidir con el layout vertical que arma gen.py.
#define MENU_TILE_LABEL0 0    // "NUEVA" / "PARTIDA", bloque 64x32 (32 tiles)
#define MENU_TILE_LABEL1 32   // "CONTINUAR",         bloque 64x32 (32 tiles)
#define MENU_TILE_CURSOR 64   // '>',                 tile suelto 8x8

// Debajo del logo/silueta, centrado donde normalmente vive PRESS START
// (PRESS_START_Y=114 en title_screen.c) -- ese sprite se oculta mientras el
// menú está abierto.
//
// OJO: CreateSprite posiciona por el CENTRO del sprite, no por la esquina
// superior izquierda (ver CalcCenterToCornerVec en src/sprite.c) -- estas
// constantes son coordenadas de CENTRO.
// El logo PHANTOM (BG0) llega hasta ~y=102 en su serifa más baja (columna
// central, cerca de la "M"); estas Y se corrieron hacia abajo respecto al
// primer intento (108/140) porque a esa altura la línea "NUEVA" pisaba esa
// serifa (ver docs/design/captures/t4_menu.png). Con estos valores el bloque
// "NUEVA"/"PARTIDA" arranca claro del logo y "CONTINUAR" queda en el tercio
// inferior, sin tocar la línea de arriba.
#define MENU_LABEL_X   120   // 120 = mitad de los 240px de pantalla (bloques de 64 centrados)
#define MENU_LABEL0_Y  120   // centro del bloque "NUEVA"/"PARTIDA"
#define MENU_LABEL1_Y  142   // centro del bloque "CONTINUAR"
#define MENU_CURSOR_X  78    // a la izquierda de las etiquetas
#define MENU_CURSOR_Y0 112   // alineado con la línea "NUEVA"
#define MENU_CURSOR_Y1 142   // alineado con "CONTINUAR"

static const struct OamData sOam_MenuLabel = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .priority = 0,
};
static const struct OamData sOam_MenuCursor = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(8x8),
    .size = SPRITE_SIZE(8x8),
    .priority = 0,
};

static const struct SpriteSheet sSheet_Menu = { sMenuGfx, 64 * 72 / 2, TAG_MENU };
static const struct SpritePalette sPal_Menu = { sMenuPal, TAG_MENU };

static const struct SpriteTemplate sTmpl_MenuLabel = {
    .tileTag = TAG_MENU, .paletteTag = TAG_MENU, .oam = &sOam_MenuLabel,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
};
static const struct SpriteTemplate sTmpl_MenuCursor = {
    .tileTag = TAG_MENU, .paletteTag = TAG_MENU, .oam = &sOam_MenuCursor,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
};

static bool8 sMenuOpen;
static u8 sMenuCursor;          // 0 = Nueva, 1 = Continuar
static u8 sMenuSpriteIds[3];    // [0]=label Nueva/Partida [1]=label Continuar [2]=cursor
// Debounce de un frame: silencia el primer tick de Task_PhantomMenu (ver abajo).
static u8 sMenuInputDelay;

static void Task_PhantomMenu(u8 taskId);

static void CloseMenu(void)
{
    u32 i;
    for (i = 0; i < 3; i++)
        if (sMenuSpriteIds[i] < MAX_SPRITES)
            DestroySprite(&gSprites[sMenuSpriteIds[i]]);
    FreeSpriteTilesByTag(TAG_MENU);
    FreeSpritePaletteByTag(TAG_MENU);
    sMenuOpen = FALSE;
}

static void OpenMenu(void)
{
    u8 id;

    LoadSpriteSheet(&sSheet_Menu);
    LoadSpritePalette(&sPal_Menu);

    id = CreateSprite(&sTmpl_MenuLabel, MENU_LABEL_X, MENU_LABEL0_Y, 0);
    if (id != MAX_SPRITES)
        gSprites[id].oam.tileNum += MENU_TILE_LABEL0;
    sMenuSpriteIds[0] = id;

    id = CreateSprite(&sTmpl_MenuLabel, MENU_LABEL_X, MENU_LABEL1_Y, 0);
    if (id != MAX_SPRITES)
        gSprites[id].oam.tileNum += MENU_TILE_LABEL1;
    sMenuSpriteIds[1] = id;

    id = CreateSprite(&sTmpl_MenuCursor, MENU_CURSOR_X, MENU_CURSOR_Y0, 0);
    if (id != MAX_SPRITES)
        gSprites[id].oam.tileNum += MENU_TILE_CURSOR;
    sMenuSpriteIds[2] = id;

    sMenuCursor = 0;
    sMenuOpen = TRUE;
    sMenuInputDelay = 1;
    TitleScreen_SetPressStartVisible(FALSE);
    // Prioridad 5 (> la 4 de Task_TitleScreenMain, como el vidrio): el frame en
    // que A confirma, esta task corre DESPUÉS de Task_TitleScreenMain, cuyo
    // manejo de input ya queda en no-op por PhantomIntro_IsBusy() mientras el
    // menú está abierto. Con prioridad < 4 el orden se invertiría y ambas
    // tasks podrían reaccionar al mismo input el mismo frame.
    CreateTask(Task_PhantomMenu, 5);
}

static void Task_PhantomMenu(u8 taskId)
{
    // La task se crea DURANTE la ejecución de Task_TitleScreenMain (el frame
    // en que se pulsó Start/A) y corre más tarde ESE MISMO frame -- sin este
    // debounce, vería el mismo JOY_NEW que la abrió y confirmaría "Nueva
    // partida" al instante, sin dar chance a elegir.
    if (sMenuInputDelay)
    {
        sMenuInputDelay = 0;
        return;
    }

    if (JOY_NEW(DPAD_UP) || JOY_NEW(DPAD_DOWN))
    {
        sMenuCursor ^= 1;
        PlaySE(SE_SELECT);
        if (sMenuSpriteIds[2] < MAX_SPRITES)
            gSprites[sMenuSpriteIds[2]].y = (sMenuCursor == 0) ? MENU_CURSOR_Y0 : MENU_CURSOR_Y1;
    }
    else if (JOY_NEW(A_BUTTON))
    {
        MainCallback next = (sMenuCursor == 0) ? ENTRADA_INTRO : CB2_ContinueSavedGame;
        CloseMenu();
        DestroyTask(taskId);
        PhantomGlass_Start(next);   // el vidrio SOLO aquí, al confirmar
    }
    else if (JOY_NEW(B_BUTTON))
    {
        CloseMenu();
        DestroyTask(taskId);
        TitleScreen_SetPressStartVisible(TRUE);
    }
}

bool8 PhantomIntro_IsBusy(void)
{
    return sMenuOpen || (FindTaskIdByFunc(Task_PhantomGlass) != TASK_NONE);
}

void PhantomIntro_OnStartPressed(void)
{
    if (sMenuOpen)
        return;   // belt-and-suspenders: Task_TitleScreenMain ya gatea con IsBusy()
    if (gSaveFileStatus == SAVE_STATUS_OK)
        OpenMenu();
    else
        PhantomGlass_Start(ENTRADA_INTRO);
}
