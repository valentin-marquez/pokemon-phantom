#include "global.h"
#include "phantom_test.h"

#ifdef PHANTOM_TEST

#include "load_save.h"
#include "new_game.h"
#include "main.h"
#include "save.h"
#include "event_data.h"
#include "phantom.h"
#include "constants/phantom.h"
#include "constants/flags.h"
#include "start_menu.h"
#include "wild_encounter.h"
#include "sima_rooms.h"
#include "sima.h"

u8 gPhantomTestFailed = 0;

// Termina la emulación: deja exitCode en r0 y ejecuta el svc que
// mgba-rom-test observa (invocado con -S 0x27 -R r0).
void PhantomTest_Finish(u8 exitCode)
{
    asm volatile("mov r0, %0\n\t"
                 "svc 0x27\n"
                 :: "r"(exitCode) : "r0", "memory");
    while (1) {} // por si el svc no corta la ejecución
}

// Test 0: el ROM compila, arranca y el código se ejecuta.
static void Test_BootOk(void)
{
    PHANTOM_CHECKPOINT("boot");
    PHANTOM_ASSERT(TRUE, "boot-reached");
}

// NOTA (gap de cobertura): estos tests llaman NewGameInitData() directo; NO
// ejercitan el dispatch real ACTION_NEW_GAME -> CB2_NewGame (main_menu.c). El
// recorte de Birch queda verificado "por construcción", no automatizado —
// cerrar cuando haya inyección de inputs (Fase 2 del harness, libmgba-py).
// Test 1 (Task 4): el Forastero es fijo (masculino, sin nombre editable).
static void Test_NewGameProtagonist(void)
{
    // Ensucia los campos ANTES para que las aserciones fallen (RED real)
    // si NewGameInitData no fija el protagonista.
    // EOS (0xFF) no está en scope en este archivo (no se incluye
    // constants/characters.h por esta cadena de headers); se usa el literal
    // directamente, que es el terminador de string estándar del juego.
    gSaveBlock2Ptr->playerGender = FEMALE;
    gSaveBlock2Ptr->playerName[0] = 0xFF;   // nombre vacío (EOS)
    NewGameInitData();
    PHANTOM_ASSERT(gSaveBlock2Ptr->playerGender == MALE, "protagonist-male");
    PHANTOM_ASSERT(gSaveBlock2Ptr->playerName[0] != 0xFF, "protagonist-name-set");
}

// Test 2 (Task 5): el reloj narrativo arranca en PROLOGUE tras New Game.
static void Test_PhantomTimeInit(void)
{
    NewGameInitData();
    PHANTOM_ASSERT(VarGet(VAR_PHANTOM_TIME) == PHANTOM_TIME_PROLOGUE, "phantom-time-prologue");
}

// Test 3 (Task 6): el guardado es diegético; SAVE no aparece en el start menu.
static void Test_NoSaveInStartMenu(void)
{
    PHANTOM_ASSERT(PhantomTest_StartMenuHasSave() == FALSE, "no-save-in-startmenu");
}

// Test 4 (Task 7): sin mapas de isla todavía, los encuentros salvajes se
// desactivan globalmente al iniciar partida ("todo lo que te ataca te eligió").
static void Test_NoWildEncounters(void)
{
    // Ensucia el flag ANTES para que la aserción falle (RED real) si
    // NewGameInitData no lo fija: sWildEncountersDisabled es static y
    // persiste entre llamadas de NewGameInitData en la misma suite.
    DisableWildEncounters(FALSE);
    NewGameInitData();
    PHANTOM_ASSERT(PhantomTest_WildEncountersDisabled() == TRUE, "no-wild-encounters");
}

// Test 5 (Task 5): el special PhantomAdvanceDay avanza el reloj narrativo un paso.
static void Test_PhantomAdvanceDay(void)
{
    VarSet(VAR_PHANTOM_TIME, PHANTOM_TIME_PROLOGUE);
    PhantomAdvanceDay();
    PHANTOM_ASSERT(VarGet(VAR_PHANTOM_TIME) == PHANTOM_TIME_DAY1, "advance-day");
}

