#include "global.h"
#include "sima.h"
#include "sima_rooms.h"
#include "sprite.h"
#include "decompress.h"
#include "main.h"
#include "random.h"
#include "sound.h"
#include "constants/songs.h"

// Jugador de SIMA: dungeon crawler POR TURNOS (encargo del dueño del
// proyecto, sustituye a la version en tiempo real de la Tarea 4). La regla
// es simple y sin excepciones: pulsas una direccion, el jugador se desliza
// UNA casilla, y SOLO CUANDO ese deslizamiento termina les toca mover a los
// enemigos -- uno por uno hacia el jugador, tambien deslizandose. Sin
// deslizamiento en marcha (ni del jugador ni de los enemigos), nada se
// mueve: el jugador puede pensar indefinidamente entre turnos. Atacar
// (Tarea 7) tambien consume turno: golpeas, y despues de que termine la
// animacion del golpe (windup+activo+recuperacion) les toca a los enemigos.
//
// Tarea de sensacion (esta): tres cambios sobre lo anterior, todos pedidos
// por el dueño tras jugarlo.
//   1. Vista de perfil pura: el sprite del jugador SOLO usa la fila de
//      perfil de player.png (ver sPlayerFacing, mas abajo) -- nunca mira
//      arriba/abajo, aunque SIGUE pudiendo MOVERSE en las 4 direcciones de
//      la rejilla. sPlayerFacing (para sprite y ataque) y la direccion de UN
//      PASO (para SimaActors_PlayerStepTarget) son cosas distintas desde
//      ahora: la primera solo vive en {LEFT, RIGHT}, la segunda sigue
//      aceptando las 4.
//   2. Tap-to-turn en el eje horizontal: pulsar IZQUIERDA/DERECHA cuando el
//      jugador NO mira ya hacia ahi solo lo GIRA (gratis, sin turno); si ya
//      miraba hacia ahi, pulsar otra vez SI mueve (consume turno). Pulsar
//      ARRIBA/ABAJO mueve de inmediato (consume turno) conservando la
//      mirada izquierda/derecha actual -- no hay mirada vertical, asi que
//      ahi no hay nada que girar. Ver UpdatePlayerInput.
//   3. El golpe solo sale a izquierda/derecha (SimaActors_WeaponHitbox ya no
//      soporta arriba/abajo -- ver su comentario). Para golpear a un enemigo
//      que esta arriba o abajo, el jugador se coloca a su lado: intencional.
//
// Maquina de estados del turno (enum SimaTurnPhase, mas abajo):
//   PLAYER_INPUT -> (direccion valida) -> PLAYER_MOVE -> ENEMY_STEP -> PLAYER_INPUT
//   PLAYER_INPUT -> (A)                -> PLAYER_ATTACK -> ENEMY_STEP -> PLAYER_INPUT
//   PLAYER_INPUT -> (girar L/R, o direccion bloqueada) -> sigue en PLAYER_INPUT (SIN turno)
// src/sima.c llama a SimaActors_UpdatePlayer y SimaActors_UpdateEnemies cada
// frame desde CB2_SimaMain; cual de las dos "hace algo" en un frame dado lo
// decide sTurnPhase, compartida por ambas porque viven en el mismo archivo.
//
// Colision: con movimiento por rejilla, SimaRoom_IsSolid sobre la casilla
// destino basta -- ya no hace falta la caja de 12x12 de la version en
// tiempo real (SimaActors_BoxFits/BoxesOverlap, eliminadas con esta tarea;
// sus tests tambien, ver el informe).
//
// Enemigos (esta tarea): rango de deteccion (SIMA_ENEMY_DETECT_RANGE) en vez
// de persecucion optima siempre -- dentro del rango persiguen (igual que
// antes), fuera de el deambulan un paso aleatorio. Sigue siendo POR TURNOS:
// nada de esto corre fuera del turno de los enemigos (StartEnemyTurn, mas
// abajo). Ver el comentario junto a SimaActors_EnemyShouldChase.
//
// Tarea de animacion (esta): el dueño del proyecto separo y nombro los
// frames del personaje (antes se recortaban a ojo de un elf.png grande --
// ver el historial de graphics/sima/gen.py). Cinco estados nuevos, cableados
// aqui:
//   - idle (3 frames): en reposo, sin deslizarse. Ciclo lento (ver
//     SIMA_IDLE_ANIM_PERIOD), corre siempre que SIMA_TURN_PLAYER_INPUT esta
//     activo y el jugador no se esta desplazando.
//   - move (4 frames): durante el deslizamiento de una casilla
//     (SIMA_TURN_PLAYER_MOVE). SUSTITUYE al ciclo de 2 frames que habia antes
//     (STEP_A/STEP_C de player_walk.png) -- ahora se ven los 4 frames reales
//     de caminar, uno cada SIMA_MOVE_ANIM_PERIOD frames de juego (2), que
//     encaja EXACTO en los SIMA_PLAYER_SLIDE_FRAMES (8) de un paso: 4
//     frames * 2 = 8.
//   - damage (5 frames): al recibir un golpe. SUSTITUYE al parpadeo
//     programado (.invisible on/off) que habia antes -- ver el comentario
//     grande junto a sPlayerInvulnTimer, mas abajo, sobre por que el
//     temporizador se queda pero cambia de trabajo.
//   - dead (3 frames): al llegar la vida a 0. Ver SIMA_TURN_PLAYER_DEAD y el
//     GANCHO aislado en SimaActors_ResetAfterDeath.
//   - teleport (6 frames, "se encoge y desaparece"): al bajar la escalera,
//     ANTES del fundido a negro de cambio de piso. Ver SIMA_TURN_PLAYER_TELEPORT
//     y SimaActors_StartTeleport.
//
// Tarea de sensación (esta, sobre lo anterior): "mantener para caminar" en
// las 4 direcciones -- el dueño del proyecto reportó que caminar de lado se
// sentía rígido porque el tap-to-turn (punto 2 de arriba) exigía SOLTAR y
// volver a pulsar por cada casilla en el eje horizontal, mientras que
// arriba/abajo sí se podían mantener. Arreglado en UpdatePlayerInput con un
// margen de giro (SIMA_TURN_GRACE_FRAMES, sima.h): pulsar hacia el lado
// contrario SIGUE girando gratis al instante, pero si el botón sigue
// pulsado pasado el margen, arranca a caminar y encadena mientras se
// mantenga -- toque corto = solo apuntar, mantener = caminar, en las 4
// direcciones. Ver el comentario grande junto a UpdatePlayerInput.
//
// Los 21 frames (3+4+5+3+6) viven en UNA sola hoja, player_anim.png
// (graphics/sima/gen.py, generate_player_anim), en ese mismo orden --
// FRAME_*_BASE mas abajo depende de ese orden exacto, no lo cambies sin
// actualizar gen.py tambien. Igual que antes con player_walk.png: la hoja
// SOLO trae los frames mirando a la DERECHA (vista de perfil pura, ver mas
// abajo) -- la izquierda es h-flip de OAM (oam.hFlip mas abajo), no hay arte
// de perfil-izquierda por separado (verificado en la Tarea de animacion:
// las hojas "-left" de origen son el espejo EXACTO frame a frame de las
// "-right", 0 pixeles de diferencia). graphics_file_rules.mk convierte esta
// hoja con -mwidth 2 -mheight 2, así que cada celda de 16x16 ocupa 4 tiles
// de hardware CONTIGUOS (a diferencia de tiles.4bpp, que es BG y usa el
// barrido raster de la hoja completa vía PlaceCell en src/sima.c): "celda i"
// empieza en el tile 4*i de la hoja.
//
// Presupuesto de VRAM: 21 frames * 4 tiles/frame = 84 tiles de OBJ, de los
// 1024 disponibles -- holgado (ver el brief de esta tarea).
static const u32 sPlayerGfx[] = INCBIN_U32("graphics/sima/player_anim.4bpp");
// Misma paleta única de SIMA que las celdas de sala (índice 0 transparente +
// 4 tonos; ver src/sima.c). Se vuelve a incluir aquí (en vez de compartir el
// array de sima.c) para que este archivo no dependa de símbolos internos de
// sima.c -- son los mismos 32 bytes, duplicarlos en el ROM es irrelevante.
static const u16 sPlayerPal[] = INCBIN_U16("graphics/sima/grounds.gbapal");

#define TAG_SIMA_PLAYER 0x6000

#define PLAYER_SHEET_FRAMES     21  // 3 idle + 4 move + 5 damage + 3 dead + 6 teleport
#define PLAYER_TILES_PER_FRAME  4   // 16x16 = 2x2 tiles de hardware de 8x8

// Offsets de tile (en tiles de 4bpp, no en celdas) de cada grupo de frames
// dentro de la hoja -- el orden EXACTO en que graphics/sima/gen.py
// (PLAYER_ANIM_SOURCES) los concatena en player_anim.png. Un frame concreto
// del grupo es BASE + paso*PLAYER_TILES_PER_FRAME, con paso en
// [0, *_FRAME_COUNT).
#define FRAME_IDLE_BASE     (0  * PLAYER_TILES_PER_FRAME)
#define FRAME_MOVE_BASE     (3  * PLAYER_TILES_PER_FRAME)
#define FRAME_DAMAGE_BASE   (7  * PLAYER_TILES_PER_FRAME)
#define FRAME_DEAD_BASE     (12 * PLAYER_TILES_PER_FRAME)
#define FRAME_TELEPORT_BASE (15 * PLAYER_TILES_PER_FRAME)

#define IDLE_FRAME_COUNT     3
#define MOVE_FRAME_COUNT     4
#define DAMAGE_FRAME_COUNT   5
#define DEAD_FRAME_COUNT     3
#define TELEPORT_FRAME_COUNT 6

// enum SimaFacing vive en include/sima.h desde la Tarea 7 (lo necesita
// SimaActors_WeaponHitbox, expuesta al harness). Sigue teniendo 4 valores
// porque el PASO de movimiento (SimaActors_PlayerStepTarget) los sigue
// usando los 4 -- lo que cambia con la vista de perfil es que sPlayerFacing
// (mas abajo, la mirada del sprite/el ataque) ya SOLO se le asignan
// SIMA_FACING_LEFT/SIMA_FACING_RIGHT, nunca UP/DOWN.

static const struct OamData sOam_SimaPlayer = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 1,  // igual que BG0 (la sala): por delante de su fondo, por detrás del HUD (BG1, prioridad 0)
};

static const struct SpriteSheet sSheet_SimaPlayer = {
    sPlayerGfx, PLAYER_SHEET_FRAMES * PLAYER_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_PLAYER
};
static const struct SpritePalette sPal_SimaPlayer = { sPlayerPal, TAG_SIMA_PLAYER };

// anims/images sin usar a propósito (igual que las grietas y el menú de
// phantom_intro.c): el frame se elige a mano cada tick en
// UpdatePlayerSprite escribiendo oam.tileNum directamente sobre
// sheetTileStart, no hace falta el sistema de ANIMCMD para esto.
static const struct SpriteTemplate sTmpl_SimaPlayer = {
    .tileTag = TAG_SIMA_PLAYER,
    .paletteTag = TAG_SIMA_PLAYER,
    .oam = &sOam_SimaPlayer,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// ---------------------------------------------------------------------
// Arma del jugador: una ESPADA de 16x32, 5 frames dibujados a mano por el
// dueño (weapon_left_0..4.png) y apilados en weapon.4bpp por graphics/sima/
// gen.py (generate_weapon). Reemplaza al ARCO DE TAJO de 16x16 que hubo antes
// (ver git log de este bloque / de gen.py). El arco existió para desambiguar
// 4 direcciones cardinales -- una espada diagonal servía igual para ABAJO que
// para DERECHA -- pero con la vista de perfil pura sAttackFacing ya SOLO puede
// ser IZQUIERDA/DERECHA (viene de sPlayerFacing, ver la cabecera del archivo),
// y una espada en horizontal se lee sin esa ambigüedad. El arma principal del
// prota ES una espada, así que se vuelve a ella ahora que se puede.
//
// GEOMETRÍA (el detalle que importa, REVISADA -- el dueño rechazó el primer
// intento): el sprite es 16x32 -- el DOBLE de alto que el jugador (16x16) --
// porque los frames describen un mandoble descendente en un canvas alto: la
// mitad SUPERIOR es la espada alzada, la INFERIOR el reposo. El primer
// intento la anclaba SOBRE el jugador (centro en (sPlayerX+8, sPlayerY)),
// tapando su sprite -- rechazado en la práctica, se veía mal. Ahora se ancla
// en la CASILLA ADYACENTE en la dirección de mirada -- la misma casilla que
// SimaActors_WeaponHitbox ya usaba para el daño, pegada al jugador pero SIN
// solaparlo: top-left del arma = (hitX, hitY - 16), centro para CreateSprite
// = (hitX + 8, hitY), con (hitX, hitY) el resultado de
// SimaActors_WeaponHitbox(sAttackFacing, sPlayerX, sPlayerY, ...). Como esa
// función solo desplaza en X (vista de perfil pura: la casilla objetivo
// siempre está en la misma FILA que el jugador, hitY == sPlayerY), el
// anclaje vertical queda igual que en el primer intento -- BASE del arma
// alineada con la base de la casilla golpeada (sus 16px inferiores ocupan
// esa casilla, los 16px superiores quedan en la fila de arriba, espada
// alzada) -- NÚMERO DE GUSTO: probado contra el piso 1 real (fila y=6, sin
// muro en la fila de arriba en las columnas donde golpean los enemigos) y no
// tapa ninguna fila de muro de forma fea; si un piso futuro pone muro justo
// encima de una casilla golpeable, revisar este offset. SimaActors_WeaponHitbox
// en sí NO cambió -- solo se reutiliza también para POSICIONAR el sprite, no
// solo para el daño.
//
// FRAMES (secuencia del mandoble, ver UpdateAttack): 0 = alzada (windup,
// telégrafo), 1/2/3 = arco de tajo barriendo hacia abajo (ventana de impacto),
// 4 = reposo / follow-through. Se reparten sobre la cadencia existente del
// golpe (windup / activo / recuperación).
//
// DIRECCIÓN: el arte viene MIRANDO A LA IZQUIERDA; para DERECHA se voltea con
// UN flip de OAM (HFLIP en UpdateAttack), misma economía de VRAM que
// player_anim (que solo carga "-right" y voltea para la izquierda). Por ahora
// solo existe el arte de izquierda; el "de verdad" de derecha se dibujará
// después, pero el HFLIP evita que el juego se rompa al atacar a la derecha.
// ---------------------------------------------------------------------

static const u32 sWeaponGfx[] = INCBIN_U32("graphics/sima/weapon.4bpp");

#define TAG_SIMA_WEAPON 0x6004

#define WEAPON_SHEET_FRAMES    5
#define WEAPON_TILES_PER_FRAME 8  // 16x32 = 2x4 tiles de hardware (el doble de alto que jugador/enemigos)
// Orden de graphics/sima/gen.py (generate_weapon): los 5 frames del mandoble
// descendente, de alzada a reposo. La dirección (izquierda vs derecha) NO vive
// aquí -- es un flip de OAM elegido en UpdateAttack según sAttackFacing.
#define FRAME_WEAPON_RAISED (0 * WEAPON_TILES_PER_FRAME)  // espada alzada (windup)
#define FRAME_WEAPON_ARC1   (1 * WEAPON_TILES_PER_FRAME)  // arco de tajo, inicio (impacto)
#define FRAME_WEAPON_ARC2   (2 * WEAPON_TILES_PER_FRAME)  // arco de tajo, medio  (impacto)
#define FRAME_WEAPON_ARC3   (3 * WEAPON_TILES_PER_FRAME)  // arco de tajo, fin    (impacto)
#define FRAME_WEAPON_REST   (4 * WEAPON_TILES_PER_FRAME)  // reposo / follow-through

static const struct OamData sOam_SimaWeapon = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x32),
    .size = SPRITE_SIZE(16x32),
    .priority = 1,  // misma capa que jugador/enemigos
};

