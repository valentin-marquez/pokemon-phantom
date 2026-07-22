#include "global.h"
#include "sima_rooms.h"

// Las tres salas del crawler del prologo, declaradas como arte ASCII de
// 15x10 legible en el propio codigo: '#' muro, '.' suelo, '>' escalera,
// '@' spawn del jugador. ('*' reservado para spawns de enemigos en la
// Tarea 6; de momento se trata como suelo.)
//
// Las tres estan cerradas por un anillo de muros, tienen exactamente una
// escalera y su spawn cae en una celda transitable -- Test_SimaRoomsValid
// (src/phantom_test.c) lo comprueba en cada build via el harness in-ROM.

// Piso 0: planta baja.
static const char *const sFloor0[SIMA_ROOM_H] = {
    "###############",
    "#@...........*#",
    "#.####.#####..#",
    "#.#.........#.#",
    "#.#.#######.#.#",
    "#.#.......#.#.#",
    "#.#######.#.#.#",
    "#.........#..>#",
    "#.............#",
    "###############",
};

// Piso 1.
static const char *const sFloor1[SIMA_ROOM_H] = {
    "###############",
    "#.............#",
    "#.###.#.###.#.#",
    "#...#.#.#...#.#",
    "###.#.#.#.###.#",
    "#...#.#.#.....#",
    "#.###.#.#####.#",
    "#.....#.......#",
    "#@..........>.#",
    "###############",
};

// Piso 2.
static const char *const sFloor2[SIMA_ROOM_H] = {
    "###############",
    "#....#....#...#",
    "#.##.#.##.#.#.#",
    "#.............#",
    "#.###########.#",
    "#.#.........#.#",
    "#.#.#######.#.#",
    "#@#.#.....#.#>#",
    "#...#.....#...#",
    "###############",
};

static const char *const *const sFloors[SIMA_FLOOR_COUNT] = {
    sFloor0,
    sFloor1,
    sFloor2,
};

u8 SimaRoom_GetTile(u8 floor, s8 x, s8 y)
{
    char c;

    if (floor >= SIMA_FLOOR_COUNT || x < 0 || x >= SIMA_ROOM_W || y < 0 || y >= SIMA_ROOM_H)
        return SIMA_TILE_WALL;

    c = sFloors[floor][y][x];
    switch (c)
    {
    case '#':
        return SIMA_TILE_WALL;
    case '>':
        return SIMA_TILE_STAIRS;
    default: // '.', '@', '*'
        return SIMA_TILE_FLOOR;
    }
}

bool8 SimaRoom_IsSolid(u8 floor, s8 x, s8 y)
{
    return SimaRoom_GetTile(floor, x, y) == SIMA_TILE_WALL;
}

bool8 SimaRoom_IsStairs(u8 floor, s8 x, s8 y)
{
    return SimaRoom_GetTile(floor, x, y) == SIMA_TILE_STAIRS;
}

void SimaRoom_GetSpawn(u8 floor, s8 *outX, s8 *outY)
{
    s8 x, y;

    if (floor >= SIMA_FLOOR_COUNT)
    {
        *outX = 0;
        *outY = 0;
        return;
    }

    for (y = 0; y < SIMA_ROOM_H; y++)
    {
        for (x = 0; x < SIMA_ROOM_W; x++)
        {
            if (sFloors[floor][y][x] == '@')
            {
                *outX = x;
                *outY = y;
                return;
            }
        }
    }
    *outX = 0;
    *outY = 0;
}

u8 SimaRoom_NextFloor(u8 floor)
{
    if (floor + 1 >= SIMA_FLOOR_COUNT)
        return SIMA_FLOOR_COUNT - 1;
    return floor + 1;
}
