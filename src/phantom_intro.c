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
static const struct SpriteSheet sSheet_Crack = { sCrackGfx, 64 * (64 * 12) / 2, TAG_CRACK };
static const struct SpritePalette sPal_Crack = { sCrackPal, TAG_CRACK };
static const struct SpriteTemplate sTmpl_Crack = {
    .tileTag = TAG_CRACK, .paletteTag = TAG_CRACK, .oam = &sOam_Crack,
    .anims = gDummySpriteAnimTable, .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
};

// UNA sola grieta de impacto (telaraña conectada desde el centro real de
// pantalla, 120,80), rebanada en una rejilla de 4 columnas x 3 filas de
// sprites 64x64 para cubrir los 240x160 completos (un GBA sprite no puede
// pasar de 64x64). graphics/phantom_intro/gen.py pinta el motivo completo en
// un lienzo full-screen y lo corta en estas mismas 12 celdas -- las
// coordenadas de centro de abajo DEBEN coincidir con GRID_COLS/GRID_ROWS de
// gen.py. Todas las celdas comparten la misma hoja/paleta ya cargada
// (TAG_CRACK): son CreateSprite adicionales sobre tiles ya en VRAM, cada uno
// leyendo su propio bloque de 64 tiles contiguos vía oam.tileNum += K*64
// (mismo patrón ya usado para los sprites del menú, ver MENU_TILE_* abajo).
#define NUM_CRACKS 12
#define CRACK_TILES_PER_CELL 64   // 64x64 px = 8x8 tiles = 64 tiles 4bpp

static const s16 sCrackCellPos[NUM_CRACKS][2] = {
    //   x,   y  (centro de sprite; ver CreateSprite/CalcCenterToCornerVec)
    {  32,  32 }, {  96,  32 }, { 160,  32 }, { 208,  32 },   // fila 0
    {  32,  96 }, {  96,  96 }, { 160,  96 }, { 208,  96 },   // fila 1
    {  32, 144 }, {  96, 144 }, { 160, 144 }, { 208, 144 },   // fila 2
};

static bool8 sCrackShown;
static u8 sCrackSpriteIds[NUM_CRACKS];

