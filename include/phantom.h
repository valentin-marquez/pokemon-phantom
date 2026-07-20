#ifndef GUARD_PHANTOM_H
#define GUARD_PHANTOM_H

void PhantomAdvanceDay(void);
void PhantomMarkExecutionSeen(void);
void Phantom_TintPaletteRange(u16 offset, u16 count);
void PhantomReloadOverworldPalettes(void);

// Efecto de mareo/distorsión de lente sobre el overworld (src/phantom_fx.c).
void PhantomFx_StartMareo(void);
void PhantomFx_StopMareo(void);
void PhantomFx_RetintObjectSprites(void);
void PhantomFx_OnMapLoad(void);   // re-arma el mareo en cada carga de mapa
// Specials para scripts: VAR_0x8004=amplitud, VAR_0x8005=velocidad + PhantomMareoOn.
void PhantomMareoOn(void);
void PhantomMareoOff(void);

#endif // GUARD_PHANTOM_H
