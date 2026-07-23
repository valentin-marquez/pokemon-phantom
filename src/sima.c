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

// Modo SIMA: monta BG0/BG1, carga el atlas de celdas de arte que el crawler
// usa de verdad (graphics/sima/rooms.py) y pinta la sala del piso actual con
// SimaRoom_GetTileGfx (src/sima_rooms.c). El jugador (Tarea 4) vive en
// src/sima_actors.c: este archivo solo lo inicializa y lo actualiza cada
// frame desde CB2_SimaMain, sin conocer su representacion interna. Pisar una
// escalera (Tarea 5) funde a negro, repinta BG0 con SimaRoom_NextFloor y
// recoloca al jugador en el spawn del piso nuevo antes de fundir de vuelta.

// Una pantalla de GBA son 240x160 px. El arte de SIMA usa celdas de 16x16,
// pero el hardware solo tiene tiles de 8x8 -- cada celda de arte ocupa 2x2
// tiles de hardware. 240/16 = 15 columnas, 160/16 = 10 filas: la sala entera
// cabe en una pantalla sin scroll ni camara. (SIMA_ROOM_W/H viven en
// sima_rooms.h porque la logica de sala los necesita sin depender de bg.h.)

static const u32 sTilesGfx[] = INCBIN_U32("graphics/sima/tiles.4bpp");
static const u16 sTilesPal[] = INCBIN_U16("graphics/sima/grounds.gbapal");
// HUD de corazones (Tarea 6): 2 celdas de 16x16 (corazón lleno/vacío)
// recortadas de hud.png por graphics/sima/gen.py (generate_hud_hearts), no
// la hoja de 8x9 completa -- esa serían 288 tiles de hardware (9 KB) contra
// los 8 que hacen falta (ver el reparto de VRAM junto a sSimaBgTemplates,
// abajo).
static const u32 sHudGfx[] = INCBIN_U32("graphics/sima/hud_hearts.4bpp");
#define HUD_TILE_COUNT ((int)(sizeof(sHudGfx) / TILE_SIZE_4BPP))  // 8

// Tile 100% transparente (las 32 bytes a 0, o sea las 8x8 posiciones en
// blanco) para el char block de BG1. Hace falta uno propio porque
// hud_hearts.4bpp NO reserva una celda vacía: su tile 0 es el cuarto
// superior izquierdo del corazón LLENO, con píxeles opacos de verdad (ver
// el bug real documentado junto a FillBgTilemapBufferRect en SetupGraphics,
// más abajo -- ese tile 0 fue precisamente lo que corrompía la pantalla
// entera). Se carga justo despues de sHudGfx, en el indice HUD_TILE_COUNT.
static const u32 sBlankTile[TILE_SIZE_4BPP / sizeof(u32)] = {0};

// La paleta es la unica de SIMA (indice 0 transparente + 4 tonos, verificada
// en la Tarea 1 para las 10 hojas); grounds.gbapal vale para tiles.4bpp
// tambien porque graphics/sima/gen.py recorta sin recuantizar.

// tiles.4bpp es el atlas de celdas COMPUESTAS que genera
// graphics/sima/rooms.py: una fila de N celdas de 16x16 (N =
// SimaRoom_GetSheetTilesWide()/2), una por cada combinacion distinta de
// (fondo, objeto) que aparece de verdad en alguna sala (ver el comentario
// de compose() en rooms.py sobre por que hace falta componer de antemano:
// un BG de la GBA no apila dos capas). PlaceCell ya convierte una
// coordenada de celda de arte (16x16) a su indice de tile de hardware
// (8x8) dentro de una hoja de `sheetTilesWide` tiles -- basta con pasarle
// el indice de SimaRoom_GetTileGfx como artCellX y artCellY=0.

