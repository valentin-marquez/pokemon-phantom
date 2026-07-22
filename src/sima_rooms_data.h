// GENERADO por graphics/sima/rooms.py -- NO EDITAR A MANO.
// Fuente: tools/sima-editor/salas.json (formato sima-rooms/2), pintado
// con el editor visual tools/sima-editor/index.html.
// Para regenerar (tambien tras anadir contenido a los pisos 2/3):
//     python3 graphics/sima/rooms.py

#ifndef GUARD_SIMA_ROOMS_DATA_H
#define GUARD_SIMA_ROOMS_DATA_H

// Numero de celdas compuestas distintas en graphics/sima/tiles.png (una
// fila de SIMA_ROOMS_TILE_COUNT celdas de 16x16 = SIMA_ROOMS_TILE_COUNT*2
// tiles de hardware de ancho). src/sima.c la usa como sheetTilesWide.
#define SIMA_ROOMS_TILE_COUNT 38

#define SIMA_ROOM_MAX_ENEMIES 3

// Indice de la celda compuesta (graphics/sima/tiles.png) por casilla,
// en orden raster (fila a fila). SimaRoom_GetTileGfx lo expone a src/sima.c.
static const u16 sRoomTileGfx[SIMA_FLOOR_COUNT][SIMA_ROOM_W * SIMA_ROOM_H] = {
    {
        0, 1, 2, 2, 2, 2, 3, 2, 3, 2, 2, 2, 2, 2, 4,
        5, 6, 7, 8, 9, 9, 9, 9, 9, 9, 9, 10, 8, 11, 12,
        5, 13, 14, 14, 14, 15, 16, 17, 18, 18, 14, 14, 14, 19, 12,
        5, 13, 14, 14, 14, 18, 20, 20, 20, 21, 14, 14, 14, 22, 12,
        23, 13, 14, 14, 14, 18, 20, 20, 20, 21, 14, 14, 14, 24, 12,
        5, 13, 14, 14, 14, 18, 20, 20, 20, 21, 14, 14, 14, 25, 26,
        5, 27, 14, 14, 14, 14, 28, 28, 28, 14, 14, 14, 14, 25, 12,
        5, 29, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 25, 12,
        5, 30, 31, 32, 33, 33, 33, 33, 33, 33, 33, 33, 33, 34, 12,
        35, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 37,
    },
};

// TRUE si la casilla bloquea el paso, en orden raster.
static const bool8 sRoomSolid[SIMA_FLOOR_COUNT][SIMA_ROOM_W * SIMA_ROOM_H] = {
    {
        TRUE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
        TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE,
        TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE,
        TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE,
        TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, TRUE,
        TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE,
        TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,
        FALSE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,
        FALSE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,
        TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE, TRUE,
    },
};

// Casilla de spawn del jugador, {x, y}, por piso.
static const s8 sRoomSpawn[SIMA_FLOOR_COUNT][2] = {
    {1, 0},
};

// Casilla de la escalera, {x, y}, por piso.
static const s8 sRoomStairs[SIMA_FLOOR_COUNT][2] = {
    {13, 8},
};

// Numero de enemigos por piso (<= SIMA_ROOM_MAX_ENEMIES).
static const u8 sRoomEnemyCount[SIMA_FLOOR_COUNT] = {
    3,
};

// Casillas de spawn de enemigo, {x, y}, por piso. Las entradas sobrantes
// (por encima de sRoomEnemyCount[piso]) quedan a {0, 0} y no se leen.
static const s8 sRoomEnemies[SIMA_FLOOR_COUNT][SIMA_ROOM_MAX_ENEMIES][2] = {
    {
        {11, 6},
        {3, 6},
        {11, 2},
    },
};

#endif // GUARD_SIMA_ROOMS_DATA_H
