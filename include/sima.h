#ifndef GUARD_SIMA_H
#define GUARD_SIMA_H

// SIMA: el dungeon crawler ficticio que el protagonista juega en la consola
// durante el prologo. Ver docs/superpowers/specs/2026-07-21-prologo-consola-design.md
void CB2_InitSima(void);

// Vida del jugador (Tarea 6): 3 corazones, vease DrawHud en src/sima.c.
#define SIMA_PLAYER_MAX_HP 3

// Direccion. Vivia como enum privado dentro de src/sima_actors.c hasta la
// Tarea 7; se sube aqui porque SimaActors_WeaponHitbox (mas abajo) necesita
// que el harness in-ROM (src/phantom_test.c) pueda pasarle un facing sin
// depender de un simbolo interno del .c. Sigue teniendo 4 valores porque el
// PASO de movimiento (SimaActors_PlayerStepTarget/EnemyStepTarget) usa los
// 4 -- el jugador se mueve en las 4 direcciones de la rejilla.
//
// Lo que SI cambio (tarea de "vista de perfil pura"): la MIRADA del jugador
// -- el estado que decide que fila de sprite se dibuja y hacia donde sale el
// ataque (SimaActors_WeaponHitbox) -- ya SOLO puede ser SIMA_FACING_LEFT o
// SIMA_FACING_RIGHT. UP/DOWN se siguen usando como direccion de PASO (subir
// o bajar en la rejilla), nunca como mirada. Ver el comentario de cabecera
// de src/sima_actors.c.
enum SimaFacing
{
    SIMA_FACING_DOWN,
    SIMA_FACING_UP,
    SIMA_FACING_LEFT,
    SIMA_FACING_RIGHT,
};

// Jugador de SIMA (src/sima_actors.c): dungeon crawler POR TURNOS, no en
// tiempo real -- se mueve una casilla de la rejilla a la vez, deslizandose
// visualmente hacia ella durante unos pocos frames (ver SIMA_PLAYER_SLIDE_FRAMES
// en el .c), y solo cuando ese paso termina les toca mover a los enemigos
// (ver SimaActors_UpdateEnemies). Sin colision por caja: con movimiento por
// rejilla, SimaRoom_IsSolid sobre la casilla destino basta (la caja de 12x12
// de la version en tiempo real, SimaActors_BoxFits, se elimino con este
// cambio).
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

// Funcion pura (turnos): la casilla a la que el jugador se moveria un paso
// desde (x, y) [casillas de sala, no pixeles] mirando `facing`, y si ese
// paso es valido (TRUE) o esta bloqueado por un muro (FALSE, y outX/outY se
// quedan en la posicion de partida -- girarse sin moverse no consume turno,
// ver src/sima_actors.c). Expuesta para el harness in-ROM, igual que antes
// SimaActors_BoxFits.
bool8 SimaActors_PlayerStepTarget(u8 floor, s8 x, s8 y, u8 facing, s8 *outX, s8 *outY);

// Pura, sin sprites: ¿esta el turno del jugador completamente resuelto (de
// nuevo esperando input, con el sprite asentado en su casilla)? src/sima.c
// la usa para no comprobar la escalera a mitad de un deslizamiento.
bool8 SimaActors_IsPlayerIdle(void);

// Daño (Tarea 6). Funcion pura, sin estado: resta `amount` de `hp` saturando
// en 0 (nunca desborda hacia 255, que haria al jugador inmortal justo cuando
// deberia morir). Expuesta para el harness in-ROM, igual que SimaActors_PlayerStepTarget.
u8 SimaActors_ApplyDamage(u8 hp, u8 amount);
// Vida actual del jugador (para el HUD, src/sima.c) y si ya llego a 0.
u8 SimaActors_GetPlayerHP(void);
bool8 SimaActors_IsPlayerDead(void);

