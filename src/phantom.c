#include "global.h"
#include "phantom.h"
#include "event_data.h"
#include "fieldmap.h"
#include "palette.h"
#include "constants/phantom.h"
#include "constants/flags.h"
#include "constants/rgb.h"

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

// Desatura [offset, offset+count) tanto en el buffer unfaded como en el faded,
// para que el tinte sobreviva al fade-in/out y a los repintados de clima
// (que leen de unfaded y rescriben faded, field_weather.c). Gateado por la
// flag persistente que enciende la ejecución de Meowth; NUNCA se llama desde
// el LoadPalette global (rompería menús/combate).
void Phantom_TintPaletteRange(u16 offset, u16 count)
{
    u16 i;

    if (!FlagGet(FLAG_PHANTOM_MEOWTH_EXECUTED))
        return;

    for (i = 0; i < count; i++)
    {
        u16 c = gPlttBufferUnfaded[offset + i];
        s32 r = GET_R(c);
        s32 g = GET_G(c);
        s32 b = GET_B(c);
        u32 gray = (r * Q_8_8(0.3) + g * Q_8_8(0.59) + b * Q_8_8(0.1133)) >> 8;
        // Tono rojizo enfermizo: el gris se sesga hacia el rojo, mezcla ~62%
        // (3/8 original + 5/8 tono) — marcado pero aún legible.
        s32 tr = gray * 18 / 16;
        s32 tg = gray * 11 / 16;
        s32 tb = gray * 11 / 16;
        // Solo tr puede pasar de 31 (18/16 > 1); RGB2 NO enmascara → clampar.
        if (tr > 31) tr = 31;

        r = (r * 3 + tr * 5) >> 3;
        g = (g * 3 + tg * 5) >> 3;
        b = (b * 3 + tb * 5) >> 3;

        gPlttBufferUnfaded[offset + i] = RGB2(r, g, b);
        gPlttBufferFaded[offset + i]   = RGB2(r, g, b);
    }
}

// Fuerza la recarga de las paletas del tileset del mapa actual para que el
// tinte se vea de inmediato tras encender la flag, sin esperar a un warp.
void PhantomReloadOverworldPalettes(void)
{
    LoadMapTilesetPalettes(gMapHeader.mapLayout);
}
