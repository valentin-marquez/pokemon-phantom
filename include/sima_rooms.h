#ifndef GUARD_SIMA_ROOMS_H
#define GUARD_SIMA_ROOMS_H

// Datos de sala y colision de SIMA. Logica pura, sin dependencias de
// renderizado ni de VRAM: por eso el harness in-ROM (src/phantom_test.c),
// que no inyecta inputs, puede verificar sin jugar que las salas estan bien
// formadas. src/sima.c consume SimaRoom_GetTile para la colision y
// SimaRoom_GetTileGfx para pintar BG0.
//
// Las salas se disenan con el editor visual (tools/sima-editor/index.html),
// se exportan a tools/sima-editor/salas.json y de ahi se generan
// src/sima_rooms_data.h y graphics/sima/tiles.png con
// `python3 graphics/sima/rooms.py` (ver ese script para el detalle del
// importador). src/sima_rooms.c consume esos datos generados.

// Una pantalla de GBA son 240x160 px; el arte de SIMA usa celdas de 16x16 ->
// 240/16 = 15 columnas, 160/16 = 10 filas. La sala entera cabe en pantalla
// sin scroll ni camara (ver src/sima.c).
#define SIMA_ROOM_W 15
#define SIMA_ROOM_H 10

// Numero de pisos CON CONTENIDO de salas.json (spawn != null). El JSON del
// editor reserva tres pisos, pero solo los que ya estan dibujados entran en
// la ROM -- graphics/sima/rooms.py reescribe este numero automaticamente al
// regenerar, no hace falta tocarlo a mano.
#define SIMA_FLOOR_COUNT 1

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

// Busca el spawn del piso y lo escribe en outX/outY.
void SimaRoom_GetSpawn(u8 floor, s8 *outX, s8 *outY);

// Piso al que baja la escalera de `floor`, saturando en el ultimo
// (SIMA_FLOOR_COUNT - 1): no hay piso mas alla de los datos generados, asi
// que pisar la escalera del ultimo piso no debe desbordar la tabla
// (SimaRoom_GetTile la leeria fuera de rango).
u8 SimaRoom_NextFloor(u8 floor);

// Indice de la celda compuesta en graphics/sima/tiles.png que corresponde a
// (x, y) del piso dado -- lo que src/sima.c pinta en pantalla. Distinto de
// SimaRoom_GetTile: ese es solo FLOOR/WALL/STAIRS para colision, este es el
// tile grafico real (suelo con caja encima, muro con cenefa, etc). Fuera de
// rango devuelve 0 (la primera celda del atlas, siempre existe).
u16 SimaRoom_GetTileGfx(u8 floor, s8 x, s8 y);

// Numero de enemigos colocados en el piso (spawns fijos del editor, Tarea 6).
u8 SimaRoom_GetEnemyCount(u8 floor);

// Casilla del enemigo `index` (< SimaRoom_GetEnemyCount(floor)) del piso.
// Fuera de rango escribe {0, 0}.
void SimaRoom_GetEnemy(u8 floor, u8 index, s8 *outX, s8 *outY);

// Casilla de la escalera del piso (siempre existe: graphics/sima/rooms.py
// aborta si un piso tiene spawn pero no escalera). Se expone ademas de
// SimaRoom_IsStairs porque la Tarea 6 necesita la coordenada exacta para
// repintar esa unica celda cuando la escalera aparece (ver src/sima.c).
void SimaRoom_GetStairs(u8 floor, s8 *outX, s8 *outY);

// Celda grafica que debe pintarse EN LUGAR de la escalera mientras sigue
// cerrada (Tarea 6: la escalera no aparece hasta que caen todos los
// enemigos del piso). El atlas no tiene una celda "solo suelo" para la
// posicion exacta de la escalera -- graphics/sima/rooms.py compone base+prop
// en un unico tile compuesto -- asi que se usa el gfx de una casilla de
// suelo vecina (mismo suelo base, sin el prop de la escalera encima). Si
// ninguna vecina es suelo transitable, cae al suelo del spawn (que
// Test_SimaRoomsValid ya certifica transitable) como red de seguridad.
u16 SimaRoom_GetHiddenStairsGfx(u8 floor);

// Ancho, en tiles de hardware de 8x8, de la hoja graphics/sima/tiles.png
// (el atlas de celdas compuestas). src/sima.c la necesita como
// `sheetTilesWide` de PlaceCell; vive aqui y no como macro publica porque
// depende del numero de celdas compuestas, que solo conoce el importador
// (graphics/sima/rooms.py -> src/sima_rooms_data.h).
u16 SimaRoom_GetSheetTilesWide(void);

#endif // GUARD_SIMA_ROOMS_H