static const struct SpriteSheet sSheet_SimaWeapon = {
    sWeaponGfx, WEAPON_SHEET_FRAMES * WEAPON_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_WEAPON
};

// paletteTag = TAG_SIMA_PLAYER a propósito, igual que los enemigos: misma
// paleta única de sprites de SIMA para todo (los 5 frames de la espada usan
// exactos los 4 tonos de SIMA -- verificado; gen.py aborta si no fuera así).
static const struct SpriteTemplate sTmpl_SimaWeapon = {
    .tileTag = TAG_SIMA_WEAPON,
    .paletteTag = TAG_SIMA_PLAYER,
    .oam = &sOam_SimaWeapon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// Cadencia del golpe (Tarea 7): windup -> activo -> recuperación, sumando
// ATTACK_TOTAL_FRAMES antes de que le toque el turno a los enemigos (ver
// StartEnemyTurn, más abajo -- con el cambio a turnos, ya no hay "cooldown"
// que gestionar: mientras sAttackTimer > 0 el jugador está en
// SIMA_TURN_PLAYER_ATTACK y no puede volver a pulsar A hasta que ese turno
// termine y vuelva a SIMA_TURN_PLAYER_INPUT). Números elegidos para que el
// golpe se LEA (el arma es visible unos frames antes de dañar, un telégrafo
// mínimo). Mismo espíritu que el resto de tiempos de esta tarea: "paso
// deliberado, no arcade".
#define ATTACK_WINDUP_FRAMES   3  // arma visible (FRAME_A), sin dañar todavía
#define ATTACK_ACTIVE_FRAMES   4  // arma visible (FRAME_B), caja de golpe activa
#define ATTACK_RECOVERY_FRAMES 9  // arma oculta, hasta que le toca el turno a los enemigos
#define ATTACK_TOTAL_FRAMES (ATTACK_WINDUP_FRAMES + ATTACK_ACTIVE_FRAMES + ATTACK_RECOVERY_FRAMES)

// ATTACK_SWEEP_PX (el desplazamiento cosmético perpendicular que tenía el
// arma mientras fue una espada diagonal) se quitó en la tarea de sensación
// del golpe: existía para simular que el mandoble "venía de un lado" porque
// su dibujo por sí solo no comunicaba dirección. El arco de tajo (ver el
// comentario grande sobre WEAPON_SHEET_FRAMES) ya resuelve eso con su propia
// silueta -- añadirle un bamboleo de píxeles solo restaría precisión al
// mensaje nuevo ("el corte está exactamente sobre esta casilla, quieto").

// Tamaño de una casilla de la rejilla de sala, en píxeles (ver SIMA_ROOM_W/H
// en sima_rooms.h). No vive ahí porque es un detalle de cómo se ANIMA el
// movimiento (deslizamiento en píxeles), no de la geometría de la sala.
#define SIMA_TILE_PX 16

// NÚMEROS DE GUSTO -- LOS TIEMPOS DEL TURNO (ajustables jugando, ver el
// informe de esta tarea). Antes (tiempo real) el jugador cruzaba una casilla
// en 8 frames a 2px/frame (PLAYER_SPEED); se mantiene exactamente esa
// cadencia de deslizamiento aquí (SIMA_PLAYER_SLIDE_FRAMES=8,
// SIMA_PLAYER_SLIDE_SPEED=16/8=2px/frame) para que el "paso" se siga
// sintiendo igual de decidido -- lo único que cambia es que ahora el
// deslizamiento SIEMPRE llega exactamente a la casilla siguiente (nunca se
// para a medias) y dispara el turno de los enemigos al terminar.
#define SIMA_PLAYER_SLIDE_FRAMES 8
#define SIMA_PLAYER_SLIDE_SPEED (SIMA_TILE_PX / SIMA_PLAYER_SLIDE_FRAMES)  // 2 px/frame

// Los enemigos se deslizan con la MISMA cadencia que el jugador a propósito:
// un turno se "lee" como dos deslizamientos simétricos (el tuyo, luego el
// suyo), no como el jugador rápido y los enemigos arrastrándose o al revés.
#define SIMA_ENEMY_SLIDE_FRAMES 8
#define SIMA_ENEMY_SLIDE_SPEED (SIMA_TILE_PX / SIMA_ENEMY_SLIDE_FRAMES)  // 2 px/frame

// Frames de juego entre pasos del ciclo de caminata (4 frames de move, ver
// FRAME_MOVE_BASE/MOVE_FRAME_COUNT): con un deslizamiento de
// SIMA_PLAYER_SLIDE_FRAMES=8, un periodo de 2 encaja EXACTO -- los 4 frames
// de caminar se ven, uno cada uno, en cada casilla cruzada (2*4=8), a
// diferencia del ciclo de 2 poses que habia antes de la tarea de animacion.
#define SIMA_MOVE_ANIM_PERIOD 2

// Ciclo idle (3 frames, ver FRAME_IDLE_BASE/IDLE_FRAME_COUNT): mucho mas
// lento que el de caminar a proposito -- es "respirar quieto", no un paso.
// NÚMERO DE GUSTO, afinable jugando.
#define SIMA_IDLE_ANIM_PERIOD 20

// Nota: nunca inicializar estos estaticos con un valor no-cero inline (p.
// ej. "= MAX_SPRITES") -- el linker moderno de este repo descarta la
// seccion .data resultante (probado: rompe con "defined in discarded
// section .data"). sPlayerActive (BSS, arranca en FALSE) hace de guarda en
// vez de comparar sPlayerSpriteId contra un centinela.
static bool8 sPlayerActive;
static u8 sPlayerSpriteId;
static u8 sPlayerFloor;
static s16 sPlayerX;   // esquina superior izquierda del sprite, en píxeles de pantalla
static s16 sPlayerY;
static u8 sPlayerFacing;
static bool8 sPlayerMoving;
// Margen de giro (tarea de sensación "mantener para caminar"): sTurnGraceActive
// es TRUE desde el frame en que el jugador gira hacia un lado al que no
// miraba (JOY_HELD detecta la pulsación, sPlayerFacing cambia) hasta que el
// margen se agota (empieza a caminar) o el botón se suelta (se cancela: solo
// giró). sTurnGraceTimer cuenta frames de juego mientras está activo. Ambos
// arrancan en su reposo (FALSE/0) vía BSS, misma regla de estáticos que el
// resto del archivo. Ver SimaActors_ResolveHorizInput (declarada en sima.h)
// para la lógica pura, y el comentario grande junto a UpdatePlayerInput para
// cómo se usan aquí.
static bool8 sTurnGraceActive;
static u8 sTurnGraceTimer;
static u8 sPlayerAnimStep;    // 0..MOVE_FRAME_COUNT-1: frame actual del ciclo de caminar
static u8 sPlayerAnimTimer;
static u8 sPlayerIdleAnimStep;   // 0..IDLE_FRAME_COUNT-1: frame actual del ciclo idle
static u8 sPlayerIdleAnimTimer;

// Vida y daño por contacto (Tarea 6). sPlayerHP arranca en 0 en .bss (misma
// regla de estáticos que el resto del archivo) pero SimaActors_InitPlayer lo
// fija a SIMA_PLAYER_MAX_HP antes de que nada pueda leerlo, así que nunca se
// observa ese 0 transitorio.
static u8 sPlayerHP;
// Golpe recibido. sPlayerInvulnTimer ERA puramente visual (parpadeo on/off
// de .invisible, porque no habia arte de golpe); la tarea de animacion lo
// convirtio en el reloj de la animacion de golpe (5 frames de
// elf-take-damage-look-right.png, ver SimaActors_DamageAnimFrame) Y,
// ADEMAS, en la ventana de invulnerabilidad como regla de juego -- ese
// segundo trabajo se QUITA en la tarea de "damage feel" (ver el comentario
// junto a SIMA_HIT_INVULN_FRAMES en sima.h sobre por que: 20 frames en un
// juego POR TURNOS no protege de nada, el reloj se agota solo mientras el
// jugador piensa). Desde esa tarea, sPlayerInvulnTimer vuelve a tener UN
// solo trabajo -- el reloj de la animacion, cuenta ATRAS desde
// SIMA_HIT_INVULN_FRAMES hasta 0, y mientras sea > 0 UpdatePlayerSprite
// muestra el frame de golpe que toque en vez de idle/caminar/muerto -- y la
// regla de juego real vive en sPlayerInvulnTurns, justo abajo.
static u8 sPlayerInvulnTimer;

// Inmunidad como regla de juego (tarea de "damage feel"): cuantos TURNOS de
// enemigos, tras el ultimo golpe, el jugador sigue protegido de uno nuevo.
// Arranca en .bss a 0 (sin proteccion). Al golpear, StartEnemyTurn lo fija a
// SIMA_HIT_INVULN_TURNS (sima.h); en cada llamada posterior a StartEnemyTurn
// -- osea, en cada turno de enemigos que se RESUELVE -- se decrementa UNA
// vez, nunca en el mismo turno en que se acaba de fijar (ver el comentario
// grande junto a StartEnemyTurn para el porque exacto de ese orden). A
// diferencia de sPlayerInvulnTimer (arriba, en FRAMES, puramente visual
// ahora), este contador es la UNICA fuente de verdad de "¿puede este
// contacto hacerme daño?" -- SimaActors_ContactShouldDamage (sima.h) es la
// funcion pura que lo lee.
static u8 sPlayerInvulnTurns;

// Congelacion de impacto (tarea de "damage feel"): cuenta ATRAS desde
// SIMA_HITSTOP_FRAMES hasta 0 mientras sTurnPhase == SIMA_TURN_HITSTOP (ver
// AdvanceHitstop). sPendingDeath es la unica diferencia entre "que pasa
// cuando el hitstop termina" -- FALSE va a SIMA_TURN_ENEMY_STEP (golpe
// normal, sigue el turno de los enemigos), TRUE va a SIMA_TURN_PLAYER_DYING
// (golpe mortal: encadena daño -> muerte sin input de por medio, ver el
// comentario grande junto a StartEnemyTurn).
static u8 sHitstopTimer;
static bool8 sPendingDeath;

// Muerte (tarea de animacion): sPlayerDeathTimer cuenta HACIA ARRIBA desde 0
// mientras SIMA_TURN_PLAYER_DEAD esta activo (ver UpdatePlayerDeath),
// reproduciendo los 3 frames de elf-dead-look-right.png. Al llegar a
// SIMA_DEATH_ANIM_FRAMES (include/sima.h) se queda clavado ahi (no sigue
// subiendo) hasta que src/sima.c note SimaActors_IsDeathAnimDone() y dispare
// el fundido a negro -- ver el GANCHO grande junto a SimaActors_ResetAfterDeath.
static u8 sPlayerDeathTimer;

// Teleport/escalera (tarea de animacion): mismo patron que sPlayerDeathTimer
// pero para los 6 frames de elf-teleport-disapear.png ("se encoge y
// desaparece"), disparado por SimaActors_StartTeleport al pisar una
// escalera desbloqueada -- ANTES del fundido de cambio de piso (que sigue
// viviendo en src/sima.c, sin cambios en su mecanica, solo retrasado hasta
// que esta animacion termine).
static u8 sPlayerTeleportTimer;

// ---------------------------------------------------------------------
// Maquina de estados del turno. Compartida entre SimaActors_UpdatePlayer y
// SimaActors_UpdateEnemies (viven en el mismo archivo): cual de las dos hace
// algo en un frame dado depende de sTurnPhase. Arranca en
// SIMA_TURN_PLAYER_INPUT (valor 0, igual que el resto de estaticos de este
// archivo arrancan en su "reposo" via BSS) tanto por el orden del enum como
// por el reinicio explicito en SimaActors_InitPlayer/WarpToFloor.
//
// SIMA_TURN_PLAYER_DEAD/SIMA_TURN_PLAYER_TELEPORT (tarea de animacion): dos
// fases mas, FUERA del ciclo normal input->move/attack->enemy_step. Ninguna
// de las dos lee input (SimaActors_UpdatePlayer solo llama a
// UpdatePlayerInput en SIMA_TURN_PLAYER_INPUT) ni deja avanzar a los
// enemigos (SimaActors_UpdateEnemies solo llama a AdvanceEnemyStepPhase en
// SIMA_TURN_ENEMY_STEP) -- "nada se mueve si tu no te mueves" se mantiene
// por construccion, no hace falta un guard aparte.
//
// SIMA_TURN_HITSTOP/SIMA_TURN_PLAYER_DYING (tarea de "damage feel"): dos
// fases mas, insertadas por StartEnemyTurn justo despues de resolver un
// golpe -- ver su comentario grande para el diagrama completo. Igual que
// DEAD/TELEPORT, ninguna de las dos lee input ni deja avanzar el
// deslizamiento de los enemigos (los guards de UpdatePlayer/UpdateEnemies de
// abajo solo reaccionan a sus fases nombradas).
// ---------------------------------------------------------------------
enum SimaTurnPhase
{
    SIMA_TURN_PLAYER_INPUT,    // esperando direccion o A; el jugador puede actuar
    SIMA_TURN_PLAYER_MOVE,     // jugador deslizandose a la casilla destino
    SIMA_TURN_PLAYER_ATTACK,   // golpe en curso (reusa sAttackTimer/UpdateAttack)
    SIMA_TURN_ENEMY_STEP,      // turno de los enemigos: se deslizan (y el jugador puede estar en pleno empujon)
    SIMA_TURN_PLAYER_DEAD,     // animacion de muerte en curso: sin input, sin turno de enemigos
    SIMA_TURN_PLAYER_TELEPORT, // animacion de "encogerse y desvanecerse" en curso, antes del fundido de piso
    SIMA_TURN_HITSTOP,         // congelacion de impacto: nada avanza, ni el reloj de la animacion de golpe
    SIMA_TURN_PLAYER_DYING,    // golpe mortal: reaccion de golpe terminando de reproducirse antes del derrumbe
};

static u8 sTurnPhase;

// Deslizamiento del jugador en curso (SIMA_TURN_PLAYER_MOVE). sPlayerSlideDX/DY
// es el delta POR FRAME (signo * SIMA_PLAYER_SLIDE_SPEED, siempre en un solo
// eje: no hay diagonales por turnos); sPlayerSlideTargetX/Y es la casilla de
// destino en píxeles, a la que se hace snap exacto en el último frame para
// no arrastrar redondeo.
static s16 sPlayerSlideDX;
static s16 sPlayerSlideDY;
static s16 sPlayerSlideTargetX;
static s16 sPlayerSlideTargetY;
static u8 sPlayerSlideTimer;

// Empujon al recibir daño (mejora de sensacion, pedida por el dueño del
// proyecto: "que lo haga retroceder o algo así"). Con el cambio a turnos:
// desplazamiento de UNA casilla en direccion contraria al enemigo que
// conectó, si esa casilla está libre -- si no, no se mueve (ver
// StartPlayerKnockback). Igual que el deslizamiento normal: delta por frame
// + destino en píxeles + cronómetro, con snap exacto al terminar.
static s16 sPlayerKnockbackDX;
static s16 sPlayerKnockbackDY;
static s16 sPlayerKnockbackTargetX;
static s16 sPlayerKnockbackTargetY;
static u8 sPlayerKnockbackTimer;
// NÚMERO DE GUSTO: más corto que un paso normal (4 frames < 8) para que el
// golpe se sienta más brusco/inmediato que un movimiento voluntario -- con
// SIMA_TILE_PX=16, eso da 16/4=4px/frame, el doble de rápido que el paso
// normal. DEBE ser <= SIMA_ENEMY_SLIDE_FRAMES (el empujón se resuelve
// siempre DENTRO del turno de los enemigos, nunca lo alarga).
#define SIMA_KNOCKBACK_SLIDE_FRAMES 4
#define SIMA_KNOCKBACK_SLIDE_SPEED (SIMA_TILE_PX / SIMA_KNOCKBACK_SLIDE_FRAMES)  // 4 px/frame

// Congelación de impacto / hit-stop (tarea de "damage feel", encargo del
// dueño: "que pase algo" al recibir el golpe -- hoy solo había animación +
// empujón, sin ningún momento de impacto real). Al aterrizar un golpe, TODO
// se congela -- ni el jugador ni los enemigos avanzan un solo píxel, ni
// siquiera el reloj de la animación de golpe -- durante
// SIMA_HITSTOP_FRAMES, ANTES de que el empujón/deslizamiento de enemigos ya
// calculados en StartEnemyTurn se reproduzcan. Es la fase SIMA_TURN_HITSTOP
// (ver el enum), resuelta en AdvanceHitstop. NÚMERO DE GUSTO: 8 frames
// (~133ms a 60Hz) es lo bastante corto para no sentirse como una pausa, pero
// sobra para que el ojo registre "el juego se paró en seco aquí" -- el
// truco barato y efectivo de hit-stop de toda la vida (Street Fighter II en
// adelante), reencarnado aquí como un frenazo seco en vez de un efecto de
// partículas o cámara, que no pega con el resto de SIMA (paso a paso,
// deliberado).
#define SIMA_HITSTOP_FRAMES 8

// Sonido del golpe (tarea de "damage feel"): SIMA era, hasta esta tarea,
// COMPLETAMENTE muda (verificado: cero PlaySE en todo src/sima.c y
// src/sima_actors.c) -- un golpe sin sonido es, con diferencia, lo que más
// restaba sensación de impacto ("no pasa nada" literal, no solo figurado).
// SE_WALL_HIT es el "porrazo" genérico que el motor ya usa en el overworld
// quien se choca contra un muro/objeto (src/field_player_avatar.c) -- un
// golpe corto y seco, sin melodía ni asociación a un movimiento Pokémon
// concreto (a diferencia de SE_M_MEGA_KICK/SE_M_COMET_PUNCH, que "suenan a
// batalla Pokémon", fuera de tono para un dungeon crawler). Se reutiliza tal
// cual -- no hay SE dedicado a "criatura te muerde" en el pool del juego
// base, y sería préstamo por FUNCIÓN (un golpe seco), no por objeto directo.
// NOTA para otra tarea: esto NO arregla que el resto de SIMA (pasos,
// arma, muerte, escaleras) siga sin un solo efecto de sonido -- ver el
// informe de esta tarea.
#define SIMA_HIT_SE SE_WALL_HIT

// Ataque (Tarea 7). sWeaponActive es la misma guarda de presupuesto de
// sprites que sPlayerActive (por si CreateSprite se queda sin hueco).
// sAttackTimer en 0 significa "sin golpe en curso"; 1..ATTACK_TOTAL_FRAMES
// mientras el golpe está en curso (ver UpdateAttack). sAttackFacing fija la
// dirección del golpe al iniciarlo, no la lee de sPlayerFacing cada frame:
// con turnos el jugador no puede girar a mitad de un golpe de todos modos
// (SIMA_TURN_PLAYER_ATTACK no lee input), pero fijarla explícita documenta
// la intención.
static bool8 sWeaponActive;
static u8 sWeaponSpriteId;
static u8 sAttackTimer;
static u8 sAttackFacing;

static void UpdatePlayerSprite(void);
static void UpdatePlayerInput(void);
static void UpdatePlayerSlide(void);
static void UpdateAttack(void);
static void AdvancePlayerKnockback(void);
static void StartPlayerKnockback(s8 enemyTileX, s8 enemyTileY);
static void StartEnemyTurn(void);
static void AdvanceEnemyStepPhase(void);
static void UpdatePlayerDeath(void);
static void UpdatePlayerTeleport(void);
static void ResetPlayerAfterDeath(void);
static void AdvanceHitstop(void);
static void UpdatePlayerDying(void);
// Colisión jugador-enemigo (reconstrucción tras el apagón, ver el commit
// 9fa98d870 y el informe de esta tarea): definida más abajo, junto al resto
// del estado de enemigos (sEnemyX/Y/Alive), pero forward-declarada aquí
// porque UpdatePlayerInput/StartPlayerKnockback -- que viven ANTES de esa
// sección en el archivo -- la necesitan. Mismo patrón que StartEnemyTurn,
// arriba.
static bool8 TileHasLiveEnemy(s8 x, s8 y);

// Función pura (turnos): la casilla a la que el jugador se movería un paso
// desde (x, y) [casillas de sala, no píxeles] mirando `facing`. Separada del
// input y de los sprites para que el harness in-ROM (src/phantom_test.c)
// pueda ejercitarla sin pulsar nada, igual que antes hacía
// SimaActors_BoxFits -- pero ahora en casillas, no en una caja de píxeles:
// con movimiento por rejilla, SimaRoom_IsSolid sobre la casilla destino
// basta. Devuelve FALSE sin mover outX/outY (se quedan en la posición de
// partida) si la casilla destino está bloqueada -- ese es el caso "girarse
// es gratis, no consume turno" del brief: quien llama solo tiene que mirar
// el valor de retorno para decidir si arranca un deslizamiento o no.
bool8 SimaActors_PlayerStepTarget(u8 floor, s8 x, s8 y, u8 facing, s8 *outX, s8 *outY)
{
    s8 nx = x;
    s8 ny = y;

    switch (facing)
    {
    case SIMA_FACING_UP:
        ny--;
        break;
    case SIMA_FACING_DOWN:
        ny++;
        break;
    case SIMA_FACING_LEFT:
        nx--;
        break;
    case SIMA_FACING_RIGHT:
        nx++;
        break;
    }

    if (SimaRoom_IsSolid(floor, nx, ny))
    {
        *outX = x;
        *outY = y;
        return FALSE;
    }

    *outX = nx;
    *outY = ny;
    return TRUE;
}

// Función pura (Tarea 6): resta `amount` de `hp` saturando en 0. Separada de
// cualquier estado (recibe/devuelve el valor, no toca sPlayerHP) para que el
// harness in-ROM la ejercite sin sprites -- un underflow en u8 aquí daría
// 255 de vida y haría al jugador inmortal justo cuando debería morir.
u8 SimaActors_ApplyDamage(u8 hp, u8 amount)
{
    if (amount >= hp)
        return 0;
    return hp - amount;
}

u8 SimaActors_GetPlayerHP(void)
{
    return sPlayerHP;
}

bool8 SimaActors_IsPlayerDead(void)
{
    return sPlayerHP == 0;
}

// Función pura (tarea de "damage feel"): ¿debería un contacto de enemigo
// hacer daño AHORA, dado que quedan `invulnTurnsRemaining` turnos de
// inmunidad? Aislada en su propia función de una línea, mismo criterio que
// SimaActors_StairsUnlocked -- la regla es tan simple que cabría inline en
// StartEnemyTurn, pero separarla es lo que le permite al harness in-ROM
// comprobar la frontera exacta (0 daña, cualquier valor > 0 no) sin turno en
// marcha, igual que SimaActors_EnemyShouldChase con el rango de detección.
bool8 SimaActors_ContactShouldDamage(u8 invulnTurnsRemaining)
{
    return invulnTurnsRemaining == 0;
}

// Funciones puras (tarea de animacion): traducen un cronometro de estado
// (que cuenta frames de juego) al indice de frame (0-based) de la animacion
// correspondiente, saturando en el ultimo frame en vez de salirse de rango.
// Separadas de UpdatePlayerSprite/los sprites para que el harness in-ROM
// pueda comprobar la transicion de frame a frame (y sus fronteras exactas,
// en particular el saturado al final) sin poder pulsar nada -- mismo
// espiritu que SimaActors_ApplyDamage/PlayerStepTarget.
//
// SimaActors_DamageAnimFrame recibe `invulnTimer` con la MISMA semantica que
// sPlayerInvulnTimer: cuenta ATRAS desde SIMA_HIT_INVULN_FRAMES (recien
// golpeado) hasta 0 (fuera de la ventana de golpe) -- por eso el calculo
// invierte el sentido (a mas timer restante, frame MAS TEMPRANO de la
// animacion).
u8 SimaActors_DamageAnimFrame(u8 invulnTimer)
{
    u8 elapsed, step;

    if (invulnTimer == 0 || invulnTimer > SIMA_HIT_INVULN_FRAMES)
        return 0;   // sin golpe en curso (o valor fuera de rango): primer frame por defecto

    elapsed = SIMA_HIT_INVULN_FRAMES - invulnTimer;
    step = elapsed / SIMA_DAMAGE_ANIM_PERIOD;
    if (step >= DAMAGE_FRAME_COUNT)
        step = DAMAGE_FRAME_COUNT - 1;
    return step;
}

// SimaActors_DeathAnimFrame/TeleportAnimFrame reciben su cronometro con la
// semantica CONTRARIA (cuentan hacia ADELANTE desde 0, como
// sPlayerDeathTimer/sPlayerTeleportTimer) -- el calculo es la division
// directa, saturada en el ultimo frame.
u8 SimaActors_DeathAnimFrame(u8 deathTimer)
{
    u8 step = deathTimer / SIMA_DEATH_ANIM_PERIOD;
    if (step >= DEAD_FRAME_COUNT)
        step = DEAD_FRAME_COUNT - 1;
    return step;
}

u8 SimaActors_TeleportAnimFrame(u8 teleportTimer)
{
    u8 step = teleportTimer / SIMA_TELEPORT_ANIM_PERIOD;
    if (step >= TELEPORT_FRAME_COUNT)
        step = TELEPORT_FRAME_COUNT - 1;
    return step;
}

// Pura, sin sprites: ¿está el turno del jugador completamente resuelto (de
// nuevo esperando input, con el sprite asentado exactamente en su casilla,
// sin deslizamiento ni golpe en curso)? src/sima.c la usa para no comprobar
// la escalera a mitad de un deslizamiento (podría "adelantarse" a la casilla
// de llegada antes de tiempo si se mirase el centro del sprite en pleno
// movimiento).
bool8 SimaActors_IsPlayerIdle(void)
{
    return sTurnPhase == SIMA_TURN_PLAYER_INPUT;
}

// Función pura (Tarea 7, restringida a izquierda/derecha en la tarea de
// sensación): casilla de 16x16 (esquina superior izquierda, en píxeles) que
// amenaza el arma cuando el jugador -- parado en (playerX, playerY) -- ataca
// mirando `facing`. Siempre la casilla ADYACENTE (un salto de 16px en el eje
// de la dirección), nunca la propia del jugador: así un golpe no puede
// autolesionar, y solo alcanza a un enemigo que esté de verdad delante, no a
// uno que solo comparta casilla por detrás o al lado. Separada de todo
// estado para que el harness in-ROM la ejercite sin sprites, igual que
// SimaActors_PlayerStepTarget.
//
// Con la vista de perfil pura, `facing` YA SOLO llega como
// SIMA_FACING_LEFT/SIMA_FACING_RIGHT (viene siempre de sAttackFacing, que
// copia a sPlayerFacing -- ver el comentario de cabecera del archivo): los
// casos ARRIBA/ABAJO que esta función tenía se ELIMINARON, no se dejaron
// como ramas muertas. Cualquier valor que no sea LEFT se trata como RIGHT
// (no hay un tercer resultado razonable que devolver).
void SimaActors_WeaponHitbox(u8 facing, s16 playerX, s16 playerY, s16 *outX, s16 *outY)
{
    s16 x = playerX;

    if (facing == SIMA_FACING_LEFT)
        x -= 16;
    else
        x += 16;

    *outX = x;
    *outY = playerY;
}

void SimaActors_InitPlayer(u8 floor)
{
    s8 spawnX, spawnY;

    SimaRoom_GetSpawn(floor, &spawnX, &spawnY);

    sPlayerFloor = floor;
    sPlayerX = (s16)spawnX * SIMA_TILE_PX;
    sPlayerY = (s16)spawnY * SIMA_TILE_PX;
    // NÚMERO DE GUSTO: con vista de perfil pura, sPlayerFacing solo puede
    // ser LEFT/RIGHT (ver el comentario de cabecera del archivo) -- DOWN ya
    // no es un valor válido para el sprite. RIGHT como mirada inicial es
    // arbitrario (no hay una "cara" canónica en una sala vista de perfil);
    // ajustar aquí si un piso concreto pide entrar mirando al otro lado.
    sPlayerFacing = SIMA_FACING_RIGHT;
    sPlayerMoving = FALSE;
    sTurnGraceActive = FALSE;   // tarea de sensación: sin margen de giro en curso
    sTurnGraceTimer = 0;
    sPlayerAnimStep = 0;
    sPlayerAnimTimer = 0;
    sPlayerIdleAnimStep = 0;    // tarea de animacion: ciclo idle, arranca en el primer frame
    sPlayerIdleAnimTimer = 0;
    sPlayerHP = SIMA_PLAYER_MAX_HP;   // vida solo se fija al montar el modo, no en cada piso (ver WarpToFloor)
    sPlayerInvulnTimer = 0;
    sPlayerInvulnTurns = 0;     // tarea de "damage feel": sin proteccion por turnos al arrancar
    sHitstopTimer = 0;          // tarea de "damage feel": sin congelacion de impacto en curso
    sPendingDeath = FALSE;
    sPlayerDeathTimer = 0;      // tarea de animacion: sin animacion de muerte en curso
    sPlayerTeleportTimer = 0;   // tarea de animacion: sin animacion de teleport en curso
    sAttackTimer = 0;   // sin golpe en curso (Tarea 7)
    sPlayerKnockbackTimer = 0;   // sin empujon en curso
    sPlayerSlideTimer = 0;       // sin deslizamiento en curso
    sTurnPhase = SIMA_TURN_PLAYER_INPUT;   // reinicio explicito -- ver la nota de sCurrentFloor en src/sima.c

    LoadSpriteSheet(&sSheet_SimaPlayer);
    LoadSpritePalette(&sPal_SimaPlayer);

    // CreateSprite posiciona por el CENTRO del sprite, no por la esquina
    // superior izquierda (ver CalcCenterToCornerVec en src/sprite.c): +8 en
    // cada eje porque el sprite es 16x16.
    sPlayerSpriteId = CreateSprite(&sTmpl_SimaPlayer, sPlayerX + 8, sPlayerY + 8, 0);
    sPlayerActive = (sPlayerSpriteId != MAX_SPRITES);

    if (sPlayerActive)
        UpdatePlayerSprite();

    // Arma (Tarea 7): un sprite más, creado una sola vez aquí (mismo motivo
    // que sPlayerActive/sEnemyAlive -- LoadSpriteSheet no es idempotente,
    // ver la nota de cabecera del archivo). Arranca invisible: solo se
    // muestra durante windup/activo de un golpe (ver UpdateAttack). Con
    // jugador + 3 enemigos + arma van 5 de los 64 sprites de MAX_SPRITES.
    LoadSpriteSheet(&sSheet_SimaWeapon);
    // Paleta ya cargada arriba (sPal_SimaPlayer); LoadSpritePalette es
    // idempotente por tag, pero el arma ni la vuelve a pedir -- reutiliza la
    // misma carga del jugador via paletteTag en sTmpl_SimaWeapon.
    // Posición inicial arbitraria (columna del jugador): UpdateAttack la
    // recoloca sobre la casilla adyacente cada frame en cuanto arranca un
    // golpe (ver el comentario grande sobre WEAPON_SHEET_FRAMES) -- esta
    // solo importa mientras el arma arranca invisible, antes del primer golpe.
    sWeaponSpriteId = CreateSprite(&sTmpl_SimaWeapon, sPlayerX + 8, sPlayerY, 0);
    sWeaponActive = (sWeaponSpriteId != MAX_SPRITES);
    if (sWeaponActive)
        gSprites[sWeaponSpriteId].invisible = TRUE;
}

void SimaActors_WarpToFloor(u8 floor)
{
    s8 spawnX, spawnY;

    if (!sPlayerActive)
        return;  // sin sprite que recolocar (ver el guard de SimaActors_UpdatePlayer)

    SimaRoom_GetSpawn(floor, &spawnX, &spawnY);

    // Mismo estado que fija SimaActors_InitPlayer, salvo que aqui NO se toca
    // el sprite (sheet/paleta/CreateSprite): se reutiliza el que ya existe,
    // solo se le cambian piso/posicion/facing y se sincroniza con
    // UpdatePlayerSprite.
    sPlayerFloor = floor;
    sPlayerX = (s16)spawnX * SIMA_TILE_PX;
    sPlayerY = (s16)spawnY * SIMA_TILE_PX;
    sPlayerFacing = SIMA_FACING_RIGHT;   // mismo valor por defecto que SimaActors_InitPlayer, ver su comentario
    sPlayerMoving = FALSE;
    sTurnGraceActive = FALSE;   // tarea de sensación: tampoco se arrastra un margen de giro entre pisos
    sTurnGraceTimer = 0;
    sPlayerAnimStep = 0;
    sPlayerAnimTimer = 0;
    sPlayerIdleAnimStep = 0;
    sPlayerIdleAnimTimer = 0;
    // sPlayerHP NO se resetea aquí a propósito: la vida es del intento, no
    // del piso -- bajar un piso con un corazón no debería devolverte los
    // otros dos. El golpe/parpadeo sí se corta: no tiene sentido arrastrar
    // frames de "acabo de recibir un golpe" al piso nuevo.
    sPlayerInvulnTimer = 0;
    sPlayerInvulnTurns = 0;   // tampoco se arrastra proteccion por turnos al piso nuevo, misma razon
    // Golpe/deslizamiento/empujon/muerte/teleport/hitstop en curso tampoco se
    // arrastran al piso nuevo, misma razon: aparecer en el spawn nuevo a
    // mitad de una animacion de un piso distinto seria confuso. El turno
    // vuelve siempre a PLAYER_INPUT.
    sAttackTimer = 0;
    sPlayerSlideTimer = 0;
    sPlayerKnockbackTimer = 0;
    sPlayerDeathTimer = 0;
    sPlayerTeleportTimer = 0;
    sHitstopTimer = 0;
    sPendingDeath = FALSE;
    sTurnPhase = SIMA_TURN_PLAYER_INPUT;
    if (sWeaponActive)
        gSprites[sWeaponSpriteId].invisible = TRUE;

    UpdatePlayerSprite();
}

// Despacha segun la fase del turno: solo UNA de estas ramas hace algo en un
// frame dado. Esto es, literalmente, "nada se mueve si tu no te mueves": en
// SIMA_TURN_PLAYER_INPUT, si no hay input valido, esta funcion no toca
// sPlayerX/sPlayerY en absoluto (ver UpdatePlayerInput).
void SimaActors_UpdatePlayer(void)
{
    if (!sPlayerActive)
        return;  // SimaActors_InitPlayer no se llamó, o CreateSprite se quedó sin presupuesto (MAX_SPRITES)

    switch (sTurnPhase)
    {
    case SIMA_TURN_PLAYER_INPUT:
        UpdatePlayerInput();
        break;
    case SIMA_TURN_PLAYER_MOVE:
        UpdatePlayerSlide();
        break;
    case SIMA_TURN_PLAYER_ATTACK:
        UpdateAttack();
        UpdatePlayerSprite();
        break;
    case SIMA_TURN_ENEMY_STEP:
        // El jugador no lee input durante el turno de los enemigos, pero SI
        // puede estar en pleno empujon si uno de ellos acaba de conectar
        // (ver StartPlayerKnockback, llamada desde StartEnemyTurn -- misma
        // fase, mismo archivo). AdvancePlayerKnockback no hace nada si no
        // hay empujon en marcha.
        AdvancePlayerKnockback();
        UpdatePlayerSprite();
        break;
    case SIMA_TURN_PLAYER_DEAD:
        // Animacion de muerte en curso (ver StartEnemyTurn): sin input, sin
        // turno de enemigos -- src/sima.c (CheckPlayerDeath) es quien saca
        // al juego de esta fase, cuando SimaActors_IsDeathAnimDone() se
        // cumpla, arrancando el fundido a negro.
        UpdatePlayerDeath();
        break;
    case SIMA_TURN_PLAYER_TELEPORT:
        // Animacion de "encogerse y desvanecerse" en curso (ver
        // SimaActors_StartTeleport): igual que arriba, src/sima.c
        // (CheckTeleportDone) dispara el fundido de cambio de piso cuando
        // SimaActors_IsTeleportAnimDone() se cumpla.
        UpdatePlayerTeleport();
        break;
    case SIMA_TURN_HITSTOP:
        // Congelacion de impacto (tarea de "damage feel"): nada se dibuja
        // de nuevo aqui a proposito -- el sprite se queda EXACTAMENTE con el
        // frame/posicion que tenia en el instante del golpe (ya pintado por
        // UpdatePlayerSlide/UpdateAttack antes de llamar a StartEnemyTurn),
        // ese es el punto de un frenazo. AdvanceHitstop solo cuenta frames y
        // decide a que fase se sigue cuando termine.
        AdvanceHitstop();
        break;
    case SIMA_TURN_PLAYER_DYING:
        // Golpe mortal (tarea de "damage feel", encargo del dueño): la
        // reaccion al golpe (animacion de daño) termina de reproducirse
        // ANTES del derrumbe -- ver el comentario grande junto a
        // StartEnemyTurn y a UpdatePlayerDying.
        UpdatePlayerDying();
        break;
    }
}

// Función pura (tarea de sensación "mantener para caminar"): la decisión
// TURN/WAIT/WALK del eje horizontal. Ver el comentario junto a su
// declaración en sima.h para la tabla de casos completa; aquí solo una nota
// de implementación: *graceActive/*graceTimer son sTurnGraceActive/
// sTurnGraceTimer cuando la llama UpdatePlayerInput (más abajo) -- pasados
// por referencia para que el harness in-ROM pueda simularlos con variables
// locales, sin tocar el estado real del jugador.
u8 SimaActors_ResolveHorizInput(u8 facing, u8 horizDir, bool8 *graceActive, u8 *graceTimer)
{
    if (facing != horizDir)
    {
        // Mira al lado contrario de lo que se pulsa: gira YA (quien llama
        // debe asignar facing = horizDir) y arranca el margen desde cero --
        // una dirección nueva siempre reinicia el margen, sin importar en
        // qué estado estuviera antes.
        *graceActive = TRUE;
        *graceTimer = 0;
        return SIMA_HORIZ_INPUT_TURN;
    }

    if (*graceActive)
    {
        // Ya mira hacia horizDir porque ACABA de girar hacia aquí (no
        // porque ya miraba antes de que se empezara a pulsar): sigue
        // dentro del margen mientras el contador no llegue a
        // SIMA_TURN_GRACE_FRAMES.
        (*graceTimer)++;
        if (*graceTimer < SIMA_TURN_GRACE_FRAMES)
            return SIMA_HORIZ_INPUT_WAIT;

        // Margen superado sin soltar: se acaba (no se vuelve a esperar
        // mientras se siga pulsando de aquí en adelante) y arranca a
        // caminar.
        *graceActive = FALSE;
        return SIMA_HORIZ_INPUT_WALK;
    }

    // Ya miraba hacia horizDir SIN margen pendiente: o nunca hizo falta
    // girar (ya apuntaba para aquí antes de este toque), o el margen se
    // agotó en un frame anterior. Camina de inmediato.
    return SIMA_HORIZ_INPUT_WALK;
}

// SIMA_TURN_PLAYER_INPUT: lee A (ataque) y D-pad.
//
// ARRIBA/ABAJO usan JOY_HELD: mantener pulsada una dirección camina turno
// tras turno a su cadencia natural (cada turno solo avanza cuando el
// anterior termina del todo, así que HELD no puede "colar" un segundo paso
// a mitad de uno ya en marcha), en vez de exigir soltar y volver a pulsar
// por cada casilla. No hay mirada vertical que cambiar, así que en este eje
// no hay nada que girar.
//
// IZQUIERDA/DERECHA (tap-to-turn + mantener para caminar, TAREA DE
// SENSACIÓN "arreglo de mantener pulsado", esta) también usan JOY_HELD,
// pero con un margen de por medio -- SimaActors_ResolveHorizInput,
// SIMA_TURN_GRACE_FRAMES en sima.h.
//
// HISTORIA (por qué esto no era tan simple como cambiar JOY_NEW por
// JOY_HELD): la versión anterior de tap-to-turn (tarea de vista de perfil)
// usaba JOY_NEW aquí a propósito, porque con JOY_HELD el frame en que el
// jugador gira (sPlayerFacing cambia) y el frame en el que "ya mira hacia
// ahí" son ambos parte del MISMO toque físico en cuanto se mantiene pulsado
// más de 1 frame -- ¡prácticamente siempre! un frame a 60Hz dura ~16ms: el
// turno se colaba en un movimiento al frame siguiente sin que el jugador
// soltara el botón, y "un toque para girar, sin moverte" dejaba de ser
// posible en la práctica. El parche (JOY_NEW) arreglaba eso, pero como
// efecto secundario exigía soltar y repulsar por cada casilla al caminar de
// lado -- rígido, e inconsistente con arriba/abajo (que sí se podían
// mantener). Reportado por el dueño del proyecto, arreglado en esta tarea.
//
// LA SOLUCIÓN: separar el EVENTO de girar (instantáneo, en cuanto
// facing != dirección pulsada) del EVENTO de empezar a caminar (retrasado
// un margen corto). Es, en espíritu, la misma separación que resuelve
// girar-vs-caminar en el overworld de pokeemerald -- ver
// CheckMovementInputNotOnBike/PlayerNotOnBikeTurningInPlace en
// src/field_player_avatar.c, que también distinguen "cambiaste de
// dirección" (TURN_DIRECTION, un frame quieto) de "sigues en la misma"
// (MOVING) -- aunque SIN copiar su timing exacto: allí SIEMPRE se mueve
// exactamente un frame después de girar (no hay concepto de "toque puro que
// nunca mueve"), aquí SÍ hace falta que un toque corto pueda soltarse sin
// haber movido nunca, de ahí el margen en vez de un solo frame fijo.
// SimaActors_ResolveHorizInput hace esa distinción de forma pura (ver su
// comentario en sima.h); aquí solo se aplica el resultado:
//   - TURN: gira, no mueve, no consume turno (el margen acaba de arrancar).
//   - WAIT: ya giró hacia ahí hace poco, sigue dentro del margen: ni gira
//     (nada que girar) ni mueve. Si el jugador suelta el botón en este
//     punto (rama sin input, más abajo, que cancela el margen), se quedó en
//     "solo giró" -- el toque corto que pide el brief.
//   - WALK: o ya miraba hacia horizDir antes de pulsar (mueve de
//     inmediato), o el margen se acaba de agotar sin soltar (empieza a
//     caminar). Cae al mismo código de mover de más abajo, compartido con
//     arriba/abajo.
//
// A sigue usando JOY_NEW (igual que antes de esta tarea): mantenerlo
// pulsado no encadena golpes.
static void UpdatePlayerInput(void)
{
    u8 moveDir;
    u8 horizDir;
    s8 curX, curY, nextX, nextY;

    // Ciclo idle (tarea de animacion): corre TODOS los frames en que el
    // jugador esta en SIMA_TURN_PLAYER_INPUT, sin importar que accion (si
    // alguna) se decida despues este mismo frame -- el personaje "respira"
    // mientras espera, no solo cuando de verdad no pasa nada.
    sPlayerIdleAnimTimer++;
    if (sPlayerIdleAnimTimer >= SIMA_IDLE_ANIM_PERIOD)
    {
        sPlayerIdleAnimTimer = 0;
        sPlayerIdleAnimStep++;
        if (sPlayerIdleAnimStep >= IDLE_FRAME_COUNT)
            sPlayerIdleAnimStep = 0;
    }

    if (JOY_NEW(A_BUTTON))
    {
        // Un ataque nuevo corta cualquier margen de giro a medias: son dos
        // acciones distintas, el margen no debería sobrevivir a un golpe de
        // por medio.
        sTurnGraceActive = FALSE;
        sAttackFacing = sPlayerFacing;
        sAttackTimer = 1;
        sTurnPhase = SIMA_TURN_PLAYER_ATTACK;
        UpdateAttack();
        UpdatePlayerSprite();
        return;
    }

    if (JOY_HELD(DPAD_UP))
    {
        sTurnGraceActive = FALSE;   // eje distinto: no arrastra un margen horizontal pendiente
        moveDir = SIMA_FACING_UP;   // vertical: mueve de inmediato, sPlayerFacing no cambia
    }
    else if (JOY_HELD(DPAD_DOWN))
    {
        sTurnGraceActive = FALSE;
        moveDir = SIMA_FACING_DOWN;
    }
    else if (JOY_HELD(DPAD_LEFT) || JOY_HELD(DPAD_RIGHT))
    {
        horizDir = JOY_HELD(DPAD_LEFT) ? SIMA_FACING_LEFT : SIMA_FACING_RIGHT;

        switch (SimaActors_ResolveHorizInput(sPlayerFacing, horizDir, &sTurnGraceActive, &sTurnGraceTimer))
        {
        case SIMA_HORIZ_INPUT_TURN:
            // Gira ya mismo, no consume turno -- ver el comentario de
            // cabecera de esta función.
            sPlayerFacing = horizDir;
            sPlayerMoving = FALSE;
            UpdatePlayerSprite();
            return;
        case SIMA_HORIZ_INPUT_WAIT:
            // Dentro del margen: ni gira (ya giró) ni mueve todavía.
            sPlayerMoving = FALSE;
            UpdatePlayerSprite();
            return;
        default:   // SIMA_HORIZ_INPUT_WALK
            moveDir = horizDir;
            break;
        }
    }
    else
    {
        // Nada pulsado: cancela cualquier margen de giro a medias -- soltar
        // antes de que se agote es, literalmente, "solo giraste" (el toque
        // corto que pide el brief). La mirada NO se deshace: girar es
        // gratis y permanente, solo caminar necesitaba el margen.
        sTurnGraceActive = FALSE;
        sPlayerMoving = FALSE;
        UpdatePlayerSprite();
        return;
    }

    curX = (s8)(sPlayerX / SIMA_TILE_PX);
    curY = (s8)(sPlayerY / SIMA_TILE_PX);

    if (!SimaActors_PlayerStepTarget(sPlayerFloor, curX, curY, moveDir, &nextX, &nextY)
        || TileHasLiveEnemy(nextX, nextY))
    {
        // Casilla bloqueada por un muro, O por un enemigo vivo (colisión
        // jugador-enemigo, reconstrucción tras el apagón -- se perdió y sin
        // ella el jugador podía caminar ENCIMA de un enemigo: sprites
        // superpuestos y un golpe sin telégrafo, ver el informe de esta
        // tarea): no arranca deslizamiento, el turno se queda en
        // PLAYER_INPUT (no consume turno -- igual que un giro o un muro).
        // Nota de cortocircuito: si el muro ya bloqueaba, nextX/nextY quedan
        // en (curX, curY) -- la propia casilla del jugador, que nunca tiene
        // un enemigo vivo -- así que TileHasLiveEnemy ni se evalúa en ese caso.
        sPlayerMoving = FALSE;
        UpdatePlayerSprite();
        return;
    }

    sPlayerSlideDX = (s16)(nextX - curX) * SIMA_PLAYER_SLIDE_SPEED;
    sPlayerSlideDY = (s16)(nextY - curY) * SIMA_PLAYER_SLIDE_SPEED;
    sPlayerSlideTargetX = (s16)nextX * SIMA_TILE_PX;
    sPlayerSlideTargetY = (s16)nextY * SIMA_TILE_PX;
    sPlayerSlideTimer = 0;
    sPlayerMoving = TRUE;
    sTurnPhase = SIMA_TURN_PLAYER_MOVE;

    UpdatePlayerSprite();
}

// SIMA_TURN_PLAYER_MOVE: avanza el deslizamiento del jugador hacia la
// casilla destino (SIMA_PLAYER_SLIDE_SPEED px/frame, SIMA_PLAYER_SLIDE_FRAMES
// frames en total). Al llegar, snap exacto a la casilla (sin arrastrar
// redondeo) y arranca el turno de los enemigos -- el jugador NO vuelve a
// leer input hasta que ese turno termine.
static void UpdatePlayerSlide(void)
{
    sPlayerX += sPlayerSlideDX;
    sPlayerY += sPlayerSlideDY;
    sPlayerSlideTimer++;

    // Ciclo de caminar (tarea de animacion): los 4 frames de move, uno cada
    // SIMA_MOVE_ANIM_PERIOD frames de juego -- ver el comentario junto a esa
    // constante sobre por que encaja exacto en SIMA_PLAYER_SLIDE_FRAMES.
    sPlayerAnimTimer++;
    if (sPlayerAnimTimer >= SIMA_MOVE_ANIM_PERIOD)
    {
        sPlayerAnimTimer = 0;
        sPlayerAnimStep++;
        if (sPlayerAnimStep >= MOVE_FRAME_COUNT)
            sPlayerAnimStep = 0;
    }

    if (sPlayerSlideTimer >= SIMA_PLAYER_SLIDE_FRAMES)
    {
        sPlayerX = sPlayerSlideTargetX;
        sPlayerY = sPlayerSlideTargetY;
        sPlayerMoving = FALSE;
        sPlayerAnimTimer = 0;
        sPlayerAnimStep = 0;
        StartEnemyTurn();   // el paso del jugador consume turno
    }

    UpdatePlayerSprite();
}

void SimaActors_GetPlayerTile(s8 *x, s8 *y)
{
    // Centro del sprite (no la esquina superior izquierda): es la casilla
    // que "ocupa" el jugador a efectos de lógica de tareas posteriores
    // (escaleras, disparadores, enemigos). Con turnos, sPlayerX/sPlayerY
    // solo son múltiplos exactos de SIMA_TILE_PX cuando SimaActors_IsPlayerIdle()
    // es TRUE -- src/sima.c ya gatea con eso antes de mirar la escalera.
    *x = (s8)((sPlayerX + 8) / SIMA_TILE_PX);
    *y = (s8)((sPlayerY + 8) / SIMA_TILE_PX);
}

// Arranca el empujón (mejora de sensación, ahora por turnos): un
// desplazamiento de UNA casilla en dirección contraria al enemigo que
// conectó -- si esa casilla está libre. Si está bloqueada, no hay empujón en
// absoluto (el brief es explícito: "si no, no se mueve"). Llamada desde
// StartEnemyTurn (mismo archivo, misma fase) en el instante exacto en que un
// enemigo ataca, con la casilla de ESE enemigo -- nunca en diagonal, porque
// un paso de rejilla es siempre de un solo eje (ver SimaActors_EnemyStepTarget):
// a diferencia de la version en tiempo real, aqui no hace falta resolver el
// caso "el enemigo estaba en diagonal".
static void StartPlayerKnockback(s8 enemyTileX, s8 enemyTileY)
{
    s8 px = (s8)(sPlayerX / SIMA_TILE_PX);
    s8 py = (s8)(sPlayerY / SIMA_TILE_PX);
    s8 dirX = 0, dirY = 0;
    s8 targetX, targetY;

    if (enemyTileX < px)
        dirX = 1;
    else if (enemyTileX > px)
        dirX = -1;
    else if (enemyTileY < py)
        dirY = 1;
    else if (enemyTileY > py)
        dirY = -1;

    if (dirX == 0 && dirY == 0)
        return;  // caso degenerado (mismo tile que el jugador): no debería pasar, pero sin dirección no hay a dónde empujar

    targetX = (s8)(px + dirX);
    targetY = (s8)(py + dirY);

    if (SimaRoom_IsSolid(sPlayerFloor, targetX, targetY) || TileHasLiveEnemy(targetX, targetY))
        return;   // casilla de destino bloqueada (muro, o OTRO enemigo vivo -- no
                  // empujar al jugador encima de él, misma regla de colisión que
                  // UpdatePlayerInput): sin empujón, tal como pide el brief

    sPlayerKnockbackDX = (s16)dirX * SIMA_KNOCKBACK_SLIDE_SPEED;
    sPlayerKnockbackDY = (s16)dirY * SIMA_KNOCKBACK_SLIDE_SPEED;
    sPlayerKnockbackTargetX = (s16)targetX * SIMA_TILE_PX;
    sPlayerKnockbackTargetY = (s16)targetY * SIMA_TILE_PX;
    sPlayerKnockbackTimer = SIMA_KNOCKBACK_SLIDE_FRAMES;
}

// Avanza el empujón un frame, si hay uno en marcha (no-op si no). Llamada
// desde SimaActors_UpdatePlayer mientras sTurnPhase == SIMA_TURN_ENEMY_STEP.
// SIMA_KNOCKBACK_SLIDE_FRAMES <= SIMA_ENEMY_SLIDE_FRAMES por diseño (ver su
// definición), así que el empujón siempre termina dentro del turno de los
// enemigos -- nunca lo alarga ni se lo come.
static void AdvancePlayerKnockback(void)
{
    if (sPlayerKnockbackTimer == 0)
        return;

    sPlayerX += sPlayerKnockbackDX;
    sPlayerY += sPlayerKnockbackDY;
    sPlayerKnockbackTimer--;

    if (sPlayerKnockbackTimer == 0)
    {
        sPlayerX = sPlayerKnockbackTargetX;   // snap exacto, sin arrastrar redondeo
        sPlayerY = sPlayerKnockbackTargetY;
    }
}

// Escribe en el sprite el frame/flip que corresponde al facing y estado
// actuales, y sincroniza su posición en pantalla con sPlayerX/sPlayerY.
// Único punto que toca gSprites[sPlayerSpriteId]: todas las fases del turno
// pasan por aquí para no duplicar la tabla de frames.
//
// Vista de perfil pura (tarea de sensación): sPlayerFacing SOLO puede ser
// SIMA_FACING_LEFT/SIMA_FACING_RIGHT (ver el comentario de cabecera del
// archivo), así que el único trabajo de "orientación" es el h-flip -- cierto
// incluso mientras el jugador cruza una casilla en vertical (ARRIBA/ABAJO):
// UpdatePlayerInput nunca toca sPlayerFacing en ese caso.
//
// Prioridad de animación (tarea de animación): a diferencia de antes (un
// simple if/else movimiento-vs-quieto), ahora hay varios estados que pueden
// coincidir en el tiempo, y solo UNO puede dibujarse. El orden, de mayor a
// menor prioridad, y por qué:
//   1. MUERTE (SIMA_TURN_PLAYER_DEAD) -- si el jugador está muriendo, nada
//      más importa visualmente.
//   2. TELEPORT (SIMA_TURN_PLAYER_TELEPORT) -- mismo argumento; y de hecho
//      estas dos fases son mutuamente excluyentes con TODO lo demás (ver el
//      comentario de la máquina de estados, junto al enum).
//   3. GOLPE (sPlayerInvulnTimer > 0) -- por delante de mover/quieto aposta:
//      el jugador puede recibir un golpe y, sin que termine el flash de
//      reacción, quedarse quieto o empezar a moverse en su turno siguiente
//      (el golpe no bloquea el turno, solo blinda de un segundo golpe --
//      ver el comentario junto a sPlayerInvulnTimer). Se prefiere seguir
//      viendo la reacción al golpe durante su ventana completa antes que
//      cortarla por un nuevo movimiento.
//   4. MOVIÉNDOSE (sPlayerMoving) -- ciclo de caminar.
//   5. QUIETO -- ciclo idle.
//
// Golpe mortal (tarea de "damage feel"): SIMA_TURN_PLAYER_DYING NO tiene su
// propia rama aquí -- no hace falta. Durante esa fase sPlayerInvulnTimer
// sigue siendo > 0 (UpdatePlayerDying lo decrementa, ver su comentario), así
// que cae solo en la rama 3 (GOLPE) de más arriba y muestra exactamente la
// misma reacción que un golpe no mortal. En el frame exacto en que se agota
// y sTurnPhase pasa a SIMA_TURN_PLAYER_DEAD, la siguiente llamada ya entra
// por la rama 1 -- la transición daño -> muerte queda cubierta por las
// prioridades que ya existían, sin un caso especial nuevo.
static void UpdatePlayerSprite(void)
{
    struct Sprite *sprite = &gSprites[sPlayerSpriteId];
    u16 frameTile;
    bool8 hFlip = (sPlayerFacing == SIMA_FACING_LEFT);
    bool8 hidden = FALSE;

    if (sTurnPhase == SIMA_TURN_PLAYER_DEAD)
    {
        frameTile = FRAME_DEAD_BASE
            + SimaActors_DeathAnimFrame(sPlayerDeathTimer) * PLAYER_TILES_PER_FRAME;
    }
    else if (sTurnPhase == SIMA_TURN_PLAYER_TELEPORT)
    {
        frameTile = FRAME_TELEPORT_BASE
            + SimaActors_TeleportAnimFrame(sPlayerTeleportTimer) * PLAYER_TILES_PER_FRAME;
        // Una vez agotados los 6 frames (ya totalmente "desaparecido") se
        // oculta del todo -- por si el último frame de
        // elf-teleport-disapear.png no fuese 100% transparente, no hay que
        // confiar en el arte para el efecto final, la lógica lo garantiza.
        hidden = (sPlayerTeleportTimer >= SIMA_TELEPORT_ANIM_FRAMES);
    }
    else if (sPlayerInvulnTimer > 0)
    {
        frameTile = FRAME_DAMAGE_BASE
            + SimaActors_DamageAnimFrame(sPlayerInvulnTimer) * PLAYER_TILES_PER_FRAME;
    }
    else if (sPlayerMoving)
    {
        frameTile = FRAME_MOVE_BASE + sPlayerAnimStep * PLAYER_TILES_PER_FRAME;
    }
    else
    {
        frameTile = FRAME_IDLE_BASE + sPlayerIdleAnimStep * PLAYER_TILES_PER_FRAME;
    }

    sprite->oam.tileNum = sprite->sheetTileStart + frameTile;
    // Sin sistema de ANIMCMD de por medio (ver el comentario de sTmpl_SimaPlayer),
    // asi que el flip se escribe a mano: con affineMode OFF, los bits 3/4 de
    // matrixNum SON el h-flip/v-flip (ver struct OamData en include/gba/types.h),
    // no hace falta pasar por SetSpriteOamFlipBits.
    sprite->oam.matrixNum = hFlip ? ST_OAM_HFLIP : 0;
    sprite->x = sPlayerX + 8;
    sprite->y = sPlayerY + 8;
    sprite->invisible = hidden;
}

// SIMA_TURN_PLAYER_DEAD: avanza el cronómetro de la animación de muerte
// (clavado en SIMA_DEATH_ANIM_FRAMES al llegar, no sigue subiendo -- ver
// SimaActors_IsDeathAnimDone) y sincroniza el sprite. src/sima.c
// (CheckPlayerDeath) es quien decide cuándo, con la animación ya terminada,
// arrancar el fundido a negro real.
static void UpdatePlayerDeath(void)
{
    if (sPlayerDeathTimer < SIMA_DEATH_ANIM_FRAMES)
        sPlayerDeathTimer++;
    UpdatePlayerSprite();
}

// SIMA_TURN_PLAYER_TELEPORT: mismo patrón que UpdatePlayerDeath para la
// animación de "encogerse y desvanecerse" (ver SimaActors_StartTeleport).
static void UpdatePlayerTeleport(void)
{
    if (sPlayerTeleportTimer < SIMA_TELEPORT_ANIM_FRAMES)
        sPlayerTeleportTimer++;
    UpdatePlayerSprite();
}

// Congelación de impacto (tarea de "damage feel"): cuenta sHitstopTimer
// hacia 0 sin tocar ningún sprite (ver el comentario del case en
// SimaActors_UpdatePlayer) y, al agotarse, decide a qué fase sigue --
// sPendingDeath es la ÚNICA diferencia entre un golpe normal (a
// SIMA_TURN_ENEMY_STEP, el turno de los enemigos que StartEnemyTurn ya dejó
// calculado) y uno mortal (a SIMA_TURN_PLAYER_DYING, ver UpdatePlayerDying
// justo abajo).
static void AdvanceHitstop(void)
{
    if (sHitstopTimer > 0)
        sHitstopTimer--;

    if (sHitstopTimer == 0)
        sTurnPhase = sPendingDeath ? SIMA_TURN_PLAYER_DYING : SIMA_TURN_ENEMY_STEP;
}

// Golpe mortal (tarea de "damage feel", encargo del dueño del proyecto:
// "primero la animación de DAÑO, y después la de MUERTE" -- antes, al
// llegar a 0 de vida, StartEnemyTurn saltaba directo a SIMA_TURN_PLAYER_DEAD
// y la reacción al golpe nunca llegaba a verse, porque UpdatePlayerSprite
// prioriza MUERTE por encima de GOLPE, ver su comentario grande). Esta fase
// es el puente: dura exactamente lo mismo que un golpe normal tarda en
// reproducir sus 5 frames (sPlayerInvulnTimer, el MISMO reloj de siempre --
// aquí es esta función, no SimaActors_UpdateEnemies, quien lo decrementa
// mientras esta fase esté activa, para no contarlo dos veces en el mismo
// frame, ver el guard en SimaActors_UpdateEnemies) y, al agotarse, arranca
// el derrumbe de siempre (SIMA_TURN_PLAYER_DEAD/sPlayerDeathTimer, sin
// cambios). Sin input posible de por medio en ningún punto de la cadena
// hitstop -> daño -> muerte: ninguna de esas tres fases lee el D-pad/A
// (SimaActors_UpdatePlayer solo llama a UpdatePlayerInput en
// SIMA_TURN_PLAYER_INPUT).
static void UpdatePlayerDying(void)
{
    if (sPlayerInvulnTimer > 0)
    {
        sPlayerInvulnTimer--;
    }
    else
    {
        sPlayerDeathTimer = 0;
        sTurnPhase = SIMA_TURN_PLAYER_DEAD;
    }
    UpdatePlayerSprite();
}

bool8 SimaActors_IsDeathAnimDone(void)
{
    return sTurnPhase == SIMA_TURN_PLAYER_DEAD
        && sPlayerDeathTimer >= SIMA_DEATH_ANIM_FRAMES;
}

bool8 SimaActors_IsTeleportAnimDone(void)
{
    return sTurnPhase == SIMA_TURN_PLAYER_TELEPORT
        && sPlayerTeleportTimer >= SIMA_TELEPORT_ANIM_FRAMES;
}

// Arranca la animación de "encogerse y desvanecerse" (tarea de animación):
// llamada por src/sima.c (CheckStairs) al pisar una escalera desbloqueada,
// EN VEZ de fundir a negro directamente como hacía antes. El fundido de
// verdad lo dispara src/sima.c (CheckTeleportDone) cuando
// SimaActors_IsTeleportAnimDone() se cumpla.
void SimaActors_StartTeleport(void)
{
    sPlayerTeleportTimer = 0;
    sTurnPhase = SIMA_TURN_PLAYER_TELEPORT;
    if (sWeaponActive)
        gSprites[sWeaponSpriteId].invisible = TRUE;   // por si quedara visible de un golpe justo antes
    UpdatePlayerSprite();
}

// Repone al jugador tras morir: spawn del piso actual, vida llena. A
// diferencia de SimaActors_WarpToFloor (que preserva la vida a propósito --
// bajar un piso con un corazón no debería devolver los otros dos), morir SÍ
// restaura la vida al máximo: es un reinicio del intento, no un progreso.
// Estática (no expuesta) -- la única entrada pública de esta ruta es
// SimaActors_ResetAfterDeath, más abajo, que además repone a los enemigos.
static void ResetPlayerAfterDeath(void)
{
    s8 spawnX, spawnY;

    SimaRoom_GetSpawn(sPlayerFloor, &spawnX, &spawnY);

    sPlayerX = (s16)spawnX * SIMA_TILE_PX;
    sPlayerY = (s16)spawnY * SIMA_TILE_PX;
    sPlayerFacing = SIMA_FACING_RIGHT;
    sPlayerMoving = FALSE;
    sPlayerAnimStep = 0;
    sPlayerAnimTimer = 0;
    sPlayerIdleAnimStep = 0;
    sPlayerIdleAnimTimer = 0;
    sPlayerHP = SIMA_PLAYER_MAX_HP;   // a diferencia de WarpToFloor: morir SÍ restaura la vida
    sPlayerInvulnTimer = 0;
    sPlayerInvulnTurns = 0;   // reinicio del intento: sin proteccion arrastrada del golpe que mato al jugador
    sPlayerDeathTimer = 0;
    sPlayerTeleportTimer = 0;
    sAttackTimer = 0;
    sPlayerSlideTimer = 0;
    sPlayerKnockbackTimer = 0;
    sHitstopTimer = 0;
    sPendingDeath = FALSE;
    sTurnPhase = SIMA_TURN_PLAYER_INPUT;   // el turno queda coherente: listo para leer input de nuevo
    if (sWeaponActive)
        gSprites[sWeaponSpriteId].invisible = TRUE;

    UpdatePlayerSprite();
}

// Avanza el golpe en curso (Tarea 7): decide qué frame del arma mostrar (o
// si se oculta, en recuperación), la coloca sobre la casilla adyacente a
// sAttackFacing (SimaActors_WeaponHitbox) y cuenta el frame. NO comprueba
// enemigos -- eso lo hace SimaActors_UpdateEnemies (más abajo en este mismo
// archivo, con AttackHitboxActive/SimaActors_WeaponHitbox), que ya recorre
// los enemigos cada frame y tiene sus posiciones a mano; duplicar ese bucle
// aquí sería la misma información dos veces. sAttackTimer es la ÚNICA
// fuente de verdad de la fase del golpe: UpdateEnemies solo LEE su valor a
// través de AttackHitboxActive, nunca lo toca. Al terminar (recuperación
// cumplida), el golpe consume el turno: le toca a los enemigos.
//
// sAttackFacing solo puede ser LEFT/RIGHT (copia de sPlayerFacing, ver la
// cabecera del archivo); la dirección se resuelve con un HFLIP de OAM sobre el
// arte de izquierda (ver el comentario grande sobre WEAPON_SHEET_FRAMES).
static void UpdateAttack(void)
{
    // Los 4 frames que se ven durante la ventana ACTIVA, en orden: el arco de
    // tajo barriendo hacia abajo y el reposo/follow-through. El índice se
    // satura al último por si algún día ATTACK_ACTIVE_FRAMES pasa de 4.
    static const u8 sSwingFrame[4] = {
        FRAME_WEAPON_ARC1, FRAME_WEAPON_ARC2, FRAME_WEAPON_ARC3, FRAME_WEAPON_REST,
    };
    bool8 hFlip;
    s16 hitX, hitY;

    if (!sWeaponActive)
    {
        // Sin sprite de arma (CreateSprite se quedó sin presupuesto): no hay
        // nada que animar ni golpe que reproducir, pero tampoco hay que
        // dejar al jugador congelado para siempre esperando un golpe que
        // nunca se resuelve -- el turno pasa igualmente a los enemigos.
        sAttackTimer = 0;
        StartEnemyTurn();
        return;
    }

    // REVISADO (el dueño rechazó anclar la espada sobre el jugador -- tapaba
    // su sprite, ver el comentario grande sobre WEAPON_SHEET_FRAMES): se
    // ancla en la casilla ADYACENTE que amenaza el golpe, la misma que
    // SimaActors_WeaponHitbox ya calculaba para el daño (esa función NO
    // cambia; ahora también posiciona el dibujo). hitY == sPlayerY siempre
    // (vista de perfil pura: el objetivo está en la misma fila), así que la
    // base del arma queda alineada con la base de esa casilla y la mitad
    // alzada en la fila de arriba -- mismo anclaje vertical que antes, solo
    // cambia la columna.
    SimaActors_WeaponHitbox(sAttackFacing, sPlayerX, sPlayerY, &hitX, &hitY);
    gSprites[sWeaponSpriteId].x = hitX + 8;
    gSprites[sWeaponSpriteId].y = hitY;

    // El arte mira a la IZQUIERDA; se voltea para DERECHA. El flip es sobre el
    // centro del sprite (columna del jugador), así que la espada queda simétrica
    // a ambos lados.
    hFlip = (sAttackFacing == SIMA_FACING_RIGHT);
    gSprites[sWeaponSpriteId].oam.matrixNum = hFlip ? ST_OAM_HFLIP : 0;

    if (sAttackTimer <= ATTACK_WINDUP_FRAMES)
    {
        // Windup: espada alzada, telégrafo del golpe; todavía no daña.
        gSprites[sWeaponSpriteId].invisible = FALSE;
        gSprites[sWeaponSpriteId].oam.tileNum = gSprites[sWeaponSpriteId].sheetTileStart
            + FRAME_WEAPON_RAISED;
    }
    else if (sAttackTimer <= ATTACK_WINDUP_FRAMES + ATTACK_ACTIVE_FRAMES)
    {
        // Activo: el arco barre hacia abajo y remata en reposo, en sincronía con
        // AttackHitboxActive (usado por SimaActors_UpdateEnemies), que devuelve
        // TRUE en este mismo rango de sAttackTimer -- el daño cae mientras se ve
        // el arco.
        u8 idx = sAttackTimer - ATTACK_WINDUP_FRAMES - 1;  // 0..ACTIVE-1
        if (idx > 3)
            idx = 3;
        gSprites[sWeaponSpriteId].invisible = FALSE;
        gSprites[sWeaponSpriteId].oam.tileNum = gSprites[sWeaponSpriteId].sheetTileStart
            + sSwingFrame[idx];
    }
    else
    {
        // Recuperación: la espada ya se guardó, pero el golpe sigue en curso
        // (el turno todavía no ha pasado a los enemigos) hasta que
        // sAttackTimer llegue a ATTACK_TOTAL_FRAMES.
        gSprites[sWeaponSpriteId].invisible = TRUE;
    }

    sAttackTimer++;
    if (sAttackTimer > ATTACK_TOTAL_FRAMES)
    {
        sAttackTimer = 0;   // golpe terminado
        StartEnemyTurn();   // el golpe consume turno: ahora les toca a los enemigos
    }
}

// Función pura (Tarea 7): ¿está la caja de golpe del arma activa AHORA
// MISMO? Envuelve el mismo rango de sAttackTimer que UpdateAttack usa para
// mostrar los frames del arco (ventana ACTIVA), para que ambas lecturas de
// "¿está golpeando?" nunca se puedan desincronizar (una sola definición de la
// ventana activa).
// No pura respecto al reloj de la partida (lee sAttackTimer, estado), pero
// no depende de sprites ni de ningún enemigo -- SimaActors_UpdateEnemies la
// usa antes de comprobar la caja contra cada enemigo.
static bool8 AttackHitboxActive(void)
{
    return sAttackTimer > ATTACK_WINDUP_FRAMES
        && sAttackTimer <= ATTACK_WINDUP_FRAMES + ATTACK_ACTIVE_FRAMES;
}

// ---------------------------------------------------------------------
// Enemigos: rata, murciélago y slime en las casillas de SimaRoom_GetEnemy
// (los '*' del editor visual). Por turnos, igual que el jugador: cuando
// StartEnemyTurn se llama (al terminar el paso o el golpe del jugador),
// cada enemigo vivo calcula UNA casilla de destino hacia el jugador
// (SimaActors_EnemyStepTarget) y se desliza hacia ella durante
// SIMA_ENEMY_SLIDE_FRAMES frames -- o, si esa casilla resulta ser la del
// propio jugador, no se mueve: es un ataque, no un paso (ver StartEnemyTurn
// más abajo). Comparten la paleta única del jugador (sPlayerPal, arriba) y
// solo usan dos frames de animación "idle" de cada hoja (celdas (0,0)/(0,1)):
// ninguna de las tres criaturas tiene una pose direccional clara en el pack
// de origen, a diferencia del jugador, así que no hay ciclo de caminata por
// dirección -- esa "respiración" es puramente cosmética y sigue su propio
// reloj SIEMPRE, no está atada al turno (no es una POSICIÓN, así que no
// rompe "nada se mueve si tú no te mueves").
// ---------------------------------------------------------------------

static const u32 sRatGfx[] = INCBIN_U32("graphics/sima/rat.4bpp");
static const u32 sBatGfx[] = INCBIN_U32("graphics/sima/bat.4bpp");
static const u32 sSlimeGfx[] = INCBIN_U32("graphics/sima/slime.4bpp");

#define TAG_SIMA_RAT   0x6001
#define TAG_SIMA_BAT   0x6002
#define TAG_SIMA_SLIME 0x6003

#define ENEMY_SHEET_CELLS     24  // hoja de 4x6 celdas de 16x16 (64x96 px): ver graphics/sima/gen.py ASSETS
#define ENEMY_TILES_PER_FRAME  4  // 16x16 = 2x2 tiles de hardware de 8x8, igual que el jugador
#define ENEMY_FRAME_IDLE_A (0 * ENEMY_TILES_PER_FRAME)  // celda (0,0) de la hoja
#define ENEMY_FRAME_IDLE_B (1 * ENEMY_TILES_PER_FRAME)  // celda (0,1): "respira" para las tres criaturas
// Tarea 7: pose de muerte (aplastada/panza arriba en las tres hojas, visto
// a ojo -- rat.png/bat.png/slime.png comparten layout de 4x6). Dos celdas
// adyacentes de la misma fila, no una sola: un frame estático se lee como
// una imagen congelada más que como "acaba de morir"; alternar entre dos
// poses casi iguales durante SIMA_ENEMY_DEATH_FRAMES da un tembleque mínimo
// sin necesitar arte nuevo.
#define ENEMY_FRAME_DEATH_A (17 * ENEMY_TILES_PER_FRAME)  // celda (4,1)
#define ENEMY_FRAME_DEATH_B (18 * ENEMY_TILES_PER_FRAME)  // celda (4,2)

enum SimaEnemyKind
{
    SIMA_ENEMY_RAT,
    SIMA_ENEMY_BAT,
    SIMA_ENEMY_SLIME,
    SIMA_ENEMY_KIND_COUNT,
};

static const struct OamData sOam_SimaEnemy = {
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .priority = 1,  // igual que el jugador: por delante de BG0, detrás del HUD (BG1)
};

static const struct SpriteSheet sSheet_SimaRat = {
    sRatGfx, ENEMY_SHEET_CELLS * ENEMY_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_RAT
};
static const struct SpriteSheet sSheet_SimaBat = {
    sBatGfx, ENEMY_SHEET_CELLS * ENEMY_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_BAT
};
static const struct SpriteSheet sSheet_SimaSlime = {
    sSlimeGfx, ENEMY_SHEET_CELLS * ENEMY_TILES_PER_FRAME * TILE_SIZE_4BPP, TAG_SIMA_SLIME
};

// paletteTag = TAG_SIMA_PLAYER a propósito: los enemigos reutilizan la misma
// paleta única de sprites que el jugador (ver sPal_SimaPlayer, arriba), tal
// como fija el spec de assets ("una paleta para todos los sprites").
static const struct SpriteTemplate sTmpl_SimaRat = {
    .tileTag = TAG_SIMA_RAT, .paletteTag = TAG_SIMA_PLAYER, .oam = &sOam_SimaEnemy,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};
static const struct SpriteTemplate sTmpl_SimaBat = {
    .tileTag = TAG_SIMA_BAT, .paletteTag = TAG_SIMA_PLAYER, .oam = &sOam_SimaEnemy,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};
static const struct SpriteTemplate sTmpl_SimaSlime = {
    .tileTag = TAG_SIMA_SLIME, .paletteTag = TAG_SIMA_PLAYER, .oam = &sOam_SimaEnemy,
    .anims = gDummySpriteAnimTable, .images = NULL, .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

// Indexado por (i % SIMA_ENEMY_KIND_COUNT) en SimaActors_InitEnemies: con los
// 3 enemigos del piso 1 da exactamente uno de cada especie, en orden fijo
// (determinista, no aleatorio -- importa para la captura de depuración).
static const struct SpriteTemplate *const sEnemyTemplates[SIMA_ENEMY_KIND_COUNT] = {
    &sTmpl_SimaRat, &sTmpl_SimaBat, &sTmpl_SimaSlime,
};

// Tope de enemigos que este archivo sabe manejar a la vez: los arrays de
// abajo se dimensionan a esto, no a SimaRoom_GetEnemyCount (que es un valor
// de EJECUCIÓN, no una constante de compilación). Coincide hoy con
// SIMA_ROOM_MAX_ENEMIES (src/sima_rooms_data.h, generado); si el editor
// visual algún día coloca más de 3 enemigos en un piso, el clamp en
// SimaActors_InitEnemies recorta en vez de desbordar estos arrays.
#define SIMA_MAX_ENEMIES 3

#define SIMA_ENEMY_ANIM_PERIOD   16   // frames entre los dos frames de "respirar" (cosmético, no ligado al turno)
#define SIMA_ENEMY_CONTACT_DAMAGE 1

// SIMA_ENEMY_DETECT_RANGE vive en include/sima.h (NÚMERO DE GUSTO, afinable
// jugando) -- publica porque SimaActors_EnemyShouldChase, que la usa, esta
// expuesta al harness in-ROM. Mas alla de ese rango un enemigo deambula
// (EnemyWanderStep, mas abajo) en vez de perseguir optimamente turno tras
// turno -- el dueño lo pidio porque la persecucion optima siempre se sentia
// "demasiado lista, siempre pegada".

// Tarea 7: un enemigo muere de UN golpe -- no hay vida propia por enemigo.
// Encaja con la regla del proyecto de "sin grinding" (CLAUDE.md): un sistema
// de aguante por enemigo solo tendría sentido si hubiera algo que farmear
// para subirlo, y aquí no lo hay. También es lo que literalmente pide el
// brief ("un enemigo tocado por el arma muere"), sin condición.
//
// sEnemyDeathTimer > 0 mientras el cadáver sigue en pantalla reproduciendo
// ENEMY_FRAME_DEATH_A/B; llega a 0 y SimaActors_UpdateEnemies destruye el
// sprite. Deliberadamente SEPARADO de sEnemyAlive: el enemigo deja de contar
// como "vivo" (para SimaActors_StairsUnlocked) en el instante mismo del
// golpe, no cuando termina la animación -- la escalera no debería esperar a
// que acabe un efecto cosmético para desbloquearse.
#define SIMA_ENEMY_DEATH_FRAMES       24   // ~0.4s a 60Hz: cuánto se ve el cadáver antes de destruir el sprite
#define SIMA_ENEMY_DEATH_ANIM_PERIOD   6   // alterna DEATH_A/DEATH_B cada 6 frames

static bool8 sEnemyAlive[SIMA_MAX_ENEMIES];
static u8 sEnemyDeathTimer[SIMA_MAX_ENEMIES];  // 0 = no está muriendo (vivo, o ya destruido)
static u8 sEnemySpriteId[SIMA_MAX_ENEMIES];
static s16 sEnemyX[SIMA_MAX_ENEMIES];   // esquina superior izquierda del sprite, en píxeles
static s16 sEnemyY[SIMA_MAX_ENEMIES];
static u8 sEnemyFloor;
static u8 sEnemyCount;      // cuántos de los SIMA_MAX_ENEMIES slots están colocados en el piso actual
static u8 sEnemyAnimTimer;
static u8 sEnemyAnimStep;

// Turno de los enemigos (SIMA_TURN_ENEMY_STEP). sEnemyMoving[i] indica si el
// enemigo i está deslizándose este turno (FALSE si atacó sin moverse, si se
// quedó bloqueado en ambos ejes, o si está muerto/muriendo); sEnemySlideDX/DY
// es su delta por frame, sEnemyTargetX/Y su casilla de destino en píxeles
// (snap exacto al terminar). sEnemyStepTimer es UN solo cronómetro
// compartido por los tres -- todos se deslizan la misma cantidad de frames,
// aunque alguno se quede quieto.
static bool8 sEnemyMoving[SIMA_MAX_ENEMIES];
static s16 sEnemySlideDX[SIMA_MAX_ENEMIES];
static s16 sEnemySlideDY[SIMA_MAX_ENEMIES];
static s16 sEnemyTargetX[SIMA_MAX_ENEMIES];
static s16 sEnemyTargetY[SIMA_MAX_ENEMIES];
static u8 sEnemyStepTimer;

// Un solo golpe por turno de enemigos, sin importar cuántos ataquen a la vez
// (mismo espíritu que la ventana de invulnerabilidad de la versión en tiempo
// real, pero medido en turnos, no en frames): StartEnemyTurn la pone a FALSE
// al empezar el turno, y el primer enemigo cuyo paso aterriza en la casilla
// del jugador la sube a TRUE.
static bool8 sPlayerHitThisTurn;

// Función pura (turnos): la casilla a la que un enemigo en (ex, ey) daría su
// paso hacia el jugador en (px, py), en el piso `floor`. Elige el eje que
// más lo acerca (empate -> vertical, misma prioridad que el facing del
// jugador); si esa casilla está bloqueada por un muro prueba el otro eje; si
// los dos lo están, se queda quieto (outX/outY quedan en ex/ey). NO
// distingue "el destino es la casilla del jugador" de "el destino es suelo
// libre" -- esa decisión (moverse de verdad vs. atacar sin moverse) la toma
// StartEnemyTurn comparando el resultado contra (px, py), porque es ahí
// donde vive el estado de vida/knockback. Es exactamente la misma regla para
// "un enemigo que llega a la casilla del jugador" y "uno que ya está
// adyacente y avanza contra él" del brief: con pasos de una sola casilla,
// adyacente-y-avanza ES llegar.
void SimaActors_EnemyStepTarget(u8 floor, s8 ex, s8 ey, s8 px, s8 py, s8 *outX, s8 *outY)
{
    s8 dx = px - ex;
    s8 dy = py - ey;
    s8 adx = (dx < 0) ? -dx : dx;
    s8 ady = (dy < 0) ? -dy : dy;
    s8 stepX = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    s8 stepY = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;

    *outX = ex;
    *outY = ey;

    if (adx > ady && stepX != 0)
    {
        if (!SimaRoom_IsSolid(floor, ex + stepX, ey))
        {
            *outX = ex + stepX;
            return;
        }
        if (stepY != 0 && !SimaRoom_IsSolid(floor, ex, ey + stepY))
            *outY = ey + stepY;
        return;
    }

    if (stepY != 0)
    {
        if (!SimaRoom_IsSolid(floor, ex, ey + stepY))
        {
            *outY = ey + stepY;
            return;
        }
        if (stepX != 0 && !SimaRoom_IsSolid(floor, ex + stepX, ey))
            *outX = ex + stepX;
        return;
    }

    if (stepX != 0 && !SimaRoom_IsSolid(floor, ex + stepX, ey))
        *outX = ex + stepX;
}

// Función pura (rango de detección, tarea de sensación): ¿debería un enemigo
// a `manhattanDist` casillas del jugador PERSEGUIRLO este turno (TRUE,
// SimaActors_EnemyStepTarget de arriba) o DEAMBULAR (FALSE, EnemyWanderStep
// más abajo)? Deliberadamente separada del RNG que decide HACIA DÓNDE
// deambula -- esta función es pura y determinista (misma distancia, mismo
// resultado siempre), así que el harness in-ROM la puede ejercitar sin
// depender de una semilla concreta, igual que SimaActors_EnemyStepTarget.
// Quien llama (StartEnemyTurn, más abajo) calcula la distancia Manhattan
// real entre el enemigo y el jugador antes de preguntar.
bool8 SimaActors_EnemyShouldChase(u8 manhattanDist)
{
    return manhattanDist <= SIMA_ENEMY_DETECT_RANGE;
}

// Deambular (tarea de sensación): un paso a una casilla adyacente NO sólida
// elegida al azar, o quieto si las 4 están bloqueadas. A diferencia de
// SimaActors_EnemyStepTarget/EnemyShouldChase, esta función SÍ usa el RNG
// del juego (Random(), include/random.h) -- por eso se queda sin exponer en
// sima.h ni se testea en el harness in-ROM (no hay semilla determinista que
// comprobar ahí; lo que SÍ es determinista y se testea es la decisión
// perseguir-vs-deambular de arriba). Recorre las 4 direcciones cardinales,
// junta las que son transitables, y usa Random() % count para elegir entre
// ellas con la misma probabilidad -- no hace falta conocer el número de
// candidatas de antemano.
static void EnemyWanderStep(u8 floor, s8 ex, s8 ey, s8 *outX, s8 *outY)
{
    static const s8 sWanderDx[4] = {0, 0, -1, 1};
    static const s8 sWanderDy[4] = {-1, 1, 0, 0};
    s8 candX[4], candY[4];
    u8 count = 0, i;

    for (i = 0; i < 4; i++)
    {
        s8 nx = ex + sWanderDx[i];
        s8 ny = ey + sWanderDy[i];
        if (!SimaRoom_IsSolid(floor, nx, ny))
        {
            candX[count] = nx;
            candY[count] = ny;
            count++;
        }
    }

    if (count == 0)
    {
        // Las 4 casillas vecinas están bloqueadas: se queda quieto, igual
        // que SimaActors_EnemyStepTarget cuando los dos ejes lo están.
        *outX = ex;
        *outY = ey;
        return;
    }

    i = Random() % count;
    *outX = candX[i];
    *outY = candY[i];
}

// Coloca los enemigos del piso leyendo SimaRoom_GetEnemy. Sin guarda de
// idempotencia para LoadSpriteSheet (a diferencia de lo que advierte la
// nota de cabecera de este archivo sobre cargas dobles): igual que
// SimaActors_InitPlayer, esta función solo se llama UNA VEZ por sesión del
// modo (CB2_InitSima, case 1), justo después de que ResetSpriteData vacíe
// la tabla de sprites -- recargar aquí siempre es correcto, nunca una fuga.
void SimaActors_InitEnemies(u8 floor)
{
    u8 i, count;

    LoadSpriteSheet(&sSheet_SimaRat);
    LoadSpriteSheet(&sSheet_SimaBat);
    LoadSpriteSheet(&sSheet_SimaSlime);
    // Misma paleta que el jugador; LoadSpritePalette SÍ es idempotente por
    // tag (a diferencia de LoadSpriteSheet), así que si SimaActors_InitPlayer
    // ya la cargó esta llamada no hace nada.
    LoadSpritePalette(&sPal_SimaPlayer);

    sEnemyFloor = floor;
    count = SimaRoom_GetEnemyCount(floor);
    if (count > SIMA_MAX_ENEMIES)
        count = SIMA_MAX_ENEMIES;  // red de seguridad, ver el comentario de SIMA_MAX_ENEMIES
    sEnemyCount = count;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        if (i < count)
        {
            s8 ex, ey;
            SimaRoom_GetEnemy(floor, i, &ex, &ey);
            sEnemyX[i] = (s16)ex * SIMA_TILE_PX;
            sEnemyY[i] = (s16)ey * SIMA_TILE_PX;
            sEnemySpriteId[i] = CreateSprite(sEnemyTemplates[i % SIMA_ENEMY_KIND_COUNT],
                                              sEnemyX[i] + 8, sEnemyY[i] + 8, 1);
            // Si CreateSprite se queda sin presupuesto (MAX_SPRITES), este
            // slot no cuenta como vivo: ni bloquea la escalera para siempre
            // (sería peor que dejarla pasar) ni intenta animar un sprite que
            // no existe. No debería pasar con el presupuesto de esta tarea
            // (jugador + 3 enemigos + HUD, ver el brief), pero es la misma
            // guarda que ya usa SimaActors_InitPlayer con sPlayerActive.
            sEnemyAlive[i] = (sEnemySpriteId[i] != MAX_SPRITES);
        }
        else
        {
            sEnemyAlive[i] = FALSE;
        }
        // Reinicio explícito (Tarea 7), misma razón que sCurrentFloor en
        // CB2_InitSima: si el modo se remonta en la misma sesión de ROM
        // (PHANTOM_DEBUG_SIMA), un cadáver de la sesión anterior no debe
        // seguir "muriendo" en la nueva.
        sEnemyDeathTimer[i] = 0;
        sEnemyMoving[i] = FALSE;
    }

    sEnemyAnimTimer = 0;
    sEnemyAnimStep = 0;
    sEnemyStepTimer = 0;
    sPlayerHitThisTurn = FALSE;
}

// Repone a los enemigos en sus casillas de spawn del piso, todos vivos --
// llamada SOLO desde SimaActors_ResetAfterDeath, más abajo (ver el GANCHO
// grande junto a esa función). A diferencia de SimaActors_InitEnemies, esta
// función NO vuelve a llamar LoadSpriteSheet (nota de cabecera del archivo:
// no es idempotente) -- las hojas de rat/bat/slime ya están en VRAM desde
// que el modo se montó, y con SIMA_FLOOR_COUNT sin cambiar de piso aquí
// (siempre se muere y se repone en el MISMO piso) el conjunto de especies
// por slot tampoco cambia.
//
// Cada slot con enemigo (i < count) puede estar en uno de tres estados
// cuando esto se llama:
//   - vivo (sEnemyAlive[i] == TRUE): el sprite existe, solo hay que
//     reposicionarlo y asegurarse de que es visible.
//   - "cadáver" en curso (sEnemyDeathTimer[i] > 0): el sprite TAMBIÉN existe
//     todavía (SimaActors_UpdateEnemies solo lo destruye cuando el
//     cronómetro llega a 0) -- se reutiliza igual que el caso vivo.
//   - ya destruido del todo (sEnemyAlive[i] == FALSE Y sEnemyDeathTimer[i]
//     == 0): DestroySprite ya se llamó (ver SimaActors_UpdateEnemies) y
//     sEnemySpriteId[i] ya no apunta a nada válido -- hace falta CreateSprite
//     de nuevo. Esto SÍ es seguro sin repetir LoadSpriteSheet: CreateSprite
//     solo pide un hueco de OAM nuevo que referencia tiles que YA están
//     cargados por tag (ver sTmpl_Sima* / TAG_SIMA_RAT|BAT|SLIME).
static void ResetEnemiesAfterDeath(u8 floor)
{
    u8 i, count;

    count = SimaRoom_GetEnemyCount(floor);
    if (count > SIMA_MAX_ENEMIES)
        count = SIMA_MAX_ENEMIES;
    sEnemyCount = count;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        s8 ex, ey;

        if (i >= count)
        {
            sEnemyAlive[i] = FALSE;   // este slot nunca tuvo enemigo en este piso
            sEnemyDeathTimer[i] = 0;
            continue;
        }

        SimaRoom_GetEnemy(floor, i, &ex, &ey);
        sEnemyX[i] = (s16)ex * SIMA_TILE_PX;
        sEnemyY[i] = (s16)ey * SIMA_TILE_PX;
        sEnemyMoving[i] = FALSE;

        if (!sEnemyAlive[i] && sEnemyDeathTimer[i] == 0)
        {
            // Sprite ya destruido (ver el comentario de cabecera de esta
            // función): pedir uno nuevo.
            sEnemySpriteId[i] = CreateSprite(sEnemyTemplates[i % SIMA_ENEMY_KIND_COUNT],
                                              sEnemyX[i] + 8, sEnemyY[i] + 8, 1);
            sEnemyAlive[i] = (sEnemySpriteId[i] != MAX_SPRITES);
        }
        else
        {
            // Vivo, o cadáver con sprite todavía en pantalla: reutilizar.
            gSprites[sEnemySpriteId[i]].x = sEnemyX[i] + 8;
            gSprites[sEnemySpriteId[i]].y = sEnemyY[i] + 8;
            gSprites[sEnemySpriteId[i]].invisible = FALSE;
            gSprites[sEnemySpriteId[i]].oam.tileNum =
                gSprites[sEnemySpriteId[i]].sheetTileStart + ENEMY_FRAME_IDLE_A;
            sEnemyAlive[i] = TRUE;
        }
        sEnemyDeathTimer[i] = 0;
    }

    sEnemyAnimTimer = 0;
    sEnemyAnimStep = 0;
    sEnemyStepTimer = 0;
    sPlayerHitThisTurn = FALSE;
}

