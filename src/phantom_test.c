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
    PHANTOM_CHECKPOINT("suite-end");
    PhantomTest_Finish(gPhantomTestFailed);
}

#endif // PHANTOM_TEST
