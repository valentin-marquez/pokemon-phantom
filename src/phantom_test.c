#include "global.h"
#include "phantom_test.h"

#ifdef PHANTOM_TEST

#include "load_save.h"
#include "new_game.h"
#include "main.h"
#include "save.h"
#include "constants/pokemon.h" // MALE/FEMALE

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

// Test 1 (Task 4): el Forastero es fijo (masculino, sin nombre editable).
static void Test_NewGameProtagonist(void)
{
    NewGameInitData();
    PHANTOM_ASSERT(gSaveBlock2Ptr->playerGender == MALE, "protagonist-male");
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
    PHANTOM_CHECKPOINT("suite-end");
    PhantomTest_Finish(gPhantomTestFailed);
}

#endif // PHANTOM_TEST
