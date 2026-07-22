#include "global.h"
#include "phantom_test.h"

#ifdef PHANTOM_TEST

#include "load_save.h"
#include "new_game.h"
#include "main.h"
#include "save.h"
#include "event_data.h"
#include "phantom.h"
#include "constants/phantom.h"
#include "constants/flags.h"
#include "start_menu.h"
#include "wild_encounter.h"
#include "sima_rooms.h"
#include "sima.h"

u8 gPhantomTestFailed = 0;

// Termina la emulación: deja exitCode en r0 y ejecuta el svc que
// mgba-rom-test observa (invocado con -S 0x27 -R r0).
void PhantomTest_Finish(u8 exitCode)
{
    asm volatile("mov r0, %0\n\t"
                 "svc 0x27\n"
                 :: "r"(exitCode) : "r0", "memory");
    while (1) {} // por si el svc no corta la ejecución
}

// Test 0: el ROM compila, arranca y el código se ejecuta.
static void Test_BootOk(void)
{
    PHANTOM_CHECKPOINT("boot");
    PHANTOM_ASSERT(TRUE, "boot-reached");
}

// NOTA (gap de cobertura): estos tests llaman NewGameInitData() directo; NO
// ejercitan el dispatch real ACTION_NEW_GAME -> CB2_NewGame (main_menu.c). El
// recorte de Birch queda verificado "por construcción", no automatizado —
// cerrar cuando haya inyección de inputs (Fase 2 del harness, libmgba-py).
// Test 1 (Task 4): el Forastero es fijo (masculino, sin nombre editable).
static void Test_NewGameProtagonist(void)
{
    // Ensucia los campos ANTES para que las aserciones fallen (RED real)
    // si NewGameInitData no fija el protagonista.
    // EOS (0xFF) no está en scope en este archivo (no se incluye
    // constants/characters.h por esta cadena de headers); se usa el literal
    // directamente, que es el terminador de string estándar del juego.
    gSaveBlock2Ptr->playerGender = FEMALE;
    gSaveBlock2Ptr->playerName[0] = 0xFF;   // nombre vacío (EOS)
    NewGameInitData();
    PHANTOM_ASSERT(gSaveBlock2Ptr->playerGender == MALE, "protagonist-male");
    PHANTOM_ASSERT(gSaveBlock2Ptr->playerName[0] != 0xFF, "protagonist-name-set");
}

// Test 2 (Task 5): el reloj narrativo arranca en PROLOGUE tras New Game.
static void Test_PhantomTimeInit(void)
{
    NewGameInitData();
    PHANTOM_ASSERT(VarGet(VAR_PHANTOM_TIME) == PHANTOM_TIME_PROLOGUE, "phantom-time-prologue");
}

// Test 3 (Task 6): el guardado es diegético; SAVE no aparece en el start menu.
static void Test_NoSaveInStartMenu(void)
{
    PHANTOM_ASSERT(PhantomTest_StartMenuHasSave() == FALSE, "no-save-in-startmenu");
}

// Test 4 (Task 7): sin mapas de isla todavía, los encuentros salvajes se
// desactivan globalmente al iniciar partida ("todo lo que te ataca te eligió").
static void Test_NoWildEncounters(void)
{
    // Ensucia el flag ANTES para que la aserción falle (RED real) si
    // NewGameInitData no lo fija: sWildEncountersDisabled es static y
    // persiste entre llamadas de NewGameInitData en la misma suite.
    DisableWildEncounters(FALSE);
    NewGameInitData();
    PHANTOM_ASSERT(PhantomTest_WildEncountersDisabled() == TRUE, "no-wild-encounters");
}

// Test 5 (Task 5): el special PhantomAdvanceDay avanza el reloj narrativo un paso.
static void Test_PhantomAdvanceDay(void)
{
    VarSet(VAR_PHANTOM_TIME, PHANTOM_TIME_PROLOGUE);
    PhantomAdvanceDay();
    PHANTOM_ASSERT(VarGet(VAR_PHANTOM_TIME) == PHANTOM_TIME_DAY1, "advance-day");
}

