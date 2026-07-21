#ifndef GUARD_TITLE_SCREEN_H
#define GUARD_TITLE_SCREEN_H

void CB2_InitTitleScreen(void);

// Muestra/oculta el sprite parpadeante "PRESS START" (lo usa el menú
// Nueva/Continuar de phantom_intro.c para ceder el lugar al overlay y
// devolvérselo al cerrar).
void TitleScreen_SetPressStartVisible(bool8 visible);

#endif // GUARD_TITLE_SCREEN_H