// Test 6 (Task 5): el special PhantomMarkExecutionSeen enciende FLAG_PHANTOM_SAW_EXECUTION.
static void Test_PhantomExecutionSeen(void)
{
    FlagClear(FLAG_PHANTOM_SAW_EXECUTION);
    PhantomMarkExecutionSeen();
    PHANTOM_ASSERT(FlagGet(FLAG_PHANTOM_SAW_EXECUTION) == TRUE, "saw-execution");
}

// Test 7 (SIMA): las salas son validas por construccion. Ya NO exige que el
// anillo de borde entero sea solido -- las salas de verdad (dibujadas en el
// editor visual, tools/sima-editor/salas.json) pueden tener un hueco en el
// borde a proposito (el piso 1 real tiene un arco de entrada en (1,0),
// sobre el borde superior, transitable). Lo que de verdad protege a quien
// dibuja la sala es: que el spawn y la escalera existan y no sean solidos,
// y que la escalera sea ALCANZABLE A PIE desde el spawn -- si no, la sala
// encierra al jugador sin salida. Se comprueba con una busqueda en anchura
// (BFS) sobre las casillas no solidas.
static void Test_SimaRoomsValid(void)
{
    u8 floor;
    bool8 allSpawnsWalkable = TRUE;
    bool8 allOneStairs = TRUE;
    bool8 allStairsWalkable = TRUE;
    bool8 allStairsReachable = TRUE;

    for (floor = 0; floor < SIMA_FLOOR_COUNT; floor++)
    {
        s8 x, y;
        s8 sx, sy;
        s8 stx = -1;
        s8 sty = -1;
        u32 stairsCount = 0;
        bool8 visited[SIMA_ROOM_H][SIMA_ROOM_W];
        s8 queueX[SIMA_ROOM_W * SIMA_ROOM_H];
        s8 queueY[SIMA_ROOM_W * SIMA_ROOM_H];
        static const s8 sDx[4] = {1, -1, 0, 0};
        static const s8 sDy[4] = {0, 0, 1, -1};
        u16 head = 0;
        u16 tail = 0;
        bool8 stairsReached = FALSE;

        SimaRoom_GetSpawn(floor, &sx, &sy);
        if (SimaRoom_IsSolid(floor, sx, sy))
            allSpawnsWalkable = FALSE;

        for (y = 0; y < SIMA_ROOM_H; y++)
        {
            for (x = 0; x < SIMA_ROOM_W; x++)
            {
                if (SimaRoom_IsStairs(floor, x, y))
                {
                    stx = x;
                    sty = y;
                    stairsCount++;
                }
                visited[y][x] = FALSE;
            }
        }

        if (stairsCount != 1)
            allOneStairs = FALSE;
        if (stx < 0 || SimaRoom_IsSolid(floor, stx, sty))
            allStairsWalkable = FALSE;

        // BFS desde el spawn: si el spawn ya es solido (fallo detectado
        // arriba) no hay nada que recorrer y stairsReached se queda en FALSE,
        // lo cual es correcto (una sala rota no puede certificarse alcanzable).
        if (!SimaRoom_IsSolid(floor, sx, sy))
        {
            visited[sy][sx] = TRUE;
            queueX[tail] = sx;
            queueY[tail] = sy;
            tail++;

            while (head < tail)
            {
                s8 cx = queueX[head];
                s8 cy = queueY[head];
                u8 dir;
                head++;

                if (cx == stx && cy == sty)
                    stairsReached = TRUE;

                for (dir = 0; dir < 4; dir++)
                {
                    s8 nx = cx + sDx[dir];
                    s8 ny = cy + sDy[dir];
                    if (nx < 0 || nx >= SIMA_ROOM_W || ny < 0 || ny >= SIMA_ROOM_H)
                        continue;
                    if (visited[ny][nx] || SimaRoom_IsSolid(floor, nx, ny))
                        continue;
                    visited[ny][nx] = TRUE;
                    queueX[tail] = nx;
                    queueY[tail] = ny;
                    tail++;
                }
            }
        }

        if (!stairsReached)
            allStairsReachable = FALSE;
    }

    PHANTOM_ASSERT(allSpawnsWalkable, "sima-spawns-walkable");
    PHANTOM_ASSERT(allOneStairs, "sima-rooms-one-stairs");
    PHANTOM_ASSERT(allStairsWalkable, "sima-stairs-walkable");
    PHANTOM_ASSERT(allStairsReachable, "sima-stairs-reachable-from-spawn");
    // Fuera de rango debe ser solido en las cuatro direcciones, o el jugador se sale de la sala.
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, -1, 5), "sima-oob-solid-left");
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, SIMA_ROOM_W, 5), "sima-oob-solid-right");
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, 5, -1), "sima-oob-solid-top");
    PHANTOM_ASSERT(SimaRoom_IsSolid(0, 5, SIMA_ROOM_H), "sima-oob-solid-bottom");
}