// REPARTO DE VRAM DE BG (Tarea de arreglo de VRAM, sobre la corrupción de
// pantalla completa que metió la Tarea 6). Los 64 KB de VRAM de fondos
// (0x0000-0xFFFF dentro de BG_VRAM) se dividen en bloques de tiles de 16 KB
// (BG_CHAR_SIZE, indice*0x4000) y bloques de mapa de 2 KB (BG_SCREEN_SIZE,
// indice*0x800), y AMBOS tipos de bloque viven en la MISMA VRAM de 64 KB --
// hay que elegir los indices para que ningun char block se coma un map
// block que este usando otro BG (o el mismo).
//
//   bloque de tiles 0 (BG0, sala)     : 0x0000-0x3FFF (152/512 tiles usados)
//   bloque de tiles 1 (BG1, HUD)      : 0x4000-0x7FFF (9/512 tiles usados)
//   0x8000-0xEFFF                     : LIBRE (bloques de tiles 2-29, 28 KB)
//   bloque de mapa 30 (BG0 tilemap)   : 0xF000-0xF7FF
//   bloque de mapa 31 (BG1 tilemap)   : 0xF800-0xFFFF
//
// Con los tiles apretados contra el principio (bloques 0-1) y los mapas
// contra el final (bloques 30-31) sobra un colchon de 28 KB en medio antes
// de que un char block pise un map block -- la proxima capa que se añada
// (Tarea 9, el marcador) tiene bloques de tiles 2-29 libres de sobra.
//
// OJO: la corrupcion real que motivo este repaso NO fue un solape de VRAM.
// La revision de la Tarea 5 habia avisado (con razon, en abstracto) de que
// el char block 3 (0xC000-0xFFFF), usado por BG1 antes de este cambio,
// comparte cola de VRAM con los map blocks 30/31 (0xF000-0xFFFF) -- pero los
// 8 tiles de hud_hearts.4bpp se quedaban a 0x3000 bytes (384 tiles) de
// distancia de pisarlos, asi que ESE solape nunca llego a pasar (verificado
// leyendo la VRAM emulada: mapblocks y tiles de BG0 identicos con y sin
// HUD). El bug real: SetBgTilemapBuffer deja el tilemap de BG1 a CERO, y la
// casilla 0 por defecto apuntaba al tile 0 de hud_hearts.4bpp -- que no es
// una celda en blanco, es el cuarto superior izquierdo del corazon LLENO,
// con pixeles opacos. Como BG1 va con priority 0 (delante de BG0 y de los
// sprites) y cubre las 30x20 celdas de la pantalla, ese fragmento de
// corazon se pintaba encima de TODO menos de las 3 celdas que DrawHud si
// toca -- de ahi el patron de marcas repetido. El arreglo (sBlankTile +
// FillBgTilemapBufferRect en SetupGraphics, mas abajo) es independiente de
// este reparto de bloques; el reparto se deja mas holgado igual, para que
// quede obvio de un vistazo que no hay solape real.
static const struct BgTemplate sSimaBgTemplates[] = {
    // BG0: la sala. Prioridad 1 (por detras de HUD/texto).
    {.bg = 0,
     .charBaseIndex = 0,
     .mapBaseIndex = 30,
     .screenSize = 0,
     .paletteMode = 0,
     .priority = 1,
     .baseTile = 0},
    // BG1: HUD/texto. Tarea 6 lo usa para los corazones de vida (ver
    // DrawHud, más abajo); Tarea 9 añadirá el marcador encima.
    {.bg = 1,
     .charBaseIndex = 1,
     .mapBaseIndex = 31,
     .screenSize = 0,
     .paletteMode = 0,
     .priority = 0,
     .baseTile = 0},
};

// Piso que BG0 tiene pintado ahora mismo (Tarea 5). Estatico en .bss, sin
// inicializador explicito: arranca en 0, que es exactamente el piso con el
// que CB2_InitSima monta el modo (ver la nota de sima_actors.c sobre por que
// nunca se inicializan estos estaticos inline con un valor no-cero).
static u8 sCurrentFloor;