// Enemigos: rata/murcielago/slime colocados en las casillas de
// SimaRoom_GetEnemy. Por turnos igual que el jugador -- cuando su paso
// termina (SimaActors_UpdatePlayer lo señaliza), cada enemigo vivo da UN
// paso de una casilla hacia el jugador (ver SimaActors_EnemyStepTarget) y se
// desliza visualmente hacia ella. Daño por contacto: si ese paso aterriza en
// la casilla del jugador, es un golpe (no un movimiento) -- cubre tanto
// "llega a la casilla" como "ya adyacente y avanza contra el" con la misma
// regla, y empuja al jugador una casilla en direccion contraria (ver
// StartPlayerKnockback en el .c). Se inicializan una sola vez al montar el
// modo, igual que SimaActors_InitPlayer (no hay SimaActors_WarpEnemiesToFloor
// todavia: con SIMA_FLOOR_COUNT en 1 no hace falta, y anadirlo sin poder
// probarlo en un piso 2 real seria codigo sin ejercitar -- pendiente para
// cuando el piso 2 se dibuje en el editor).
void SimaActors_InitEnemies(u8 floor);
void SimaActors_UpdateEnemies(void);
// Cuantos de los enemigos colocados en el piso siguen vivos ahora mismo.
u8 SimaActors_GetAliveEnemyCount(void);

// Funcion pura (turnos): la casilla a la que un enemigo en (ex, ey) daria su
// paso hacia el jugador en (px, py) en el piso `floor`. Elige el eje que mas
// lo acerca; si esa casilla esta bloqueada por un muro prueba el otro eje;
// si los dos lo estan, se queda quieto (outX/outY == ex/ey). NO distingue
// "el destino es la casilla del jugador" (ataque) de "el destino es suelo
// libre" (movimiento real) -- quien llama decide comparando el resultado
// contra (px, py). Expuesta para el harness in-ROM, igual que
// SimaActors_PlayerStepTarget.
void SimaActors_EnemyStepTarget(u8 floor, s8 ex, s8 ey, s8 px, s8 py, s8 *outX, s8 *outY);

// NÚMERO DE GUSTO (tarea de sensación), afinable jugando: distancia Manhattan
// (en casillas) a la que un enemigo detecta al jugador y lo persigue en vez
// de deambular. Ver el comentario junto a SimaActors_EnemyShouldChase.
#define SIMA_ENEMY_DETECT_RANGE 4

// Función pura (tarea de sensación): ¿debería un enemigo a `manhattanDist`
// casillas del jugador perseguirlo (TRUE, SimaActors_EnemyStepTarget) o
// deambular (FALSE) este turno? Separada del RNG que decide HACIA DÓNDE
// deambula (ese vive sin exponer en src/sima_actors.c, EnemyWanderStep --
// no hay semilla determinista que testear ahí) para que el harness in-ROM
// pueda comprobar la frontera exacta del rango sin RNG de por medio.
// Expuesta para el harness, igual que SimaActors_EnemyStepTarget.
bool8 SimaActors_EnemyShouldChase(u8 manhattanDist);

// Pura, sin sprites (harness in-ROM, igual que SimaActors_BoxFits): dado un
// numero de enemigos vivos, ¿esta la escalera abierta? Aislada en su propia
// funcion de una linea porque es una decision de diseño que puede cambiar
// (hoy: "aparece de golpe" al morir el ultimo, no "visible pero apagada" --
// ver el comentario junto a la implementacion en src/sima_actors.c).
bool8 SimaActors_StairsUnlocked(u8 aliveEnemyCount);

// Ataque del jugador (Tarea 7). Funcion pura, sin sprites: la casilla de
// 16x16 (esquina superior izquierda, en pixeles) que el arma amenaza cuando
// el jugador -- parado en (playerX, playerY) -- ataca mirando `facing`. Es
// siempre la casilla ADYACENTE en esa direccion, nunca la propia del
// jugador: por eso un golpe no puede autolesionar y solo alcanza a un
// enemigo que este de verdad delante, no a uno que solo comparta casilla por
// detras o al lado. Expuesta para el harness in-ROM, igual que
// SimaActors_PlayerStepTarget/ApplyDamage.
void SimaActors_WeaponHitbox(u8 facing, s16 playerX, s16 playerY, s16 *outX, s16 *outY);

#endif // GUARD_SIMA_H
