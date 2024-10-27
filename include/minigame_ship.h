#ifndef GUARD_MINIGAME_SHIP_H
#define GUARD_MINIGAME_SHIP_H

#include "global.h"

// Constantes públicas para el minijuego
#define MINIGAME_SHIP_BG_WIDTH 256
#define MINIGAME_SHIP_BG_HEIGHT 256

// Estados del minijuego que pueden ser útiles para otros archivos
enum MinigameShipState
{
    MINIGAME_STATE_INTRO,
    MINIGAME_STATE_STORY,
    MINIGAME_STATE_GAMEPLAY,
    MINIGAME_STATE_EXIT
};

// Callbacks principales
void CB2_InitMinigameShip(void);

// Funciones auxiliares que podrían ser útiles para otros archivos
void MinigameShip_StopAllEffects(void);
bool8 MinigameShip_IsActive(void);

// Estructuras que podrían ser necesarias para la comunicación entre archivos
struct MinigameShipResults
{
    u16 score;
    u8 timeElapsed;
    bool8 completed;
};

// Declaraciones de datos externos necesarios
extern const struct BgTemplate gMinigameShipBgTemplates[];
extern const u32 gMinigameShipBackgroundGfx[];
extern const u32 gMinigameShipBackgroundTilemap[];
extern const u16 gMinigameShipBackgroundPal[];

// Si vas a tener sprites específicos del minijuego
extern const struct CompressedSpriteSheet gMinigameShipSpriteSheets[];
extern const struct SpritePalette gMinigameShipSpritePalettes[];
extern const struct SpriteTemplate gMinigameShipSpriteTemplates[];

// Declaración de cualquier callback o función que necesite ser accesible desde otros archivos
void MinigameShip_SetCallback(void (*callback)(void));

#endif // GUARD_MINIGAME_SHIP_H