// Maquina de estados del cambio de piso al pisar una escalera: NONE mientras
// se juega normal, FADE_OUT mientras la pantalla se funde a negro (fase en la
// que SimaActors_UpdatePlayer se deja de llamar para que el jugador no siga
// caminando a oscuras), FADE_IN mientras el piso ya cambiado vuelve a
// aparecer. SIMA_TRANS_NONE vale 0, igual que sCurrentFloor arranca en 0 sin
// inicializador explicito.
enum SimaTransitionState
{
    SIMA_TRANS_NONE,
    SIMA_TRANS_FADE_OUT,
    SIMA_TRANS_FADE_IN,
};

static u8 sTransitionState;

// Tarea 6: si la escalera del piso actual YA está pintada como tal en BG0
// (frente a tapada como suelo llano mientras queden enemigos vivos).
// DrawRoom la fija cada vez que pinta la sala; UpdateStairsVisibility la usa
// para detectar el momento exacto en que hace falta repintar.
static bool8 sStairsVisible;

// Último HP del jugador ya pintado en BG1 (Tarea 6, ver DrawHud). 0xFF fuerza
// el primer pintado (SIMA_PLAYER_MAX_HP nunca llega a 0xFF).
static u8 sHudDrawnHP;

// Forward: UpdateFloorTransition (mas abajo) repinta la sala al cambiar de
// piso; DrawRoom, UpdateStairsVisibility y DrawHud se definen despues de
// PlaceCell, mas adelante en este mismo archivo, pero CB2_SimaMain (que las
// llama) va antes.
static void DrawRoom(u8 floor);
static void UpdateStairsVisibility(void);
static void DrawHud(void);

