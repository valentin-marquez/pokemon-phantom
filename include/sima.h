#ifndef GUARD_SIMA_H
#define GUARD_SIMA_H

// SIMA: el dungeon crawler ficticio que el protagonista juega en la consola
// durante el prologo. Ver docs/superpowers/specs/2026-07-21-prologo-consola-design.md
void CB2_InitSima(void);

// Vida del jugador (Tarea 6): 3 corazones, vease DrawHud en src/sima.c.
#define SIMA_PLAYER_MAX_HP 3

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

// Daño (Tarea 6). Funcion pura, sin estado: resta `amount` de `hp` saturando
// en 0 (nunca desborda hacia 255, que haria al jugador inmortal justo cuando
// deberia morir). Expuesta para el harness in-ROM, igual que SimaActors_BoxFits.
u8 SimaActors_ApplyDamage(u8 hp, u8 amount);
// Vida actual del jugador (para el HUD, src/sima.c) y si ya llego a 0.
u8 SimaActors_GetPlayerHP(void);
bool8 SimaActors_IsPlayerDead(void);

// Enemigos (Tarea 6): rata/murcielago/slime colocados en las casillas de
// SimaRoom_GetEnemy, con movimiento simple hacia el jugador (misma colision
// de pared que SimaActors_BoxFits, mismo tamaño de sprite) y daño por
// contacto con invulnerabilidad breve tras cada golpe. Se inicializan una
// sola vez al montar el modo, igual que SimaActors_InitPlayer (no hay
// SimaActors_WarpEnemiesToFloor todavia: con SIMA_FLOOR_COUNT en 1 no hace
// falta, y anadirlo sin poder probarlo en un piso 2 real seria codigo sin
// ejercitar -- pendiente para cuando el piso 2 se dibuje en el editor).
void SimaActors_InitEnemies(u8 floor);
void SimaActors_UpdateEnemies(void);
// Cuantos de los enemigos colocados en el piso siguen vivos ahora mismo.
u8 SimaActors_GetAliveEnemyCount(void);

// Pura, sin sprites (harness in-ROM, igual que SimaActors_BoxFits): dado un
// numero de enemigos vivos, ¿esta la escalera abierta? Aislada en su propia
// funcion de una linea porque es una decision de diseño que puede cambiar
// (hoy: "aparece de golpe" al morir el ultimo, no "visible pero apagada" --
// ver el comentario junto a la implementacion en src/sima_actors.c).
bool8 SimaActors_StairsUnlocked(u8 aliveEnemyCount);

#endif // GUARD_SIMA_H
