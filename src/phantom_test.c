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

// Test 8 (turnos): SimaActors_PlayerStepTarget (src/sima_actors.c) es la
// funcion pura que decide si un paso de rejilla es valido -- sustituye a
// SimaActors_BoxFits (eliminada con el cambio a movimiento por turnos: con
// una casilla, no una caja de 12x12, la colision es solo SimaRoom_IsSolid
// sobre el destino). ELIMINADOS de este archivo con esa funcion: los 5
// PHANTOM_ASSERT de Test_SimaPlayerBoxFits (sima-player-box-fits-spawn,
// sima-player-box-blocked-by-wall, sima-player-box-blocked-oob,
// sima-box-clear-1px, sima-box-blocked-1px) -- no solo los dos ultimos (los
// pensados para el off-by-one de la caja de 12x12): la funcion que probaban
// ya no existe, asi que ninguno de los 5 podia seguir compilando. Lo que
// verificaban sigue cubierto: "spawn transitable"/"fuera de rango es solido"
// ya los prueba Test_SimaRoomsValid (sima-spawns-walkable,
// sima-oob-solid-*); "un muro bloquea" y el caso de margen de 1px los prueban
// los casos de abajo, ahora en casillas en vez de en pixeles de una caja.
//
// Casillas reales del piso 0 (tools/sima-editor/salas.json, ver
// src/sima_rooms_data.h/sRoomSolid): fila y=2, columnas 1-4 suelo, columna 5
// muro (separacion exacta, sin caja de por medio -- no hace falta un "margen
// de 1px" artificial: la casilla vecina YA es la prueba de margen).
static void Test_SimaPlayerStepTarget(void)
{
    s8 x, y;

    // Paso normal: spawn (1,0) mirando abajo cae en suelo (1,1).
    PHANTOM_ASSERT(SimaActors_PlayerStepTarget(0, 1, 0, SIMA_FACING_DOWN, &x, &y)
                       && x == 1 && y == 1,
                   "sima-step-player-open");

    // Bloqueado por el borde de la sala (fuera de rango, SimaRoom_IsSolid
    // devuelve solido): desde el spawn mirando arriba no hay paso, y la
    // casilla de salida NO cambia (se queda en el spawn).
    PHANTOM_ASSERT(!SimaActors_PlayerStepTarget(0, 1, 0, SIMA_FACING_UP, &x, &y)
                       && x == 1 && y == 0,
                   "sima-step-player-blocked-oob");

    // Bloqueado por un muro real (no de borde): (4,2) mirando a la derecha
    // pega contra (5,2), muro. Tampoco cambia de casilla.
    PHANTOM_ASSERT(!SimaActors_PlayerStepTarget(0, 4, 2, SIMA_FACING_RIGHT, &x, &y)
                       && x == 4 && y == 2,
                   "sima-step-player-blocked-wall");

    // La casilla vecina en la direccion contraria SI es transitable: prueba
    // de margen sin caja -- confirma que el bloqueo de arriba es del muro
    // real (columna 5), no de un desplazamiento por error en otro eje.
    PHANTOM_ASSERT(SimaActors_PlayerStepTarget(0, 4, 2, SIMA_FACING_LEFT, &x, &y)
                       && x == 3 && y == 2,
                   "sima-step-player-clear-neighbor");
}

// Test 9 (turnos): SimaActors_EnemyStepTarget (src/sima_actors.c) es la
// funcion pura que decide el paso de un enemigo hacia el jugador -- elige el
// eje que mas lo acerca, prueba el otro si el primero esta bloqueado, y se
// queda quieto si los dos lo estan. Los cuatro casos usan casillas reales
// del piso 0 (src/sima_rooms_data.h/sRoomSolid), verificadas a mano contra
// esa tabla:
//   fila y=6: (3,6) y (11,6) son las casillas reales de dos enemigos (ver
//     tools/sima-editor/salas.json) -- sin muros entre medias en esa fila.
//   fila y=2, columna 5-9: muro: (6,2) y (8,2) son muro.
//   fila y=3, columnas 5 y 9: muro; columnas 6-8: suelo.
//   fila y=1, columnas 1-10: suelo (pasillo superior, sin muros).
static void Test_SimaEnemyStepTarget(void)
{
    s8 x, y;

    // Eje X domina (mismo y=6) y esta libre: se mueve una casilla hacia el
    // jugador por X. Mismas casillas que los enemigos reales del piso 1.
    SimaActors_EnemyStepTarget(0, 3, 6, 11, 6, &x, &y);
    PHANTOM_ASSERT(x == 4 && y == 6, "sima-step-enemy-primary-axis");

    // Eje X domina (adx=3 > ady=1) pero (5,3) es muro: cae al eje Y,
    // (4,4) esta libre.
    SimaActors_EnemyStepTarget(0, 4, 3, 7, 4, &x, &y);
    PHANTOM_ASSERT(x == 4 && y == 4, "sima-step-enemy-fallback-axis");

    // Empate (adx=ady=2): primero intenta Y (arriba, prioridad vertical),
    // (6,2) es muro -> cae a X, (7,3) esta libre.
    SimaActors_EnemyStepTarget(0, 6, 3, 8, 1, &x, &y);
    PHANTOM_ASSERT(x == 7 && y == 3, "sima-step-enemy-tie-fallback");

    // Ambos ejes bloqueados: (8,2) y (9,3) son muro -> se queda quieto
    // (misma casilla de entrada). Esto es "una casilla bloqueada no
    // consume turno" para un enemigo: StartEnemyTurn (src/sima_actors.c)
    // ve nx==ex && ny==ey y no arranca ningun deslizamiento para el.
    SimaActors_EnemyStepTarget(0, 8, 3, 9, 1, &x, &y);
    PHANTOM_ASSERT(x == 8 && y == 3, "sima-step-enemy-both-blocked");

    // Adyacente: el "paso" aterriza exactamente en la casilla del jugador
    // -- la misma regla cubre "llega a la casilla" y "ya adyacente avanza
    // contra el" (ver el comentario de SimaActors_EnemyStepTarget).
    SimaActors_EnemyStepTarget(0, 5, 1, 6, 1, &x, &y);
    PHANTOM_ASSERT(x == 6 && y == 1, "sima-step-enemy-reaches-player");
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
    Test_SimaPlayerStepTarget();
    Test_SimaEnemyStepTarget();
    Test_SimaFloorProgression();
    Test_SimaDamage();
    Test_SimaStairsUnlocked();
    Test_SimaWeaponHitbox();
    PHANTOM_CHECKPOINT("suite-end");
    PhantomTest_Finish(gPhantomTestFailed);
}

#endif // PHANTOM_TEST
