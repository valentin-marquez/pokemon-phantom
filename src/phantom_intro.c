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
#include "constants/rgb.h"

// Punto de entrada de la secuencia de intro (Pieza 2 lo sustituirá).
#define ENTRADA_INTRO CB2_InitMinigameShip

static void GoTo(MainCallback nextCB)
{
    FadeOutBGM(4);
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    SetMainCallback2(nextCB);
}

void PhantomIntro_OnStartPressed(void)
{
    if (gSaveFileStatus == SAVE_STATUS_OK)
        GoTo(CB2_ContinueSavedGame);   // temporal: menú se añade en Task 4
    else
        GoTo(ENTRADA_INTRO);
}
