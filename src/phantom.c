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
        // Tono verde enfermizo ("luna mala"): el gris se sesga hacia el verde.
        s32 tr = gray * 12 / 16;
        s32 tg = gray * 17 / 16;
        s32 tb = gray * 10 / 16;
        if (tg > 31) tg = 31;

        r = (r + tr) >> 1;
        g = (g + tg) >> 1;
        b = (b + tb) >> 1;

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
