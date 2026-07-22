#ifndef GUARD_SIMA_H
#define GUARD_SIMA_H

// SIMA: el dungeon crawler ficticio que el protagonista juega en la consola
// durante el prologo. Ver docs/superpowers/specs/2026-07-21-prologo-consola-design.md
void CB2_InitSima(void);

// Jugador de SIMA (Tarea 4, src/sima_actors.c): sprite de 16x16 movido en
// pixeles con colision por casilla contra SimaRoom_IsSolid.
void SimaActors_InitPlayer(u8 floor);
void SimaActors_UpdatePlayer(void);
// Recoloca al jugador ya existente en el spawn de `floor`, sin volver a
// LoadSpriteSheet/LoadSpritePalette/CreateSprite (Tarea 5: LoadSpriteSheet no
// es idempotente -- llamarlo dos veces con el mismo tag pisa/desperdicia
// memoria de VRAM). Para cambios de piso (escaleras), no para el arranque
// del modo (eso sigue siendo SimaActors_InitPlayer).
void SimaActors_WarpToFloor(u8 floor);
// Casilla que ocupa el centro del sprite del jugador ahora mismo (para
// logica de tareas posteriores: escaleras, disparadores, enemigos).
void SimaActors_GetPlayerTile(s8 *x, s8 *y);
// Funcion pura de colision (sin sprite ni input): ¿cabe la caja de colision
// del jugador en (x, y) [esquina superior izquierda del sprite de 16x16, en
// pixeles] sin superponerse a un muro del piso dado? Expuesta para que el
// harness in-ROM (src/phantom_test.c) la pueda probar sin inputs.
bool8 SimaActors_BoxFits(u8 floor, s16 x, s16 y);

#endif // GUARD_SIMA_H
