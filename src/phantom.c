#include "global.h"
#include "phantom.h"
#include "event_data.h"
#include "constants/phantom.h"
#include "constants/flags.h"

// Avanza el reloj narrativo (llamado al dormir).
void PhantomAdvanceDay(void)
{
    VarSet(VAR_PHANTOM_TIME, VarGet(VAR_PHANTOM_TIME) + 1);
}

// Marca que el jugador presenció la ejecución (swap de NPCs intra-día).
void PhantomMarkExecutionSeen(void)
{
    FlagSet(FLAG_PHANTOM_SAW_EXECUTION);
}
