#include "global.h"
#include "sima_rooms.h"
#include "sima_rooms_data.h"

// Las salas del crawler del prologo ya no se escriben a mano como arte
// ASCII (esa version quedo rechazada por parecer un laberinto de
// programador -- ver el historial de git de este archivo): se dibujan con
// el editor visual tools/sima-editor/index.html, se exportan a
// tools/sima-editor/salas.json y de ahi graphics/sima/rooms.py genera
// src/sima_rooms_data.h (tablas de tile grafico/solido/spawn/escalera/
// enemigos por piso) y graphics/sima/tiles.png (el atlas de celdas
// compuestas que pinta src/sima.c). Para repintar el piso 1 o anadir
// contenido a los pisos 2/3: editar salas.json con el editor y correr
//     python3 graphics/sima/rooms.py
// sima_rooms_data.h se pisa entero en cada ejecucion -- no editarlo a mano.

static u8 TileIndexOf(s8 x, s8 y)
{
    return (u8)(y * SIMA_ROOM_W + x);
}

static bool8 InRange(u8 floor, s8 x, s8 y)
{
    return floor < SIMA_FLOOR_COUNT && x >= 0 && x < SIMA_ROOM_W && y >= 0 && y < SIMA_ROOM_H;
}

u8 SimaRoom_GetTile(u8 floor, s8 x, s8 y)
{
    if (!InRange(floor, x, y))
        return SIMA_TILE_WALL;

    if (x == sRoomStairs[floor][0] && y == sRoomStairs[floor][1])
        return SIMA_TILE_STAIRS;
    if (sRoomSolid[floor][TileIndexOf(x, y)])
        return SIMA_TILE_WALL;
    return SIMA_TILE_FLOOR;
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
    if (floor >= SIMA_FLOOR_COUNT)
    {
        *outX = 0;
        *outY = 0;
        return;
    }

    *outX = sRoomSpawn[floor][0];
    *outY = sRoomSpawn[floor][1];
}

u8 SimaRoom_NextFloor(u8 floor)
{
    if (floor + 1 >= SIMA_FLOOR_COUNT)
        return SIMA_FLOOR_COUNT - 1;
    return floor + 1;
}

u16 SimaRoom_GetTileGfx(u8 floor, s8 x, s8 y)
{
    if (!InRange(floor, x, y))
        return 0;

    return sRoomTileGfx[floor][TileIndexOf(x, y)];
}

u8 SimaRoom_GetEnemyCount(u8 floor)
{
    if (floor >= SIMA_FLOOR_COUNT)
        return 0;

    return sRoomEnemyCount[floor];
}

void SimaRoom_GetEnemy(u8 floor, u8 index, s8 *outX, s8 *outY)
{
    if (floor >= SIMA_FLOOR_COUNT || index >= sRoomEnemyCount[floor])
    {
        *outX = 0;
        *outY = 0;
        return;
    }

    *outX = sRoomEnemies[floor][index][0];
    *outY = sRoomEnemies[floor][index][1];
}

u16 SimaRoom_GetSheetTilesWide(void)
{
    return SIMA_ROOMS_TILE_COUNT * 2;
}
