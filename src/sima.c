#include "global.h"
#include "sima.h"
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

// Esqueleto del modo SIMA (Tarea 2 del plan): monta BG0/BG1, carga las hojas
// de arte de la Tarea 1 y pinta una sala de prueba rellena de suelo. Sin
// jugador, sin logica de sala (eso llega en sima_rooms.c, Tarea 3): aqui solo
// se demuestra que el pipeline grafico funciona de punta a punta.

// Una pantalla de GBA son 240x160 px. El arte de SIMA usa celdas de 16x16,
// pero el hardware solo tiene tiles de 8x8 -- cada celda de arte ocupa 2x2
// tiles de hardware. 240/16 = 15 columnas, 160/16 = 10 filas: la sala entera
// cabe en una pantalla sin scroll ni camara.
#define SIMA_ROOM_W 15
#define SIMA_ROOM_H 10

static const u32 sGroundsGfx[] = INCBIN_U32("graphics/sima/grounds.4bpp");
static const u16 sGroundsPal[] = INCBIN_U16("graphics/sima/grounds.gbapal");
static const u32 sWallsGfx[] = INCBIN_U32("graphics/sima/walls.4bpp");
// walls comparte la misma paleta que grounds (verificado en la Tarea 1: un
// unico juego de 4 tonos + transparente para las 10 hojas), asi que no hace
// falta un walls.gbapal aparte.

// Dimensiones en pixeles de las hojas fuente (graphics/sima/gen.py). Se usan
// para calcular, a partir de una celda de arte de 16x16, el indice de sus
// cuatro tiles de hardware de 8x8 dentro del blob que carga LoadBgTiles.
#define GROUNDS_PX_W 208
#define GROUNDS_PX_H 256
#define WALLS_PX_W   224
#define WALLS_PX_H   256

#define GROUNDS_TILES_WIDE (GROUNDS_PX_W / 8)                         // 26
#define GROUNDS_TILE_COUNT ((GROUNDS_PX_W / 8) * (GROUNDS_PX_H / 8))  // 832
#define WALLS_TILES_WIDE   (WALLS_PX_W / 8)                           // 28
#define WALLS_TILE_COUNT   ((WALLS_PX_W / 8) * (WALLS_PX_H / 8))      // 896

// grounds ocupa los tiles [0, GROUNDS_TILE_COUNT) del char block de BG0;
// walls se carga justo a continuacion. OJO para tareas futuras: el indice de
// tile de una entrada de tilemap es de 10 bits (0-1023, ver
// WriteSequenceToBgTilemapBuffer/CopyTileMapEntry en src/bg.c), y
// GROUNDS_TILE_COUNT + WALLS_TILE_COUNT = 1728 > 1023. Las ultimas celdas de
// walls quedan fuera del rango que BG0 puede referenciar con su
// charBaseIndex actual (0). Aqui no es un problema porque la sala de prueba
// solo pinta suelo (indices bajos), pero la Tarea 3 (que sí dibuja muros)
// tendra que resolverlo: por ejemplo repuntando charBaseIndex antes de
// dibujar muros, o empaquetando solo el subconjunto de celdas de walls que
// se usan de verdad en vez del .png completo.
#define WALLS_TILE_BASE GROUNDS_TILE_COUNT

// Celda de suelo liso dentro de grounds.png (coordenadas en celdas de 16x16,
// no en tiles de 8x8): un barrido de la hoja confirma que la celda (10,1) es
// un relleno de un unico indice de color en sus 16x16 px, es decir, suelo
// generico sin bordes ni esquinas -- justo lo que hace falta para una sala
// de prueba homogenea.
#define FLOOR_CELL_X 10
#define FLOOR_CELL_Y 1

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

// Sala de prueba: las 15x10 celdas rellenas del mismo suelo. La Tarea 3
// sustituye esto por SimaRoom_GetTile(floor, x, y).
static void DrawTestRoom(void)
{
    u8 x, y;

    for (y = 0; y < SIMA_ROOM_H; y++)
    {
        for (x = 0; x < SIMA_ROOM_W; x++)
        {
            PlaceCell(0, x, y, GROUNDS_TILES_WIDE, 0, FLOOR_CELL_X, FLOOR_CELL_Y);
        }
    }
}

static void SetupGraphics(void)
{
    InitBgsFromTemplates(0, sSimaBgTemplates, ARRAY_COUNT(sSimaBgTemplates));
    SetBgTilemapBuffer(0, AllocZeroed(BG_SCREEN_SIZE));
    SetBgTilemapBuffer(1, AllocZeroed(BG_SCREEN_SIZE));

    // Graficos sin comprimir (ver graphics/sima/gen.py): copia directa a
    // VRAM, sin LZ77UnCompVram. grounds primero, walls justo despues (ver el
    // comentario de WALLS_TILE_BASE arriba).
    LoadBgTiles(0, sGroundsGfx, sizeof(sGroundsGfx), 0);
    LoadBgTiles(0, sWallsGfx, sizeof(sWallsGfx), WALLS_TILE_BASE);

    // Paleta unica de SIMA (indice 0 transparente + 4 tonos), compartida por
    // grounds y walls: se carga una sola vez en BG_PLTT_ID(0).
    LoadPalette(sGroundsPal, BG_PLTT_ID(0), PLTT_SIZE_4BPP);

    DrawTestRoom();
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
