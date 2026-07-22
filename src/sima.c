#include "global.h"
#include "sima.h"
#include "sima_rooms.h"
#include "main.h"
#include "bg.h"
#include "gpu_regs.h"
#include "palette.h"
#include "malloc.h"
#include "decompress.h"
#include "sprite.h"
#include "task.h"
#include "text.h"
#include "constants/rgb.h"

// Modo SIMA: monta BG0/BG1, carga las 3 celdas de arte que el crawler usa de
// verdad (Tarea 3, graphics/sima/gen.py) y pinta la sala del piso actual con
// SimaRoom_GetTile (src/sima_rooms.c). El jugador (Tarea 4) vive en
// src/sima_actors.c: este archivo solo lo inicializa y lo actualiza cada
// frame desde CB2_SimaMain, sin conocer su representacion interna.

// Una pantalla de GBA son 240x160 px. El arte de SIMA usa celdas de 16x16,
// pero el hardware solo tiene tiles de 8x8 -- cada celda de arte ocupa 2x2
// tiles de hardware. 240/16 = 15 columnas, 160/16 = 10 filas: la sala entera
// cabe en una pantalla sin scroll ni camara. (SIMA_ROOM_W/H viven en
// sima_rooms.h porque la logica de sala los necesita sin depender de bg.h.)

static const u32 sTilesGfx[] = INCBIN_U32("graphics/sima/tiles.4bpp");
static const u16 sTilesPal[] = INCBIN_U16("graphics/sima/grounds.gbapal");
// La paleta es la unica de SIMA (indice 0 transparente + 4 tonos, verificada
// en la Tarea 1 para las 10 hojas); grounds.gbapal vale para tiles.4bpp
// tambien porque graphics/sima/gen.py recorta sin recuantizar.

// tiles.4bpp son 3 celdas de 16x16 en fila (48x16 px = 12 tiles de hardware
// de 8x8), una por SimaTile en este orden: SIMA_TILE_FLOOR, SIMA_TILE_WALL,
// SIMA_TILE_STAIRS (ver TILE_CELLS en graphics/sima/gen.py). PlaceCell ya
// convierte una coordenada de celda de arte (16x16) a su indice de tile de
// hardware (8x8) dentro de una hoja de `sheetTilesWide` tiles -- basta con
// pasarle el indice del SimaTile como artCellX y artCellY=0.
#define TILES_SHEET_TILES_WIDE 6  // 48px / 8px

static const struct BgTemplate sSimaBgTemplates[] = {
    // BG0: la sala. Prioridad 1 (por detras de HUD/texto).
    {.bg = 0,
     .charBaseIndex = 0,
     .mapBaseIndex = 31,
     .screenSize = 0,
     .paletteMode = 0,
     .priority = 1,
     .baseTile = 0},
    // BG1: HUD/texto (Tareas 6 y 9). De momento se monta vacio.
    {.bg = 1,
     .charBaseIndex = 3,
     .mapBaseIndex = 30,
     .screenSize = 0,
     .paletteMode = 0,
     .priority = 0,
     .baseTile = 0},
};