// Test 8 (Task 4): la caja de colision del jugador (SimaActors_BoxFits,
// src/sima_actors.c) es una funcion pura -- sin sprite ni input de por
// medio, se puede probar aqui igual que Test_SimaRoomsValid prueba
// SimaRoom_IsSolid. Cabe en la casilla de spawn de cada piso (que
// Test_SimaRoomsValid ya certifico transitable), no cabe en la esquina
// superior izquierda de la sala (solida en todos los pisos reales, aunque el
// borde ya no sea un anillo cerrado -- ver el arco de entrada del piso 1) ni
// muy fuera de sus limites.
static void Test_SimaPlayerBoxFits(void)
{
    u8 floor;
    bool8 allSpawnsFit = TRUE;
    bool8 allWallsBlock = TRUE;

    for (floor = 0; floor < SIMA_FLOOR_COUNT; floor++)
    {
        s8 sx, sy;
        SimaRoom_GetSpawn(floor, &sx, &sy);
        if (!SimaActors_BoxFits(floor, (s16)sx * 16, (s16)sy * 16))
            allSpawnsFit = FALSE;

        // (0,0) cae en el anillo de muros en las tres salas: la caja del
        // jugador no puede caber ahi.
        if (SimaActors_BoxFits(floor, 0, 0))
            allWallsBlock = FALSE;
    }

    PHANTOM_ASSERT(allSpawnsFit, "sima-player-box-fits-spawn");
    PHANTOM_ASSERT(allWallsBlock, "sima-player-box-blocked-by-wall");
    // Muy fuera de la sala: SimaRoom_IsSolid ya devuelve TRUE fuera de rango
    // (ver sima-oob-solid-* arriba), asi que la caja tampoco deberia caber.
    PHANTOM_ASSERT(!SimaActors_BoxFits(0, -100, -100), "sima-player-box-blocked-oob");

    // Casos de margen (revision de codigo, Tarea 4): los tres casos de arriba
    // caen todos dentro de una sola celda de la rejilla en las cuatro
    // esquinas de la caja, asi que no detectarian un off-by-one en la
    // aritmetica de SimaActors_BoxFits (p.ej. "right = left + COLLISION_W"
    // olvidando el "-1" -- right pasaria de 29 a 30 y ambos numeros siguen
    // cayendo en la misma celda [16,31], el test seguiria en verde).
    //
    // Se usa el piso 0 real (tools/sima-editor/salas.json), fila y=1: las
    // columnas 1-10 son suelo y la columna 11 es muro (comprobado contra
    // src/sima_rooms_data.h, sRoomSolid). Se elige el borde DERECHO de ese
    // tramo (columna 11) en vez del izquierdo porque es ahi donde se calcula
    // "right" -- la variable que el bug hipotetico de arriba corrompe; el
    // margen izquierdo usa "left", que no tiene ese "-1" y no lo detectaria.
    //
    // Constantes relevantes (src/sima_actors.c): COLLISION_W = 12,
    // COLLISION_MARGIN_X = (16 - 12) / 2 = 2. Con "x" la esquina superior
    // izquierda del sprite (parametro de SimaActors_BoxFits):
    //   left  = x + 2
    //   right = left + COLLISION_W - 1 = left + 11
    //
    // Se fija y = 16 (fila 1 completa, pixeles 16-31) para que arriba/abajo
    // de la caja nunca toquen una fila distinta y solo el eje X este en
    // juego:
    //   top    = y + 2  = 18  -> fila 18/16 = 1
    //   bottom = top + 11 = 29 -> fila 29/16 = 1 (misma fila que top)
    //
    // Muro (columna 11) empieza en el pixel 11*16 = 176. Suelo (columna 10)
    // termina en el pixel 10*16+15 = 175.
    //
    // Caso "libra por 1px" (x = 162): left = 164, right = 164+11 = 175.
    // right/16 = 10 -> suelo en las cuatro esquinas -> debe caber (TRUE).
    // Con el bug hipotetico (right = left+12 = 176), right/16 pasaria a 11
    // (muro) y este caso fallaria en falso -- exactamente el off-by-one que
    // el caso de abajo, solo, no distingue.
    PHANTOM_ASSERT(SimaActors_BoxFits(0, 162, 16), "sima-box-clear-1px");

    // Caso "solapa por 1px" (x = 163, un pixel mas cerca del muro): left =
    // 165, right = 165+11 = 176. right/16 = 11 (muro, columna 11) -> la
    // esquina superior derecha de la caja cae sobre el muro -> no debe caber
    // (FALSE).
    PHANTOM_ASSERT(!SimaActors_BoxFits(0, 163, 16), "sima-box-blocked-1px");
}

