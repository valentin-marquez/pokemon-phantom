#ifndef GUARD_SIMA_ROOMS_H
#define GUARD_SIMA_ROOMS_H

// Datos de sala y colision de SIMA (Tarea 3). Logica pura, sin dependencias
// de renderizado ni de VRAM: por eso el harness in-ROM (src/phantom_test.c),
// que no inyecta inputs, puede verificar sin jugar que las tres salas estan
// bien formadas. src/sima.c consume SimaRoom_GetTile para pintar BG0.

// Una pantalla de GBA son 240x160 px; el arte de SIMA usa celdas de 16x16 ->
// 240/16 = 15 columnas, 160/16 = 10 filas. La sala entera cabe en pantalla
// sin scroll ni camara (ver src/sima.c).
#define SIMA_ROOM_W 15
#define SIMA_ROOM_H 10

// Piso 0 (planta), piso 1, piso 2: las tres salas del crawler del prologo.
#define SIMA_FLOOR_COUNT 3

enum SimaTile
{
    SIMA_TILE_FLOOR,
    SIMA_TILE_WALL,
    SIMA_TILE_STAIRS,
};

// Devuelve el tile en (x, y) del piso dado. Fuera de [0, SIMA_ROOM_W) x
// [0, SIMA_ROOM_H) devuelve SIMA_TILE_WALL, para que el jugador nunca pueda
// salirse de la sala sin necesidad de que cada llamador comprueba el rango.
u8 SimaRoom_GetTile(u8 floor, s8 x, s8 y);

bool8 SimaRoom_IsSolid(u8 floor, s8 x, s8 y);
bool8 SimaRoom_IsStairs(u8 floor, s8 x, s8 y);

// Busca el spawn ('@') del piso y lo escribe en outX/outY.
void SimaRoom_GetSpawn(u8 floor, s8 *outX, s8 *outY);

// Piso al que baja la escalera de `floor`, saturando en el ultimo
// (SIMA_FLOOR_COUNT - 1): no hay piso mas alla de sFloors, asi que pisar la
// escalera del ultimo piso no debe desbordar la tabla (SimaRoom_GetTile la
// leeria fuera de rango).
u8 SimaRoom_NextFloor(u8 floor);

#endif // GUARD_SIMA_ROOMS_H
