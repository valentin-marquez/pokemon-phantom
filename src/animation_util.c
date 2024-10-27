/* animation_util.c */
#include "global.h"
#include "palette.h"
#include "malloc.h"
#include "constants/rgb.h"
#include "animation_util.h"

#define RED_WEIGHT 30
#define GREEN_WEIGHT 59
#define BLUE_WEIGHT 11
#define SCALE_FACTOR 100

static u16 ConvertToGrayscale(u16 color)
{
    u8 r = GET_R(color);
    u8 g = GET_G(color);
    u8 b = GET_B(color);
    u8 gray = (u8)((r * RED_WEIGHT + g * GREEN_WEIGHT + b * BLUE_WEIGHT) / SCALE_FACTOR);
    return RGB2(gray, gray, gray);
}

static u16 InterpolateColor(u16 color1, u16 color2, u8 step, u8 totalSteps)
{
    u8 r1, g1, b1;
    u8 r2, g2, b2;
    u8 r, g, b;

    if (step >= totalSteps)
        return color2;

    r1 = GET_R(color1);
    g1 = GET_G(color1);
    b1 = GET_B(color1);

    r2 = GET_R(color2);
    g2 = GET_G(color2);
    b2 = GET_B(color2);

    r = r1 + (((r2 - r1) * step) / totalSteps);
    g = g1 + (((g2 - g1) * step) / totalSteps);
    b = b1 + (((b2 - b1) * step) / totalSteps);

    return RGB2(r, g, b);
}

struct PaletteAnimator *CreatePaletteAnimator(const u16 *targetPal, u8 offset, u8 size, u8 steps)
{
    struct PaletteAnimator *animator;
    u8 i;

    animator = AllocZeroed(sizeof(struct PaletteAnimator));
    if (animator == NULL)
        return NULL;

    animator->targetPalette = AllocZeroed(size * 2); /* u16 = 2 bytes */
    animator->currentPalette = AllocZeroed(size * 2);
    animator->originalPalette = AllocZeroed(size * 2);

    if (animator->targetPalette == NULL ||
        animator->currentPalette == NULL ||
        animator->originalPalette == NULL)
    {
        DestroyPaletteAnimator(animator);
        return NULL;
    }

    /* Copiar paletas */
    for (i = 0; i < size; i++)
    {
        animator->targetPalette[i] = targetPal[i];
        animator->originalPalette[i] = gPlttBufferUnfaded[offset + i];
        animator->currentPalette[i] = gPlttBufferUnfaded[offset + i];
    }

    animator->paletteOffset = offset;
    animator->colorCount = size;
    animator->totalSteps = steps;
    animator->step = 0;
    animator->isActive = TRUE;
    animator->sequence = NULL;

    return animator;
}

void SetPaletteAnimatorSequence(struct PaletteAnimator *animator, const u8 *colorIndexes, u8 count, u8 delay)
{
    struct ColorSequence *sequence;
    u8 i;

    if (animator == NULL)
        return;

    /* Liberar secuencia anterior si existe */
    if (animator->sequence != NULL)
    {
        if (animator->sequence->indexes != NULL)
            Free(animator->sequence->indexes);
        Free(animator->sequence);
    }

    sequence = AllocZeroed(sizeof(struct ColorSequence));
    if (sequence == NULL)
        return;

    sequence->indexes = AllocZeroed(count);
    if (sequence->indexes == NULL)
    {
        Free(sequence);
        return;
    }

    for (i = 0; i < count; i++)
        sequence->indexes[i] = colorIndexes[i];

    sequence->count = count;
    sequence->currentIndex = 0;
    sequence->delay = delay;
    sequence->currentDelay = 0;

    animator->sequence = sequence;
    animator->step = 0;
    animator->isActive = TRUE;
}

void StartPaletteAnimatorGrayscale(struct PaletteAnimator *animator)
{
    u8 i;

    if (animator == NULL)
        return;

    /* Convertir paleta actual a escala de grises */
    for (i = 0; i < animator->colorCount; i++)
    {
        if (i != 0) /* Preservar color transparente */
            animator->currentPalette[i] = ConvertToGrayscale(animator->originalPalette[i]);
    }

    /* Aplicar paleta en escala de grises */
    for (i = 0; i < animator->colorCount; i++)
        gPlttBufferFaded[animator->paletteOffset + i] = animator->currentPalette[i];

    animator->step = 0;
    animator->isActive = TRUE;
}

void UpdatePaletteAnimator(struct PaletteAnimator *animator)
{
    u8 i;

    if (animator == NULL || !animator->isActive)
        return;

    if (animator->sequence != NULL)
    {
        /* Manejar animación secuencial */
        if (animator->sequence->currentDelay < animator->sequence->delay)
        {
            animator->sequence->currentDelay++;
            return;
        }

        animator->sequence->currentDelay = 0;

        /* Animar solo el color actual en la secuencia */
        i = animator->sequence->indexes[animator->sequence->currentIndex];
        if (i < animator->colorCount)
        {
            animator->currentPalette[i] = InterpolateColor(
                animator->currentPalette[i],
                animator->targetPalette[i],
                animator->step,
                animator->totalSteps);

            gPlttBufferFaded[animator->paletteOffset + i] = animator->currentPalette[i];
        }

        /* Incrementar paso */
        animator->step++;
        if (animator->step >= animator->totalSteps)
        {
            animator->step = 0;
            animator->sequence->currentIndex++;

            if (animator->sequence->currentIndex >= animator->sequence->count)
                animator->isActive = FALSE;
        }
    }
    else
    {
        /* Animar todos los colores simultáneamente */
        for (i = 1; i < animator->colorCount; i++)
        {
            animator->currentPalette[i] = InterpolateColor(
                animator->currentPalette[i],
                animator->targetPalette[i],
                animator->step,
                animator->totalSteps);

            gPlttBufferFaded[animator->paletteOffset + i] = animator->currentPalette[i];
        }

        animator->step++;
        if (animator->step >= animator->totalSteps)
            animator->isActive = FALSE;
    }
}

void DestroyPaletteAnimator(struct PaletteAnimator *animator)
{
    if (animator == NULL)
        return;

    if (animator->targetPalette != NULL)
        Free(animator->targetPalette);
    if (animator->currentPalette != NULL)
        Free(animator->currentPalette);
    if (animator->originalPalette != NULL)
        Free(animator->originalPalette);
    if (animator->sequence != NULL)
    {
        if (animator->sequence->indexes != NULL)
            Free(animator->sequence->indexes);
        Free(animator->sequence);
    }

    Free(animator);
}

bool8 IsPaletteAnimatorActive(struct PaletteAnimator *animator)
{
    if (animator == NULL)
        return FALSE;
    return animator->isActive;
}