// Test 9 (Task 5): la progresion de pisos satura en el ultimo. Si desbordara,
// SimaRoom_GetTile leeria fuera de la tabla de salas. Generico sobre
// SIMA_FLOOR_COUNT (hoy 1, mientras los pisos 2/3 esten en stand-by en el
// editor) en vez de asumir un numero fijo de pisos.
static void Test_SimaFloorProgression(void)
{
    u8 floor;
    bool8 allAdvanceOrSaturate = TRUE;

    for (floor = 0; floor < SIMA_FLOOR_COUNT; floor++)
    {
        u8 next = SimaRoom_NextFloor(floor);
        u8 expected = (floor + 1 >= SIMA_FLOOR_COUNT) ? (u8)(SIMA_FLOOR_COUNT - 1) : (u8)(floor + 1);
        if (next != expected)
            allAdvanceOrSaturate = FALSE;
    }

    PHANTOM_ASSERT(allAdvanceOrSaturate, "sima-floor-progression");
    PHANTOM_ASSERT(SimaRoom_NextFloor(SIMA_FLOOR_COUNT - 1) == SIMA_FLOOR_COUNT - 1,
                   "sima-floor-saturates");
}

// Test 10 (Tarea 6): el daño satura en 0. Un underflow en u8 daria 255 de
// vida y haria al jugador inmortal justo cuando deberia morir.
static void Test_SimaDamage(void)
{
    PHANTOM_ASSERT(SimaActors_ApplyDamage(3, 1) == 2, "sima-damage-normal");
    PHANTOM_ASSERT(SimaActors_ApplyDamage(1, 1) == 0, "sima-damage-to-zero");
    PHANTOM_ASSERT(SimaActors_ApplyDamage(1, 5) == 0, "sima-damage-saturates");
    PHANTOM_ASSERT(SimaActors_ApplyDamage(0, 1) == 0, "sima-damage-from-zero");
}

