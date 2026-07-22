#ifndef GUARD_TITLE_SCREEN_H
#define GUARD_TITLE_SCREEN_H

void CB2_InitTitleScreen(void);

// Muestra/oculta el sprite parpadeante "PRESS START" (lo usa el menú
// Nueva/Continuar de phantom_intro.c para ceder el lugar al overlay y
// devolvérselo al cerrar).
void TitleScreen_SetPressStartVisible(bool8 visible);

// Offset base de BG0 (el logo) en píxeles. SetMainTitleScreen() lo reasigna con
// BG_COORD_SET *cada frame*, así que cualquier efecto que escriba BG0HOFS/VOFS
// directamente (la sacudida del vidrio, ver src/phantom_intro.c) tiene que
// SUMARSE a esta base en vez de pisarla: escribir el offset crudo hace que el
// logo pegue un salto de 46px hacia abajo mientras dure el efecto.
#define TITLE_LOGO_BG_X   (-4)
#define TITLE_LOGO_BG_Y  (-46)

// Restaura el tilemap del logo si hay un glitch a medias y lo desactiva.
// Necesario antes de una secuencia que congele el glitch (el vidrio): si se
// deja de llamar a UpdateGlitchEffect() con un glitch activo, el logo se queda
// desgarrado en pantalla hasta que la secuencia termine.
void TitleScreen_CancelGlitch(void);

#endif // GUARD_TITLE_SCREEN_H