// Test 6 (Task 5): el special PhantomMarkExecutionSeen enciende FLAG_PHANTOM_SAW_EXECUTION.
static void Test_PhantomExecutionSeen(void)
{
    FlagClear(FLAG_PHANTOM_SAW_EXECUTION);
    PhantomMarkExecutionSeen();
    PHANTOM_ASSERT(FlagGet(FLAG_PHANTOM_SAW_EXECUTION) == TRUE, "saw-execution");
}

// Test 7 (SIMA): las salas son validas por construccion -- cerradas por muros,
// con exactamente una escalera y un spawn transitable. Un fallo aqui es una
// sala mal dibujada, y sin esta comprobacion solo se detectaria jugando.
static void Test_SimaRoomsValid(void)
{
    u8 floor;
    bool8 allEnclosed = TRUE;
    bool8 allHaveOneStairs = TRUE;
    bool8 allSpawnsWalkable = TRUE;

    for (floor = 0; floor < SIMA_FLOOR_COUNT; floor++)
    {
        s8 x, y;
        u32 stairs = 0;
        for (x = 0; x < SIMA_ROOM_W; x++)
        {
            if (!SimaRoom_IsSolid(floor, x, 0) || !SimaRoom_IsSolid(floor, x, SIMA_ROOM_H - 1))
                allEnclosed = FALSE;
        }
        for (y = 0; y < SIMA_ROOM_H; y++)
        {
            if (!SimaRoom_IsSolid(floor, 0, y) || !SimaRoom_IsSolid(floor, SIMA_ROOM_W - 1, y))
                allEnclosed = FALSE;
        }
        for (y = 0; y < SIMA_ROOM_H; y++)
            for (x = 0; x < SIMA_ROOM_W; x++)
                if (SimaRoom_IsStairs(floor, x, y))
                    stairs++;
        if (stairs != 1)
            allHaveOneStairs = FALSE;
        {
            s8 sx, sy;
            SimaRoom_GetSpawn(floor, &sx, &sy);
            if (SimaRoom_IsSolid(floor, sx, sy))
                allSpawnsWalkable = FALSE;
        }
    }

    PHANTOM_ASSERT(allEnclosed, "sima-rooms-enclosed");
    PHANTOM_ASSERT(allHaveOneStairs, "sima-rooms-one-stairs");
    PHANTOM_ASSERT(allSpawnsWalkable, "sima-spawns-walkable");
    // Fuera de rango debe ser solido en las cuatro direcciones, o el jugador se sale de la sala.
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, -1, 5), "sima-oob-solid-left");
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, SIMA_ROOM_W, 5), "sima-oob-solid-right");
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, 5, -1), "sima-oob-solid-top");
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, 5, SIMA_ROOM_H), "sima-oob-solid-bottom");
}