// Reproduce el vidrio impactado y, al terminar el fundido, salta a nextCB.
static void PhantomGlass_Start(MainCallback nextCB)
{
    // Idempotente: si el vidrio ya está en marcha, ignorar reentradas (p. ej.
    // doble pulsación de Start durante el efecto). Sin esto se orfana el sprite
    // de grieta y se fuga el tile-range (LoadSpriteSheet NO es idempotente).
    if (FindTaskIdByFunc(Task_PhantomGlass) != TASK_NONE)
        return;

    // Consistencia con el camino del menú (que ya lo oculta al abrir): en el
    // camino sin save este es el único lugar que lo apaga. Llamada
    // idempotente si ya estaba oculto (TitleScreen_SetPressStartVisible solo
    // reescribe .invisible en los sprites existentes).
    TitleScreen_SetPressStartVisible(FALSE);
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
            {
                u32 i;
                for (i = 0; i < NUM_CRACKS; i++)
                {
                    u8 id = CreateSprite(&sTmpl_Crack,
                        sCrackCellPos[i][0], sCrackCellPos[i][1], 0);
                    if (id != MAX_SPRITES)
                        gSprites[id].oam.tileNum += i * CRACK_TILES_PER_CELL;
                    sCrackSpriteIds[i] = id;
                }
            }
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
            if (sCrackShown)
            {
                u32 i;
                for (i = 0; i < NUM_CRACKS; i++)
                    if (sCrackSpriteIds[i] < MAX_SPRITES)
                        DestroySprite(&gSprites[sCrackSpriteIds[i]]);
            }
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
// Motivo generado por graphics/phantom_intro/gen.py: TRES bloques de texto
// ("NUEVA", "PARTIDA", "CONTINUAR") + un cursor '>', apilados VERTICALMENTE en
// menu.png con el ancho completo de un sprite 64x32. Ver el comentario en
// gen.py: eso evita que el barrido de tiles de gbagfx intercale las filas de
// dos sprites de 64 de ancho (lo que pasaría con una hoja lado-a-lado).
//
// "NUEVA PARTIDA" (~78px a 6px/char) no entra en un sprite de 64px de ancho
// (el máximo de OAM en GBA), así que esa línea usa DOS sprites: el bloque
// "NUEVA" y el bloque "PARTIDA", posicionados pegados en X (ver
// MENU_LABEL0_X/MENU_LABEL1_X) para leerse como una sola línea continua.
static const u32 sMenuGfx[] = INCBIN_U32("graphics/phantom_intro/menu.4bpp");
static const u16 sMenuPal[] = INCBIN_U16("graphics/phantom_intro/menu.gbapal");

#define TAG_MENU 0x5001

// Offsets de tile (unidades de tile 4bpp, no bytes) dentro de la hoja; deben
// coincidir con el layout vertical que arma gen.py.
#define MENU_TILE_NUEVA     0    // "NUEVA",     bloque 64x32 (32 tiles)
#define MENU_TILE_PARTIDA  32    // "PARTIDA",   bloque 64x32 (32 tiles)
#define MENU_TILE_CONTINUAR 64   // "CONTINUAR", bloque 64x32 (32 tiles)
#define MENU_TILE_CURSOR   96    // '>',         tile suelto 8x8

// Pegado inmediatamente debajo del logo (pedido del review de esta pasada:
// antes quedaba centrado en la mitad inferior de pantalla, lejos del logo).
//
// OJO: CreateSprite posiciona por el CENTRO del sprite, no por la esquina
// superior izquierda (ver CalcCenterToCornerVec en src/sprite.c) -- estas
// constantes son coordenadas de CENTRO.
//
// El logo PHANTOM (BG0) llega hasta y=102 en su serifa más baja, en la
// columna x≈120 (la "M", justo donde cae el bloque "PARTIDA" de abajo) --
// medido por análisis de píxeles sobre docs/design/captures/final/01_title.png
// (colores de la paleta del logo, no el cielo/nubes). Con TEXT_Y=2 dentro de
// cada bloque de 32px, el texto arranca en pantalla a MENU_ROW0_Y-14: con
// MENU_ROW0_Y=121 eso es y=107, 5px bajo la serifa más profunda del logo
// (verificado por captura real: un primer intento con MENU_ROW0_Y=118 daba
// solo 1px de margen -- demasiado justo -- ver docs/design/captures/final/02_menu.png).
// "CONTINUAR" va en la fila siguiente, separada por 8px de la línea de
// arriba, y termina muy por encima del final de pantalla (margen amplio abajo
// a propósito: el pedido es "pegado al logo", no "llenar la pantalla").
#define MENU_LABEL0_X   108   // centro del bloque "NUEVA" (fila 0) y "CONTINUAR" (fila 1) -- misma X: alineados a la izquierda
#define MENU_LABEL1_X   143   // centro del bloque "PARTIDA": pegado a la derecha de "NUEVA" en la misma línea
#define MENU_ROW0_Y     121   // centro de fila 0: "NUEVA PARTIDA", justo bajo el logo
#define MENU_ROW1_Y     136   // centro de fila 1: "CONTINUAR"
#define MENU_CURSOR_X    64   // a la izquierda de las etiquetas
#define MENU_CURSOR_Y0  110   // alineado con la línea "NUEVA PARTIDA"
#define MENU_CURSOR_Y1  125   // alineado con "CONTINUAR"

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

static const struct SpriteSheet sSheet_Menu = { sMenuGfx, 64 * 104 / 2, TAG_MENU };
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
static u8 sMenuSpriteIds[4];    // [0]=label Nueva [1]=label Partida [2]=label Continuar [3]=cursor
// Debounce de un frame: silencia el primer tick de Task_PhantomMenu (ver abajo).
static u8 sMenuInputDelay;

static void Task_PhantomMenu(u8 taskId);

static void CloseMenu(void)
{
    u32 i;
    for (i = 0; i < 4; i++)
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

    id = CreateSprite(&sTmpl_MenuLabel, MENU_LABEL0_X, MENU_ROW0_Y, 0);
    if (id != MAX_SPRITES)
        gSprites[id].oam.tileNum += MENU_TILE_NUEVA;
    sMenuSpriteIds[0] = id;

    id = CreateSprite(&sTmpl_MenuLabel, MENU_LABEL1_X, MENU_ROW0_Y, 0);
    if (id != MAX_SPRITES)
        gSprites[id].oam.tileNum += MENU_TILE_PARTIDA;
    sMenuSpriteIds[1] = id;

    id = CreateSprite(&sTmpl_MenuLabel, MENU_LABEL0_X, MENU_ROW1_Y, 0);
    if (id != MAX_SPRITES)
        gSprites[id].oam.tileNum += MENU_TILE_CONTINUAR;
    sMenuSpriteIds[2] = id;

    id = CreateSprite(&sTmpl_MenuCursor, MENU_CURSOR_X, MENU_CURSOR_Y0, 0);
    if (id != MAX_SPRITES)
        gSprites[id].oam.tileNum += MENU_TILE_CURSOR;
    sMenuSpriteIds[3] = id;

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
        if (sMenuSpriteIds[3] < MAX_SPRITES)
            gSprites[sMenuSpriteIds[3]].y = (sMenuCursor == 0) ? MENU_CURSOR_Y0 : MENU_CURSOR_Y1;
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