// ---------------------------------------------------------------------
// GANCHO -- fin del prólogo (pendiente; ver docs/superpowers/specs/2026-07-17
// -pokemon-phantom-design.md y MEMORY.md "intro-frontend-vision"). En el
// guion final, MORIR en SIMA es lo que termina el prólogo: la pantalla
// debería llevar al marcador con el récord del hermano (el mismo sistema de
// high score del shmup del intro), no reiniciar el piso en el que estabas.
//
// Ese marcador TODAVÍA NO EXISTE en el código, así que hoy la única salida
// razonable es reponer al jugador y a los enemigos en el piso actual -- pero
// se aísla aquí, en su propia función con su propio punto de llamada (ver
// src/sima.c, UpdateFloorTransition, caso SIMA_TRANS_FADE_OUT con
// sTransitionIsDeath == TRUE, marcado con el mismo comentario "GANCHO"),
// para que quien construya el marcador lo encuentre a la primera: sustituir
// (o encadenar tras) la llamada a SimaActors_ResetAfterDeath() por el corte
// a esa pantalla.
// ---------------------------------------------------------------------
void SimaActors_ResetAfterDeath(u8 floor)
{
    ResetPlayerAfterDeath();
    ResetEnemiesAfterDeath(floor);
}

// Arranca el turno de los enemigos: llamada UNA vez, en el frame exacto en
// que termina el paso o el golpe del jugador (ver UpdatePlayerSlide/UpdateAttack).
// Para cada enemigo vivo (ni muerto ni en pleno cadáver) calcula la
// distancia Manhattan al jugador y, con SimaActors_EnemyShouldChase, decide
// si este turno persigue (SimaActors_EnemyStepTarget, la persecución óptima
// de siempre) o deambula (EnemyWanderStep, un paso aleatorio -- tarea de
// sensación: "demasiado listo, siempre pegado"). Con la casilla de destino
// ya elegida (por el camino que sea), la decisión de qué HACER con ella es
// la misma de antes:
//   - destino == casilla del jugador  -> ataque: no se mueve, daña (una vez
//     por turno, sPlayerHitThisTurn) y empuja al jugador (StartPlayerKnockback).
//   - destino == su propia casilla    -> bloqueado (o deambulando quieto), se queda.
//   - cualquier otro destino          -> movimiento real: arranca su deslizamiento.
//
// Inmunidad por TURNOS (tarea de "damage feel"): `immuneThisTurn` se captura
// UNA vez, ANTES del bucle -- con SimaActors_ContactShouldDamage sobre el
// valor de sPlayerInvulnTurns tal como entra a este turno -- y se usa para
// los tres enemigos por igual (nunca se reevalúa a mitad del bucle). El
// contador se decrementa aquí mismo, también antes del bucle: cada llamada a
// StartEnemyTurn ES un turno de enemigos que se resuelve, así que "consumir
// un turno de protección" significa restar 1 la primera vez que se entra
// aquí con el contador todavía > 0. Si en este mismo turno un enemigo SÍ
// conecta (solo puede pasar cuando immuneThisTurn era FALSE al entrar),
// sPlayerInvulnTurns se vuelve a fijar a SIMA_HIT_INVULN_TURNS para los
// turnos que vengan -- nunca en el turno que se acaba de decrementar, así
// que un golpe siempre compra como mínimo SIMA_HIT_INVULN_TURNS turnos
// completos de protección real (verificado por memoria, ver el informe de
// esta tarea).
static void StartEnemyTurn(void)
{
    u8 i;
    s8 px = (s8)(sPlayerX / SIMA_TILE_PX);
    s8 py = (s8)(sPlayerY / SIMA_TILE_PX);
    bool8 immuneThisTurn = !SimaActors_ContactShouldDamage(sPlayerInvulnTurns);

    if (sPlayerInvulnTurns > 0)
        sPlayerInvulnTurns--;

    sPlayerHitThisTurn = FALSE;
    sEnemyStepTimer = 0;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        s8 ex, ey, nx, ny;
        s8 dx, dy;
        u8 dist;

        sEnemyMoving[i] = FALSE;

        if (sEnemyDeathTimer[i] > 0 || !sEnemyAlive[i])
            continue;   // cadáver en curso, o ya destruido: no le toca turno

        ex = (s8)(sEnemyX[i] / SIMA_TILE_PX);
        ey = (s8)(sEnemyY[i] / SIMA_TILE_PX);

        dx = px - ex;
        dy = py - ey;
        dist = (u8)((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));   // distancia Manhattan, en casillas

        if (SimaActors_EnemyShouldChase(dist))
            SimaActors_EnemyStepTarget(sEnemyFloor, ex, ey, px, py, &nx, &ny);
        else
            EnemyWanderStep(sEnemyFloor, ex, ey, &nx, &ny);

        if (nx == px && ny == py)
        {
            // Ataque: el paso del enemigo aterriza en la casilla del jugador.
            // No se mueve el sprite del enemigo -- solo el jugador retrocede,
            // vía knockback. Un solo golpe por turno (sPlayerHitThisTurn) Y,
            // con la inmunidad por TURNOS, ninguno mientras `immuneThisTurn`
            // (capturado ANTES del decremento de sPlayerInvulnTurns, arriba).
            // Si el golpe se descarta por cualquiera de las dos razones, el
            // enemigo se queda plantado ahí este turno (no avanza, no hace
            // nada) -- la misma regla "adyacente no te deja pasar" sin efecto.
            if (!sPlayerHitThisTurn && !immuneThisTurn)
            {
                sPlayerHP = SimaActors_ApplyDamage(sPlayerHP, SIMA_ENEMY_CONTACT_DAMAGE);
                sPlayerInvulnTurns = SIMA_HIT_INVULN_TURNS;   // proteccion por turnos (regla de juego)
                sPlayerInvulnTimer = SIMA_HIT_INVULN_FRAMES;  // reloj del flash de daño (solo visual)
                StartPlayerKnockback(ex, ey);
                PlaySE(SIMA_HIT_SE);   // sonido del golpe (tarea de "damage feel")
                sPlayerHitThisTurn = TRUE;
            }
        }
        else if (nx != ex || ny != ey)
        {
            sEnemyMoving[i] = TRUE;
            sEnemyTargetX[i] = (s16)nx * SIMA_TILE_PX;
            sEnemyTargetY[i] = (s16)ny * SIMA_TILE_PX;
            sEnemySlideDX[i] = (s16)(nx - ex) * SIMA_ENEMY_SLIDE_SPEED;
            sEnemySlideDY[i] = (s16)(ny - ey) * SIMA_ENEMY_SLIDE_SPEED;
        }
        // else: nx==ex && ny==ey -> bloqueado en ambos ejes, se queda quieto
        // (sEnemyMoving[i] ya es FALSE).
    }

    // Encadenado hitstop -> (daño | muerte) de la tarea de "damage feel": si
    // algún enemigo conectó este turno, TODO se congela SIMA_HITSTOP_FRAMES
    // (SIMA_TURN_HITSTOP) ANTES de reproducir el empujón/deslizamientos ya
    // calculados arriba. sPendingDeath decide a dónde salta AdvanceHitstop al
    // terminar: golpe mortal -> SIMA_TURN_PLAYER_DYING (primero la reacción de
    // daño, DESPUÉS la muerte), golpe normal -> SIMA_TURN_ENEMY_STEP (sigue el
    // turno de los enemigos). Si NADIE golpeó, se va directo al turno de los
    // enemigos, sin frenazo.
    if (sPlayerHitThisTurn)
    {
        sHitstopTimer = SIMA_HITSTOP_FRAMES;
        sPendingDeath = SimaActors_IsPlayerDead();
        sTurnPhase = SIMA_TURN_HITSTOP;
    }
    else
    {
        sTurnPhase = SIMA_TURN_ENEMY_STEP;
    }
}