// Test 11 (Tarea 6, cambio de diseño): la escalera esta cerrada mientras
// quede algun enemigo vivo y abierta solo con 0. Pura, sin sprites -- ver el
// comentario junto a SimaActors_StairsUnlocked (src/sima_actors.c) sobre por
// que esta decision de diseño vive aislada en una sola linea. Se añade
// tambien una comprobacion de la sala real: el piso 1 tiene que seguir
// teniendo sus 3 enemigos obligatorios, o esta mecanica no tendria nada que
// vigilar.
static void Test_SimaStairsUnlocked(void)
{
    PHANTOM_ASSERT(!SimaActors_StairsUnlocked(3), "sima-stairs-locked-with-enemies");
    PHANTOM_ASSERT(!SimaActors_StairsUnlocked(1), "sima-stairs-locked-with-one-enemy");
    PHANTOM_ASSERT(SimaActors_StairsUnlocked(0), "sima-stairs-unlocked-no-enemies");
    PHANTOM_ASSERT(SimaRoom_GetEnemyCount(0) == 3, "sima-floor0-three-enemies");
}

// Test 12 (Tarea 7): la caja de golpe del arma (SimaActors_WeaponHitbox,
// src/sima_actors.c) es pura -- sin sprites ni input, igual que
// SimaActors_BoxFits/ApplyDamage -- asi que el harness in-ROM la puede
// verificar sin poder pulsar A. Un jugador con su caja en (100, 50) [esquina
// superior izquierda, misma convencion que SimaActors_BoxFits] debe golpear
// la casilla ADYACENTE en cada una de las 4 direcciones (salto de 16px en el
// eje de esa direccion, nunca la propia casilla del jugador): eso es lo que
// hace que un golpe no pueda autolesionar y solo alcance a un enemigo que
// este de verdad delante.
static void Test_SimaWeaponHitbox(void)
{
    s16 x, y;

    SimaActors_WeaponHitbox(SIMA_FACING_DOWN, 100, 50, &x, &y);
    PHANTOM_ASSERT(x == 100 && y == 66, "sima-weapon-hitbox-down");

    SimaActors_WeaponHitbox(SIMA_FACING_UP, 100, 50, &x, &y);
    PHANTOM_ASSERT(x == 100 && y == 34, "sima-weapon-hitbox-up");

    SimaActors_WeaponHitbox(SIMA_FACING_LEFT, 100, 50, &x, &y);
    PHANTOM_ASSERT(x == 84 && y == 50, "sima-weapon-hitbox-left");

    SimaActors_WeaponHitbox(SIMA_FACING_RIGHT, 100, 50, &x, &y);
    PHANTOM_ASSERT(x == 116 && y == 50, "sima-weapon-hitbox-right");
}

void PhantomTest_Run(void)
{
    PHANTOM_CHECKPOINT("suite-start");
    Test_BootOk();
    // Setup compartido de estado para los tests de new game (Tasks 4/5/7).
    SetSaveBlocksPointers(GetSaveBlocksPointersBaseOffset());
    PHANTOM_CHECKPOINT("before-newgame");
    Test_NewGameProtagonist();
    PHANTOM_CHECKPOINT("after-newgame");
    Test_PhantomTimeInit();
    Test_NoSaveInStartMenu();
    Test_NoWildEncounters();
    Test_PhantomAdvanceDay();
    Test_PhantomExecutionSeen();
    Test_SimaRoomsValid();
    Test_SimaPlayerBoxFits();
    Test_SimaFloorProgression();
    Test_SimaDamage();
    Test_SimaStairsUnlocked();
    Test_SimaWeaponHitbox();
    PHANTOM_CHECKPOINT("suite-end");
    PhantomTest_Finish(gPhantomTestFailed);
}

#endif // PHANTOM_TEST
