/* animation_util.h */
#ifndef GUARD_ANIMATION_UTIL_H
#define GUARD_ANIMATION_UTIL_H

#include "global.h"

/* Estructura para definir una secuencia de colores a animar */
struct ColorSequence
{
    u8 *indexes;     /* Índices de colores a animar */
    u8 count;        /* Cantidad de colores en la secuencia */
    u8 currentIndex; /* Índice actual en la secuencia */
    u8 delay;        /* Retardo entre cada paso de la animación */
    u8 currentDelay; /* Contador actual del retardo */
};

/* Estructura para controlar la animación de paletas */
struct PaletteAnimator
{
    u16 *targetPalette;             /* Paleta objetivo final */
    u16 *currentPalette;            /* Paleta actual en transición */
    u16 *originalPalette;           /* Paleta original antes de la animación */
    u8 paletteOffset;               /* Offset en PLTT donde se encuentra la paleta */
    u8 colorCount;                  /* Cantidad de colores en la paleta */
    struct ColorSequence *sequence; /* Secuencia de animación de colores */
    u8 step;                        /* Paso actual de la animación */
    u8 totalSteps;                  /* Total de pasos para completar la animación */
    bool8 isActive;                 /* Indica si la animación está activa */
};

/* Funciones principales */
struct PaletteAnimator *CreatePaletteAnimator(const u16 *targetPal, u8 offset, u8 size, u8 steps);
void DestroyPaletteAnimator(struct PaletteAnimator *animator);
void UpdatePaletteAnimator(struct PaletteAnimator *animator);
bool8 IsPaletteAnimatorActive(struct PaletteAnimator *animator);

/* Funciones de utilidad */
void SetPaletteAnimatorSequence(struct PaletteAnimator *animator, const u8 *colorIndexes, u8 count, u8 delay);
void StartPaletteAnimatorGrayscale(struct PaletteAnimator *animator);
void StartPaletteAnimatorFadeIn(struct PaletteAnimator *animator);
void StartPaletteAnimatorSequential(struct PaletteAnimator *animator);

#endif /* GUARD_ANIMATION_UTIL_H */