// Avanza UN frame del deslizamiento de los enemigos durante su turno
// (SIMA_TURN_ENEMY_STEP, ver SimaActors_UpdateEnemies, que es quien la llama).
// Al cumplirse SIMA_ENEMY_SLIDE_FRAMES hace snap exacto a la casilla destino y
// devuelve el turno al jugador (SIMA_TURN_PLAYER_INPUT).
static void AdvanceEnemyStepPhase(void)
{
    u8 i;

    sEnemyStepTimer++;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        if (!sEnemyMoving[i])
            continue;
        sEnemyX[i] += sEnemySlideDX[i];
        sEnemyY[i] += sEnemySlideDY[i];
    }

    if (sEnemyStepTimer >= SIMA_ENEMY_SLIDE_FRAMES)
    {
        for (i = 0; i < SIMA_MAX_ENEMIES; i++)
        {
            if (sEnemyMoving[i])
            {
                sEnemyX[i] = sEnemyTargetX[i];   // snap exacto, sin arrastrar redondeo
                sEnemyY[i] = sEnemyTargetY[i];
                sEnemyMoving[i] = FALSE;
            }
        }
        sTurnPhase = SIMA_TURN_PLAYER_INPUT;   // el turno vuelve al jugador
    }
}

void SimaActors_UpdateEnemies(void)
{
    u8 i;
    // Golpe del jugador (Tarea 7): se calcula UNA vez por frame, no por
    // enemigo. Se comprueba TODOS los frames en que el arma está activa
    // (SIMA_TURN_PLAYER_ATTACK, antes de que le toque mover a nadie) -- igual
    // que en la versión en tiempo real. Con turnos, la comparación es EXACTA
    // por casilla (hitX/hitY y sEnemyX[i]/sEnemyY[i] son siempre múltiplos de
    // SIMA_TILE_PX aquí: el arma solo se activa con el jugador quieto en
    // SIMA_TURN_PLAYER_ATTACK, y ningún enemigo puede estar a medio deslizar
    // mientras tanto porque son fases mutuamente excluyentes).
    bool8 attackHit = AttackHitboxActive();
    s16 hitX = 0, hitY = 0;

    if (attackHit)
        SimaActors_WeaponHitbox(sAttackFacing, sPlayerX, sPlayerY, &hitX, &hitY);

    // Reloj visual del flash de daño: se decrementa aquí en las fases "vivas".
    // NO durante SIMA_TURN_HITSTOP (todo congelado, ni el reloj del golpe
    // avanza -- ver el comentario junto a SIMA_HITSTOP_FRAMES) ni durante
    // SIMA_TURN_PLAYER_DYING (ahí lo decrementa UpdatePlayerDying; no contarlo
    // dos veces en el mismo frame -- ver su comentario).
    if (sTurnPhase != SIMA_TURN_HITSTOP && sTurnPhase != SIMA_TURN_PLAYER_DYING
        && sPlayerInvulnTimer > 0)
        sPlayerInvulnTimer--;

    // Respiración (cosmética): su propio reloj, siempre corriendo, no ligado
    // al turno -- ver el comentario grande al principio de esta sección.
    sEnemyAnimTimer++;
    if (sEnemyAnimTimer >= SIMA_ENEMY_ANIM_PERIOD)
    {
        sEnemyAnimTimer = 0;
        sEnemyAnimStep ^= 1;
    }

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        struct Sprite *sprite = &gSprites[sEnemySpriteId[i]];

        // Cadáver en curso (Tarea 7): no se mueve, no daña por contacto, solo
        // anima la muerte y cuenta atrás hasta que toca destruir el sprite.
        // Va ANTES del guard de sEnemyAlive porque un enemigo muriendo ya
        // tiene sEnemyAlive en FALSE (ver más abajo) pero su sprite sigue
        // vivo unos frames más.
        if (sEnemyDeathTimer[i] > 0)
        {
            sEnemyDeathTimer[i]--;
            sprite->oam.tileNum = sprite->sheetTileStart +
                (((sEnemyDeathTimer[i] / SIMA_ENEMY_DEATH_ANIM_PERIOD) & 1)
                     ? ENEMY_FRAME_DEATH_A : ENEMY_FRAME_DEATH_B);
            if (sEnemyDeathTimer[i] == 0)
                DestroySprite(sprite);
            continue;
        }

        if (!sEnemyAlive[i])
            continue;  // ya destruido del todo (o nunca llegó a existir, ver SimaActors_InitEnemies)

        // Golpe del arma: un enemigo tocado muere de un golpe (ver el
        // comentario junto a SIMA_ENEMY_DEATH_FRAMES sobre por qué un solo
        // golpe y no una barra de vida). sEnemyAlive baja a FALSE AQUÍ
        // MISMO -- no cuando termina la animación -- para que
        // SimaActors_StairsUnlocked/GetAliveEnemyCount reaccionen en el
        // frame exacto del golpe.
        if (attackHit && hitX == sEnemyX[i] && hitY == sEnemyY[i])
        {
            sEnemyAlive[i] = FALSE;
            sEnemyDeathTimer[i] = SIMA_ENEMY_DEATH_FRAMES;
            continue;
        }

        sprite->oam.tileNum = sprite->sheetTileStart +
            (sEnemyAnimStep ? ENEMY_FRAME_IDLE_B : ENEMY_FRAME_IDLE_A);
    }

    // Movimiento por turnos: SOLO avanza mientras estamos en el turno de los
    // enemigos (ver StartEnemyTurn, llamada al terminar el turno del
    // jugador). El resto de las fases, este bloque no hace nada -- por eso
    // "sin pulsar nada nadie se mueve" (verificado por memoria, ver el
    // informe de esta tarea).
    if (sTurnPhase == SIMA_TURN_ENEMY_STEP)
        AdvanceEnemyStepPhase();

    // Sincroniza la posición en pantalla de cada enemigo vivo que no esté en
    // pleno cadáver -- hecho DESPUÉS de un posible avance de posición en
    // este mismo frame, para que se vea de inmediato y no un frame tarde.
    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        if (sEnemyDeathTimer[i] > 0 || !sEnemyAlive[i])
            continue;
        gSprites[sEnemySpriteId[i]].x = sEnemyX[i] + 8;
        gSprites[sEnemySpriteId[i]].y = sEnemyY[i] + 8;
    }
}

