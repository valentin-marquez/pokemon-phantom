#ifndef GUARD_PHANTOM_INTRO_H
#define GUARD_PHANTOM_INTRO_H

// Front-end del título. Llamado por title_screen.c al pulsar A/START.
// Si hay save, abre el menú Nueva/Continuar; si no, decide la ruta directo
// (nueva partida) y reproduce el vidrio impactado.
void PhantomIntro_OnStartPressed(void);

// TRUE si el menú Nueva/Continuar está abierto o el vidrio impactado está en
// marcha. Task_TitleScreenMain debe gatear su manejo de A/START con esto para
// no reabrir el menú ni relanzar el vidrio mientras cualquiera de los dos corre.
bool8 PhantomIntro_IsBusy(void);

#endif // GUARD_PHANTOM_INTRO_H
