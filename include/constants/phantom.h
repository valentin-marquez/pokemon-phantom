#ifndef GUARD_CONSTANTS_PHANTOM_H
#define GUARD_CONSTANTS_PHANTOM_H

// Estado-mundo de Pokémon Phantom. Ver docs/superpowers/specs/2026-07-17-pokemon-phantom-design.md

// Reutiliza el primer VAR_UNUSED persistente libre (verificado: vars.h:98).
#define VAR_PHANTOM_TIME   VAR_UNUSED_0x404E

// Franjas narrativas (el tiempo avanza solo al dormir).
// 0 = reloj sin arrancar. Importante: InitEventData (dentro de NewGameInitData)
// pone TODAS las vars a 0, así que el estado por defecto de una partida es UNSET;
// NewGameInitData debe fijar explícitamente PROLOGUE. Por eso PROLOGUE != 0
// (si fuera 0, la aserción del test sería tautológica: 0 tras el zero-init).
#define PHANTOM_TIME_UNSET     0
#define PHANTOM_TIME_PROLOGUE  1
#define PHANTOM_TIME_DAY1      2
#define PHANTOM_TIME_DAY2      3
#define PHANTOM_TIME_DAY3      4
#define PHANTOM_TIME_DAWN      5

#endif // GUARD_CONSTANTS_PHANTOM_H