// Test 8 (Task 4): la caja de colision del jugador (SimaActors_BoxFits,
// src/sima_actors.c) es una funcion pura -- sin sprite ni input de por
// medio, se puede probar aqui igual que Test_SimaRoomsValid prueba
// SimaRoom_IsSolid. Cabe en la casilla de spawn de cada piso (que
// Test_SimaRoomsValid ya certifico transitable), no cabe en la esquina
// superior izquierda de la sala (dentro del anillo de muros en las tres) ni
// muy fuera de sus limites.
static void Test_SimaPlayerBoxFits(void)
{
    u8 floor;
    bool8 allSpawnsFit = TRUE;
    bool8 allWallsBlock = TRUE;

    for (floor = 0; floor < SIMA_FLOOR_COUNT; floor++)
    {
        s8 sx, sy;
        SimaRoom_GetSpawn(floor, &sx, &sy);
        if (!SimaActors_BoxFits(floor, (s16)sx * 16, (s16)sy * 16))
            allSpawnsFit = FALSE;

        // (0,0) cae en el anillo de muros en las tres salas: la caja del
        // jugador no puede caber ahi.
        if (SimaActors_BoxFits(floor, 0, 0))
            allWallsBlock = FALSE;
    }

    PHANTOM_ASSERT(allSpawnsFit, "sima-player-box-fits-spawn");
    PHANTOM_ASSERT(allWallsBlock, "sima-player-box-blocked-by-wall");
    // Muy fuera de la sala: SimaRoom_IsSolid ya devuelve TRUE fuera de rango
    // (ver sima-oob-solid-* arriba), asi que la caja tampoco deberia caber.
    PHANTOM_ASSERT(!SimaActors_BoxFits(0, -100, -100), "sima-player-box-blocked-oob");

    // Casos de margen (revision de codigo, Tarea 4): los tres casos de arriba
    // caen todos dentro de una sola celda de la rejilla en las cuatro
    // esquinas de la caja, asi que no detectarian un off-by-one en la
    // aritmetica de SimaActors_BoxFits (p.ej. "right = left + COLLISION_W"
    // olvidando el "-1" -- right pasaria de 29 a 30 y ambos numeros siguen
    // cayendo en la misma celda [16,31], el test seguiria en verde).
    //
    // Se usa el piso 0 (src/sima_rooms.c, sFloor0), fila y=1: el pasillo
    // superior "#@...........*#" es todo suelo de columna 1 a 13 y muro en
    // la columna 14 (x=14 es '#'). Se elige el borde DERECHO del pasillo
    // (columna 14) en vez del izquierdo porque es ahi donde se calcula
    // "right" -- la variable que el bug hipotetico de arriba corrompe; el
    // margen izquierdo usa "left", que no tiene ese "-1" y no lo detectaria.
    //
    // Constantes relevantes (src/sima_actors.c): COLLISION_W = 12,
    // COLLISION_MARGIN_X = (16 - 12) / 2 = 2. Con "x" la esquina superior
    // izquierda del sprite (parametro de SimaActors_BoxFits):
    //   left  = x + 2
    //   right = left + COLLISION_W - 1 = left + 11
    //
    // Se fija y = 16 (fila 1 completa, pixeles 16-31) para que arriba/abajo
    // de la caja nunca toquen una fila distinta y solo el eje X este en
    // juego:
    //   top    = y + 2  = 18  -> fila 18/16 = 1
    //   bottom = top + 11 = 29 -> fila 29/16 = 1 (misma fila que top)
    //
    // Muro (columna 14) empieza en el pixel 14*16 = 224. Suelo (columna 13,
    // la casilla '*' del pasillo) termina en el pixel 13*16+15 = 223.
    //
    // Caso "libra por 1px" (x = 210): left = 212, right = 212+11 = 223.
    // right/16 = 13 -> suelo en las cuatro esquinas -> debe caber (TRUE).
    // Con el bug hipotetico (right = left+12 = 224), right/16 pasaria a 14
    // (muro) y este caso fallaria en falso -- exactamente el off-by-one que
    // el caso de abajo, solo, no distingue.
    PHANTOM_ASSERT(SimaActors_BoxFits(0, 210, 16), "sima-box-clear-1px");

    // Caso "solapa por 1px" (x = 211, un pixel mas cerca del muro): left =
    // 213, right = 213+11 = 224. right/16 = 14 (muro, columna 14) -> la
    // esquina superior derecha de la caja cae sobre el muro -> no debe caber
    // (FALSE).
    PHANTOM_ASSERT(!SimaActors_BoxFits(0, 211, 16), "sima-box-blocked-1px");
}

void PhantomTest_Run(void)
{
    PHANTOM_CHECKPOINT("suite-start");
    Test_BootOk();
    // Setup compartido de estado para los tests de new game (Tasks 4/5/7).
    SetSaveBlocksPointers(GetSaveBlocksPointersBaseOffset());
    PHANTOM_CHECKPOINT("before-newgame");
    Test_NewGameProtagonist();
    PHANTOM_CHECKPOINT("after-newgame");
    Test_PhantomTimeInit();
    Test_NoSaveInStartMenu();
    Test_NoWildEncounters();
    Test_PhantomAdvanceDay();
    Test_PhantomExecutionSeen();
    Test_SimaRoomsValid();
    Test_SimaPlayerBoxFits();
    PHANTOM_CHECKPOINT("suite-end");
    PhantomTest_Finish(gPhantomTestFailed);
}

#endif // PHANTOM_TEST