// Función pura (colisión jugador-enemigo, reconstrucción tras el apagón --
// existía antes, se perdió; sin ella el jugador podía caminar ENCIMA de un
// enemigo, sprites superpuestos y un golpe sin telégrafo, ver el informe de
// esta tarea): ¿la casilla (x, y) coincide con la casilla de un enemigo VIVO
// (enemyX, enemyY, enemyAlive)? Separada de sEnemyX/sEnemyY/sEnemyAlive (el
// estado real, ver más abajo) para que el harness in-ROM pueda comprobar la
// frontera exacta -- vivo bloquea, muerto no -- sin sprites de por medio,
// mismo espíritu que SimaActors_ContactShouldDamage/PlayerStepTarget. Un
// enemigo MUERTO (enemyAlive == FALSE, aunque su cadáver siga en pantalla
// unos frames más, ver sEnemyDeathTimer) deliberadamente NO bloquea: ya no
// hay nada sólido en esa casilla, solo un efecto cosmético.
bool8 SimaActors_TileMatchesEnemy(s8 x, s8 y, s8 enemyX, s8 enemyY, bool8 enemyAlive)
{
    return enemyAlive && x == enemyX && y == enemyY;
}

// Helper NO puro (a diferencia de la función de arriba): ¿hay algún enemigo
// VIVO de los SIMA_MAX_ENEMIES slots en la casilla (x, y) del piso actual?
// Aplica SimaActors_TileMatchesEnemy a cada uno -- necesita leer
// sEnemyX/sEnemyY/sEnemyAlive (estado real, no expuesto al harness), por eso
// se queda sin declarar en sima.h, a diferencia de la función pura. Llamada
// desde UpdatePlayerInput (bloquear el paso del jugador) y
// StartPlayerKnockback (no empujar al jugador encima de OTRO enemigo) --
// ambas viven antes en el archivo, de ahí la forward-declaration junto al
// resto de prototipos estáticos.
static bool8 TileHasLiveEnemy(s8 x, s8 y)
{
    u8 i;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
    {
        s8 ex = (s8)(sEnemyX[i] / SIMA_TILE_PX);
        s8 ey = (s8)(sEnemyY[i] / SIMA_TILE_PX);

        if (SimaActors_TileMatchesEnemy(x, y, ex, ey, sEnemyAlive[i]))
            return TRUE;
    }
    return FALSE;
}

u8 SimaActors_GetAliveEnemyCount(void)
{
    u8 i, count = 0;

    for (i = 0; i < SIMA_MAX_ENEMIES; i++)
        if (sEnemyAlive[i])
            count++;

    return count;
}

// Pura, sin sprites (Tarea 6, cambio de diseño): la escalera está cerrada
// mientras quede algún enemigo vivo, abierta con 0. Aislada en su propia
// función de una línea a propósito -- la decisión tomada HOY es que la
// escalera APARECE DE GOLPE al morir el último (no "visible pero apagada"
// hasta entonces); si eso cambia, este es el único sitio a tocar, junto con
// UpdateStairsVisibility en src/sima.c (que decide CUÁNDO repintar la
// celda a partir de lo que esta función responde, no CÓMO se ve).
//
// Es también lo que hace posible el jefe invencible del piso 3 (Tarea 8,
// fuera de alcance aquí): si su enemigo nunca muere, esta función nunca
// devuelve TRUE para ese piso y su escalera no aparece jamás.
bool8 SimaActors_StairsUnlocked(u8 aliveEnemyCount)
{
    return aliveEnemyCount == 0;
}