#ifndef GUARD_PHANTOM_TEST_H
#define GUARD_PHANTOM_TEST_H

// Harness de smoke test in-ROM. Solo activo con -DPHANTOM_TEST.
// Corre bajo mgba-rom-test: emite checkpoints por el log de mGBA y
// termina con `svc 0x27` dejando el exit code en r0 (0 = OK, 1 = fallo).

#include "gba/isagbprint.h"

#define PHANTOM_CHECKPOINT(name) DebugPrintf(":P %s", name)

extern u8 gPhantomTestFailed;

#define PHANTOM_ASSERT(cond, name)                    \
    do {                                              \
        if (cond) {                                   \
            DebugPrintf(":P PASS %s", name);          \
        } else {                                      \
            DebugPrintf(":P FAIL %s", name);          \
            gPhantomTestFailed = 1;                   \
        }                                             \
    } while (0)

void PhantomTest_Run(void);      // corre la secuencia; no retorna
void PhantomTest_Finish(u8 exitCode);

#endif // GUARD_PHANTOM_TEST_H