static void VBlankCB_Sima(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

// Si el jugador esta sobre una escalera y no hay ya un fundido en marcha,
// arranca el cambio de piso fundiendo a negro. BeginNormalPaletteFade no
// encola si gPaletteFade.active ya esta activo (ver la nota de
// minigame_pre.c) -- de ahi el gate explicito, aunque aqui ademas evita
// relanzar el fundido cada frame mientras dura.
static void CheckStairs(void)
{
    s8 x, y;

    // Turnos (cambio de genero): solo comprobar la escalera con el jugador
    // YA ASENTADO en su casilla (SimaActors_IsPlayerIdle), nunca a mitad de
    // un deslizamiento -- si no, el centro del sprite podria "adelantarse" a
    // la casilla de llegada antes de que el turno termine de verdad.
    if (!SimaActors_IsPlayerIdle())
        return;

    SimaActors_GetPlayerTile(&x, &y);
    // Tarea 6: pisar la escalera solo hace algo si ya está abierta (todos
    // los enemigos del piso muertos). SimaRoom_IsStairs por sí sola ya no
    // basta -- es geometría de sala, no si la escalera funciona todavía.
    if (SimaRoom_IsStairs(sCurrentFloor, x, y)
        && SimaActors_StairsUnlocked(SimaActors_GetAliveEnemyCount())
        && !gPaletteFade.active)
    {
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        sTransitionState = SIMA_TRANS_FADE_OUT;
    }
}

// Avanza la maquina de estados del cambio de piso. Con la pantalla ya en
// negro (fin del fundido de salida) es el momento de repintar BG0 y
// recolocar al jugador SIN que se vea: SimaActors_WarpToFloor, no
// SimaActors_InitPlayer, porque el sprite ya existe (ver la nota de
// idempotencia en sima.h).
static void UpdateFloorTransition(void)
{
    switch (sTransitionState)
    {
    case SIMA_TRANS_FADE_OUT:
        if (!gPaletteFade.active)
        {
            sCurrentFloor = SimaRoom_NextFloor(sCurrentFloor);
            DrawRoom(sCurrentFloor);
            CopyBgTilemapBufferToVram(0);
            SimaActors_WarpToFloor(sCurrentFloor);

            BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
            sTransitionState = SIMA_TRANS_FADE_IN;
        }
        break;

    case SIMA_TRANS_FADE_IN:
        if (!gPaletteFade.active)
            sTransitionState = SIMA_TRANS_NONE;
        break;
    }
}

static void CB2_SimaMain(void)
{
    RunTasks();

    if (sTransitionState == SIMA_TRANS_NONE)
    {
        SimaActors_UpdatePlayer();
        SimaActors_UpdateEnemies();
        UpdateStairsVisibility();
        CheckStairs();
    }
    else
    {
        UpdateFloorTransition();
    }

    DrawHud();
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

// Pinta en BG0 la sala real de `floor` leyendo SimaRoom_GetTileGfx
// (src/sima_rooms.c) celda a celda -- el indice de la celda YA COMPUESTA
// (fondo + objeto) en graphics/sima/tiles.png, no el SimaTile de colision
// (ese es SimaRoom_GetTile, para IsSolid/IsStairs). Se llama una vez al
// montar el modo (SetupGraphics) y de nuevo cada vez que
// UpdateFloorTransition cambia de piso al pisar una escalera (Tarea 5).
static void DrawRoom(u8 floor)
{
    s8 x, y;
    s8 stx, sty;
    u16 sheetTilesWide = SimaRoom_GetSheetTilesWide();
    // Tarea 6, cambio de diseño: la escalera no se pinta como tal mientras
    // quede algún enemigo vivo en el piso -- se dibuja como suelo llano y no
    // hace nada al pisarla (ver CheckStairs). SimaActors_StairsUnlocked es
    // la ÚNICA función que decide esto; aquí solo se lee su resultado.
    bool8 stairsUnlocked = SimaActors_StairsUnlocked(SimaActors_GetAliveEnemyCount());

    SimaRoom_GetStairs(floor, &stx, &sty);

    for (y = 0; y < SIMA_ROOM_H; y++)
    {
        for (x = 0; x < SIMA_ROOM_W; x++)
        {
            u16 tileGfx;
            if (x == stx && y == sty && !stairsUnlocked)
                tileGfx = SimaRoom_GetHiddenStairsGfx(floor);
            else
                tileGfx = SimaRoom_GetTileGfx(floor, x, y);
            PlaceCell(0, x, y, sheetTilesWide, 0, (u8)tileGfx, 0);
        }
    }

    sStairsVisible = stairsUnlocked;
}

// Si el estado de "escalera abierta" cambió desde el último frame, repinta
// la sala entera para que la escalera aparezca. Esto -- no una animación,
// no un aviso previo -- es la decisión de diseño tomada: la escalera
// APARECE DE GOLPE al morir el último enemigo. Reutiliza DrawRoom (que ya
// recorre las 15x10 celdas al montar el modo o cambiar de piso) en vez de
// repintar solo la celda de la escalera: mismo resultado, sin duplicar la
// lógica de qué gfx corresponde a cada celda. Es barato porque solo pasa
// UNA VEZ, en el frame exacto en que cae el último enemigo.
static void UpdateStairsVisibility(void)
{
    bool8 unlocked = SimaActors_StairsUnlocked(SimaActors_GetAliveEnemyCount());

    if (unlocked != sStairsVisible)
    {
        DrawRoom(sCurrentFloor);
        CopyBgTilemapBufferToVram(0);
    }
}

// Corazones de vida en BG1 (Tarea 6). hud_hearts.4bpp trae 2 celdas de 16x16
// en fila (llena, vacía -- ver graphics/sima/gen.py), así que su
// "sheetTilesWide" para PlaceCell son 2 celdas * 2 tiles = 4.
#define HUD_HEARTS_SHEET_TILES_WIDE 4
#define HUD_HEART_FULL_CELL  0
#define HUD_HEART_EMPTY_CELL 1

// Esquina SUPERIOR DERECHA, no la izquierda: BG1 tiene prioridad 0 (por
// delante de BG0 Y de los sprites, que van a prioridad 1 -- ver
// sSimaBgTemplates), así que los corazones tapan literalmente lo que haya
// debajo. Comprobado en la captura de esta tarea: en la esquina superior
// IZQUIERDA vive el arco de entrada del piso 1 (columna 1 de la fila 0, ver
// sRoomTileGfx en src/sima_rooms_data.h) -- justo donde spawnea el jugador
// -- así que ahí los corazones lo tapaban por completo. La esquina derecha
// (columnas 12-14 de la fila 0) es cenefa de muro en el piso 1, sin nada
// dinámico encima; no hay garantía de que siga siéndolo en pisos futuros
// (2/3 aún no están dibujados en el editor), pero es la mejor apuesta hoy.
#define HUD_HEARTS_COL_START (SIMA_ROOM_W - SIMA_PLAYER_MAX_HP)

static void DrawHud(void)
{
    u8 hp = SimaActors_GetPlayerHP();
    u8 i;

    if (hp == sHudDrawnHP)
        return;  // nada cambió: no tocar VRAM cada frame por nada

    for (i = 0; i < SIMA_PLAYER_MAX_HP; i++)
    {
        u8 cell = (i < hp) ? HUD_HEART_FULL_CELL : HUD_HEART_EMPTY_CELL;
        PlaceCell(1, HUD_HEARTS_COL_START + i, 0, HUD_HEARTS_SHEET_TILES_WIDE, 0, cell, 0);
    }
    CopyBgTilemapBufferToVram(1);
    sHudDrawnHP = hp;
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
    // HUD de corazones (Tarea 6): 8 tiles de hardware en el char block de
    // BG1 (ver el reparto de VRAM junto a sSimaBgTemplates). sBlankTile va
    // justo despues, en el indice HUD_TILE_COUNT.
    LoadBgTiles(1, sHudGfx, sizeof(sHudGfx), 0);
    LoadBgTiles(1, sBlankTile, sizeof(sBlankTile), HUD_TILE_COUNT);

    // ARREGLO DEL BUG REAL (ver el reparto de VRAM junto a sSimaBgTemplates):
    // SetBgTilemapBuffer acaba de dejar el tilemap de BG1 a cero, y la
    // casilla 0 por defecto NO es una celda vacia -- apunta al primer tile
    // de hud_hearts.4bpp, que tiene pixeles opacos de verdad. Hay que
    // rellenar el tilemap ENTERO con sBlankTile antes de que nada se
    // copie a VRAM; DrawHud (mas abajo) despues solo toca las 3 celdas de
    // los corazones, dejando el resto en blanco de verdad.
    FillBgTilemapBufferRect(1, HUD_TILE_COUNT, 0, 0, 32, 32, 0);

    // Paleta unica de SIMA (indice 0 transparente + 4 tonos): se carga una
    // sola vez en BG_PLTT_ID(0). Los corazones del HUD usan la misma paleta
    // (misma tabla de reindexado en graphics/sima/gen.py), así que no hace
    // falta cargar una segunda.
    LoadPalette(sTilesPal, BG_PLTT_ID(0), PLTT_SIZE_4BPP);

    DrawRoom(sCurrentFloor);
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
        // Reset explicito por si CB2_InitSima se reentra en la misma sesion
        // de ROM (p. ej. el arranque de depuracion PHANTOM_DEBUG_SIMA): sin
        // esto, sCurrentFloor/sTransitionState arrastrarian el valor de una
        // partida anterior en vez de arrancar en el piso 0 sin transicion en
        // marcha.
        sCurrentFloor = 0;
        sTransitionState = SIMA_TRANS_NONE;
        sHudDrawnHP = 0xFF;   // fuerza el primer pintado del HUD (ver DrawHud)

        // Los enemigos tienen que existir ANTES de pintar la sala:
        // SetupGraphics (más abajo) llama a DrawRoom, que consulta
        // SimaActors_GetAliveEnemyCount para decidir si la escalera se ve.
        // Sin este orden, DrawRoom leería 0 enemigos vivos (el .bss arranca
        // en 0) y pintaría la escalera abierta desde el primer frame.
        SimaActors_InitEnemies(sCurrentFloor);
        SetupGraphics();
        // SimaActors_InitPlayer vive en src/sima_actors.c y coloca el sprite
        // del jugador en el '@' de la sala (SimaRoom_GetSpawn). Las escaleras
        // cambian de piso via UpdateFloorTransition (Tarea 5).
        SimaActors_InitPlayer(sCurrentFloor);
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