static void VBlankCB_Sima(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_SimaMain(void)
{
    RunTasks();
    SimaActors_UpdatePlayer();
    AnimateSprites();
    BuildOamBuffer();
    // Imprescindible aunque esta tarea todavia no imprima nada: sin esto el
    // texto de las Tareas 9/10 no avanzaria (ver la nota de minigame_pre.c,
    // costo una sesion entera de depuracion la primera vez).
    RunTextPrinters();
    UpdatePaletteFade();
}

static void ResetGpuRegisters(void)
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

// Escribe en el tilemap de `bg` los cuatro tiles de hardware (8x8) que forman
// la celda de arte de 16x16 en (artCellX, artCellY) de una hoja de
// `sheetTilesWide` tiles de ancho, cuyos tiles empiezan en el indice
// `tileBase` dentro del char block de ese BG. El destino es la celda
// (destCol, destRow) de la REJILLA DE 16px de la sala (no de tiles): por eso
// destCol/destRow se multiplican por 2 antes de escribir.
static void PlaceCell(u8 bg, u8 destCol, u8 destRow, u16 sheetTilesWide,
                       u16 tileBase, u8 artCellX, u8 artCellY)
{
    u16 topLeft = tileBase + (artCellY * 2) * sheetTilesWide + (artCellX * 2);
    u16 entries[4];

    entries[0] = topLeft;                      // esquina superior izquierda
    entries[1] = topLeft + 1;                  // esquina superior derecha
    entries[2] = topLeft + sheetTilesWide;      // esquina inferior izquierda
    entries[3] = topLeft + sheetTilesWide + 1;  // esquina inferior derecha

    CopyToBgTilemapBufferRect(bg, entries, destCol * 2, destRow * 2, 2, 2);
}

// Pinta en BG0 la sala real de `floor` leyendo SimaRoom_GetTile
// (src/sima_rooms.c) celda a celda. El movimiento del jugador entre pisos
// (escaleras) es de una tarea posterior: de momento el modo arranca
// mostrando siempre el piso 0.
static void DrawRoom(u8 floor)
{
    s8 x, y;

    for (y = 0; y < SIMA_ROOM_H; y++)
    {
        for (x = 0; x < SIMA_ROOM_W; x++)
        {
            u8 tile = SimaRoom_GetTile(floor, x, y);
            PlaceCell(0, x, y, TILES_SHEET_TILES_WIDE, 0, tile, 0);
        }
    }
}

static void SetupGraphics(void)
{
    InitBgsFromTemplates(0, sSimaBgTemplates, ARRAY_COUNT(sSimaBgTemplates));
    SetBgTilemapBuffer(0, AllocZeroed(BG_SCREEN_SIZE));
    SetBgTilemapBuffer(1, AllocZeroed(BG_SCREEN_SIZE));

    // Graficos sin comprimir (ver graphics/sima/gen.py): copia directa a
    // VRAM, sin LZ77UnCompVram. Solo las 12 celdas de tiles.4bpp (Tarea 3),
    // no las hojas grounds/walls completas -- no caben en el indice de 10
    // bits de una entrada de tilemap (ver graphics/sima/gen.py).
    LoadBgTiles(0, sTilesGfx, sizeof(sTilesGfx), 0);

    // Paleta unica de SIMA (indice 0 transparente + 4 tonos): se carga una
    // sola vez en BG_PLTT_ID(0).
    LoadPalette(sTilesPal, BG_PLTT_ID(0), PLTT_SIZE_4BPP);

    DrawRoom(0);
    CopyBgTilemapBufferToVram(0);
    CopyBgTilemapBufferToVram(1);

    ShowBg(0);
    ShowBg(1);
}

void CB2_InitSima(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        ResetGpuRegisters();
        FreeAllSpritePalettes();
        ResetPaletteFade();
        ResetSpriteData();
        ResetTasks();
        ResetBgsAndClearDma3BusyFlags(0);

        DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
        DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
        DmaFill16(3, 0, (void *)PLTT, PLTT_SIZE);

        gMain.state++;
        break;

    case 1:
        SetupGraphics();
        // Piso 0 fijo: las escaleras todavia no cambian de piso (fuera de
        // alcance de la Tarea 4). SimaActors_InitPlayer vive en
        // src/sima_actors.c y coloca el sprite del jugador en el '@' de la
        // sala (SimaRoom_GetSpawn).
        SimaActors_InitPlayer(0);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_OBJ_1D_MAP);

        // BeginNormalPaletteFade no encola si ya hay un fundido activo (ver
        // nota de minigame_pre.c): gatear siempre con !gPaletteFade.active,
        // aunque en este punto del arranque nunca deberia haber uno en
        // marcha.
        if (!gPaletteFade.active)
            BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);

        SetVBlankCallback(VBlankCB_Sima);
        SetMainCallback2(CB2_SimaMain);
        gMain.state = 0;
        break;
    }
}
