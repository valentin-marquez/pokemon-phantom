#ifndef GUARD_MINIGAME_SPACESHIP_H
#define GUARD_MINIGAME_SPACESHIP_H

#include "global.h"

// Constantes públicas para el minijuego spaceship
#define MINIGAME_SPACESHIP_BG_WIDTH 256
#define MINIGAME_SPACESHIP_BG_HEIGHT 256

// Estados del minijuego spaceship
enum MinigameSpaceshipState
{
    SPACESHIP_STATE_INIT,
    SPACESHIP_STATE_GAMEPLAY,
    SPACESHIP_STATE_PAUSE,
    SPACESHIP_STATE_EXIT
};

// Callbacks principales
void CB2_InitMinigameSpaceship(void);

// Funciones auxiliares
void MinigameSpaceship_StopAllEffects(void);
bool8 MinigameSpaceship_IsActive(void);

// Estructuras de resultados del minijuego
struct MinigameSpaceshipResults
{
    u16 score;
    u8 timeElapsed;
    bool8 completed;
};

// Declaración del header para pre-minijuego
void CB2_InitPreMinigame(void);

#endif // GUARD_MINIGAME_SPACESHIP_H
