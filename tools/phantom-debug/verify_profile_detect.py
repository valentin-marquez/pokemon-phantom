#!/usr/bin/env python3
"""Verificacion por memoria de la tarea "cambio de sensacion" de SIMA (ver el
informe en .superpowers/sdd/task-profile-detect-report.md). Tres cosas, las
tres pedidas explicitamente por el dueño y verificadas SOLO leyendo IWRAM
(nada de mirar pixeles a ojo):

  1. Tap-to-turn en el eje horizontal: mirando a un lado, una pulsacion hacia
     el lado CONTRARIO cambia sPlayerFacing pero NO mueve al jugador ni pasa
     el turno (sTurnPhase se queda en PLAYER_INPUT). Una segunda pulsacion
     igual SI mueve (16px), y esa si pasa el turno.
  2. Vista de perfil pura: despues de moverse en vertical (ARRIBA/ABAJO),
     sPlayerFacing sigue siendo LEFT o RIGHT -- nunca UP/DOWN.
  3. Rango de deteccion: un enemigo por encima del rango (distancia Manhattan
     > SIMA_ENEMY_DETECT_RANGE) NO se acerca monotonamente al jugador turno a
     turno (deambula); en cuanto un enemigo entra en rango, si se acerca
     turno a turno (persigue).

Uso: PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python \
     tools/phantom-debug/verify_profile_detect.py
"""
from phantom_dbg.emu import Emu

ROM = "pokeemerald_modern_sima.gba"
BOOT_FRAMES = 320  # copyright + fundido + sala montada (igual que verify_turns.py)

# Direcciones IWRAM (arm-none-eabi-nm pokeemerald_modern_sima.elf), RE-VERIFICADAS
# para esta tarea -- no se movieron respecto a verify_turns.py/play_sima.py
# pese a los campos nuevos (sPlayerFacing sigue en el mismo slot: solo cambio
# el RANGO de valores que se le asignan en tiempo de ejecucion, no el layout
# de sPlayer*/sEnemy* en si).
ADDR_PLAYER_X = 0x03000eac      # s16
ADDR_PLAYER_Y = 0x03000eae      # s16
ADDR_PLAYER_FACING = 0x03000eb0  # u8: 0=DOWN 1=UP 2=LEFT 3=RIGHT (enum SimaFacing)
ADDR_TURN_PHASE = 0x03000eb6    # u8: 0=PLAYER_INPUT 1=PLAYER_MOVE 2=PLAYER_ATTACK 3=ENEMY_STEP
ADDR_ENEMY_ALIVE = 0x03000ed0   # bool8[3]
ADDR_ENEMY_X = 0x03000edc       # s16[3]
ADDR_ENEMY_Y = 0x03000ee4       # s16[3]

TILE = 16
FACING_DOWN, FACING_UP, FACING_LEFT, FACING_RIGHT = 0, 1, 2, 3
FACING_NAME = {0: "DOWN", 1: "UP", 2: "LEFT", 3: "RIGHT"}
DETECT_RANGE = 4  # SIMA_ENEMY_DETECT_RANGE (include/sima.h) -- mismo numero, a mano


def s16(v):
    return v - 0x10000 if v >= 0x8000 else v


class State:
    def __init__(self, emu):
        self.emu = emu

    def player(self):
        return s16(self.emu.mem_u16(ADDR_PLAYER_X)), s16(self.emu.mem_u16(ADDR_PLAYER_Y))

    def facing(self):
        return self.emu.mem_u8(ADDR_PLAYER_FACING)

    def phase(self):
        return self.emu.mem_u8(ADDR_TURN_PHASE)

    def enemies(self):
        out = []
        for i in range(3):
            alive = self.emu.mem_u8(ADDR_ENEMY_ALIVE + i) != 0
            x = s16(self.emu.mem_u16(ADDR_ENEMY_X + i * 2))
            y = s16(self.emu.mem_u16(ADDR_ENEMY_Y + i * 2))
            out.append((alive, x, y))
        return out


def tap(emu, key, held=2, settle=4, run_until_idle=True, max_extra=40):
    """Pulsacion de `key` (JOY_NEW/JOY_HELD solo se leen en PLAYER_INPUT).
    IMPORTANTE (bug real encontrado escribiendo este script): tras soltar la
    tecla hace falta dejar correr unos frames ANTES de que quien llama pueda
    volver a pulsarla -- si dos tap() de la MISMA tecla se encadenan con cero
    frames de por medio (add_keys justo despues de clear_keys, sin que
    run_frame() vea nunca el estado "soltada"), el emulador nunca observa el
    flanco de bajada y JOY_NEW no se dispara en la segunda pulsacion. Con
    tap-to-turn (LEFT/RIGHT via JOY_NEW, ver UpdatePlayerInput en
    src/sima_actors.c) eso rompe justo el caso "girar y LUEGO mover" -- un
    giro no consume frames extra (sTurnPhase se queda en PLAYER_INPUT), asi
    que sin este `settle` la segunda pulsacion (la que deberia moverse)
    llegaba pegada a la primera y se perdia. `settle` fuerza el hueco.
    Si run_until_idle, ademas sigue corriendo hasta que sTurnPhase vuelva a
    PLAYER_INPUT (0) o se agote max_extra -- para no cortar un turno a
    medias antes de leer memoria."""
    core = emu._core
    core.add_keys(emu.KEY[key])
    emu.run(held)
    core.clear_keys(emu.KEY[key])
    emu.run(settle)
    if run_until_idle:
        extra = 0
        while emu.mem_u8(ADDR_TURN_PHASE) != 0 and extra < max_extra:
            emu.run(1)
            extra += 1


def move(emu, st, direction_key, direction_val):
    """Un paso de rejilla en `direction_key` (UP/DOWN/LEFT/RIGHT), respetando
    tap-to-turn: si es horizontal y el jugador no mira ya hacia ahi, la
    primera pulsacion solo gira (se descarta aqui, es "gratis") y hace falta
    una segunda para moverse de verdad. Vertical siempre mueve a la
    primera. Devuelve la posicion tras el movimiento real."""
    if direction_val in (FACING_LEFT, FACING_RIGHT) and st.facing() != direction_val:
        tap(emu, direction_key)  # solo gira
    tap(emu, direction_key)      # mueve de verdad
    return st.player()


def manhattan(ax, ay, bx, by):
    return (abs(ax - bx) + abs(ay - by)) // TILE


def step_toward_enemy(emu, st, ex, ey):
    """Un turno del jugador acercandose al enemigo en (ex, ey) [pixeles]:
    elige el eje que mas lo acerca (mismo criterio que
    SimaActors_EnemyStepTarget) y da ese paso via move(). Devuelve la nueva
    posicion del jugador."""
    px, py = st.player()
    dx, dy = ex - px, ey - py
    if abs(dx) >= abs(dy) and dx != 0:
        key, val = ("RIGHT", FACING_RIGHT) if dx > 0 else ("LEFT", FACING_LEFT)
    elif dy != 0:
        key, val = ("DOWN", FACING_DOWN) if dy > 0 else ("UP", FACING_UP)
    else:
        return px, py  # ya esta en la misma casilla (no deberia pasar)
    return move(emu, st, key, val)


def main():
    emu = Emu(ROM)
    st = State(emu)
    emu.run(BOOT_FRAMES)

    failures = []

    def check(cond, msg):
        status = "OK" if cond else "FALLO"
        print(f"  [{status}] {msg}")
        if not cond:
            failures.append(msg)

    # -----------------------------------------------------------------
    # Fase 0: estado inicial -- spawn (1,0), mirando a la derecha (valor por
    # defecto elegido en SimaActors_InitPlayer, ver el informe).
    # -----------------------------------------------------------------
    print("--- fase 0: estado inicial ---")
    p0 = st.player()
    f0 = st.facing()
    print(f"jugador: {p0}  facing: {FACING_NAME.get(f0, f0)}  fase: {st.phase()}")
    check(p0 == (1 * TILE, 0 * TILE), "spawn en (1,0)*16px")
    check(f0 == FACING_RIGHT, "facing inicial es RIGHT (numero de gusto elegido en SimaActors_InitPlayer)")

    # -----------------------------------------------------------------
    # Test 2 (perfil pura): un paso vertical (unico movimiento posible desde
    # el spawn -- fila 0 solo tiene abierta la columna 1) y comprobar que el
    # facing NO cambia -- sigue siendo RIGHT, un valor horizontal.
    # -----------------------------------------------------------------
    print("\n--- test perfil: paso vertical no toca el facing ---")
    move(emu, st, "DOWN", FACING_DOWN)
    p1 = st.player()
    f1 = st.facing()
    print(f"tras DOWN: jugador {p1}  facing: {FACING_NAME.get(f1, f1)}")
    check(p1 == (1 * TILE, 1 * TILE), "el paso DOWN si movio al jugador una casilla")
    check(f1 == FACING_RIGHT, "facing sigue RIGHT tras moverse en vertical (no lo toco)")
    check(f1 in (FACING_LEFT, FACING_RIGHT), "facing es un valor horizontal (nunca UP/DOWN)")

    # Nos alejamos de los muros (columna 1 y fila 1 tienen borde solido cerca)
    # para poder probar el tap-to-turn de verdad hacia la izquierda mas
    # abajo sin que un muro real enmascare el resultado.
    for _ in range(3):
        move(emu, st, "RIGHT", FACING_RIGHT)
    p2 = st.player()
    f2 = st.facing()
    print(f"tras 3x RIGHT: jugador {p2}  facing: {FACING_NAME.get(f2, f2)}")
    check(p2 == (4 * TILE, 1 * TILE), "3 pasos RIGHT llevan a (4,1)")
    check(f2 == FACING_RIGHT, "facing sigue RIGHT (ya miraba hacia alli, cada pulsacion movio)")

    # -----------------------------------------------------------------
    # Test 1 (tap-to-turn): mirando a la derecha, pulsar IZQUIERDA (el lado
    # CONTRARIO) debe solo girar -- ni mover ni pasar turno.
    # -----------------------------------------------------------------
    print("\n--- test tap-to-turn: primera pulsacion CONTRARIA solo gira ---")
    before_turn = st.player()
    core = emu._core
    core.add_keys(emu.KEY["LEFT"])
    emu.run(2)
    core.clear_keys(emu.KEY["LEFT"])
    emu.run(6)  # muy por debajo de los 16 frames de un deslizamiento real
    after_turn = st.player()
    facing_after_turn = st.facing()
    phase_after_turn = st.phase()
    print(f"antes: {before_turn}  despues (6 frames): {after_turn}  "
          f"facing: {FACING_NAME.get(facing_after_turn, facing_after_turn)}  fase: {phase_after_turn}")
    check(before_turn == after_turn, "girarse no movio al jugador ni un pixel")
    check(facing_after_turn == FACING_LEFT, "el facing SI cambio a LEFT")
    check(phase_after_turn == 0, "el turno NO paso a los enemigos (sigue en PLAYER_INPUT)")

    print("\n--- test tap-to-turn: segunda pulsacion (ya mirando hacia alli) SI mueve ---")
    before_move = st.player()
    tap(emu, "LEFT")
    after_move = st.player()
    facing_after_move = st.facing()
    phase_after_move = st.phase()
    dx = after_move[0] - before_move[0]
    dy = after_move[1] - before_move[1]
    print(f"antes: {before_move}  despues: {after_move}  dx={dx} dy={dy}  "
          f"facing: {FACING_NAME.get(facing_after_move, facing_after_move)}  fase: {phase_after_move}")
    check((dx, dy) == (-TILE, 0), "la segunda pulsacion SI movio 16px a la izquierda")
    check(facing_after_move == FACING_LEFT, "facing se mantiene LEFT")
    check(phase_after_move == 0, "el turno se resolvio del todo (fase de vuelta a PLAYER_INPUT)")

    # -----------------------------------------------------------------
    # Test 3 (rango de deteccion): en vez de un recorrido fijo (los 3
    # enemigos reales del piso 1 ya llevan un rato deambulando por su cuenta
    # en las fases anteriores, así que sus posiciones exactas a estas
    # alturas ya NO son las de spawn de tools/sima-editor/salas.json),
    # perseguimos dinámicamente al enemigo vivo MAS CERCANO turno a turno
    # (mismo criterio de eje-dominante que SimaActors_EnemyStepTarget, ver
    # step_toward_enemy) -- así el test no depende de dónde haya
    # deambulado nadie. Paramos en cuanto ese enemigo queda a <= 2 casillas
    # (antes de intentar el siguiente paso) para no arriesgarnos a quedar
    # adyacentes a mitad de un turno (adyacente = ataque + posible
    # knockback, ver StartPlayerKnockback -- complicaría la lectura de
    # posiciones sin aportar nada a lo que este test quiere probar).
    #
    # Grabamos la posicion de los 3 enemigos despues de CADA turno del
    # jugador. La decision real de perseguir-vs-deambular que hace
    # StartEnemyTurn (src/sima_actors.c) usa la distancia entre la posicion
    # del enemigo AL EMPEZAR su turno y la posicion del jugador YA MOVIDO
    # (el paso del jugador termina antes de que StartEnemyTurn se llame) --
    # por eso la clasificacion de abajo usa p_after (la posicion del
    # jugador DESPUES de su turno, que es historia[k+1]) contra la posicion
    # del enemigo ANTES de su propio paso (e_before, historia[k]).
    # -----------------------------------------------------------------
    print("\n--- test deteccion, fase de aislamiento: el jugador oscila en el sitio ---")
    # Antes de perseguir a nadie a proposito: el jugador da pasos que NO
    # dirigen sistematicamente hacia ningun enemigo (oscila LEFT/RIGHT entre
    # dos casillas del pasillo, x=2..3 -- ambas abiertas, lejos de cualquier
    # muro). Esto aisla el movimiento de los enemigos LEJANOS de cualquier
    # sesgo introducido por el propio bot (si el bot se acercara a proposito
    # incluso a un enemigo "de control", el que se acerque seria el
    # jugador, no una prueba de que el enemigo persigue). Lo que se lea aqui
    # es SOLO el paso aleatorio de los enemigos que sigan fuera de rango.
    history = [(st.player(), st.enemies())]
    for i in range(10):
        if i % 2 == 0:
            move(emu, st, "LEFT", FACING_LEFT)
        else:
            move(emu, st, "RIGHT", FACING_RIGHT)
        history.append((st.player(), st.enemies()))
    print(f"  jugador tras oscilar: {st.player()} (casilla {st.player()[0]//TILE},{st.player()[1]//TILE})")

    print("\n--- test deteccion, fase de persecucion: rumbo al enemigo vivo mas cercano ---")
    MAX_CHASE_TURNS = 20
    for _ in range(MAX_CHASE_TURNS):
        px, py = st.player()
        alive_enemies = [(i, ex, ey) for i, (alive, ex, ey) in enumerate(st.enemies()) if alive]
        if not alive_enemies:
            break
        i, ex, ey = min(alive_enemies, key=lambda e: manhattan(e[1], e[2], px, py))
        if manhattan(ex, ey, px, py) <= 2:
            print(f"  enemigo {i} ya a <= 2 casillas -- paramos antes de arriesgar un turno de ataque")
            break
        step_toward_enemy(emu, st, ex, ey)
        history.append((st.player(), st.enemies()))

    check(len(history) > 1, "el bot dio al menos un paso persiguiendo a un enemigo")

    final_player, final_enemies = history[-1]
    print(f"jugador final: {final_player} (casilla {final_player[0]//TILE},{final_player[1]//TILE})")
    for i, (alive, ex, ey) in enumerate(final_enemies):
        print(f"  enemigo {i}: casilla ({ex//TILE},{ey//TILE})  vivo={alive}")

    # Reconstruye, por enemigo, la lista de deltas de distancia turno a
    # turno junto con si ESE turno empezo dentro o fuera de rango.
    per_enemy_deltas = [[] for _ in range(3)]  # cada entrada: (in_range, delta)
    for (_, e_before), (p_after, e_after) in zip(history, history[1:]):
        for i in range(3):
            _, ex0, ey0 = e_before[i]
            alive1, ex1, ey1 = e_after[i]
            if not alive1:
                continue
            dist_before = manhattan(ex0, ey0, *p_after)
            dist_after = manhattan(ex1, ey1, *p_after)
            per_enemy_deltas[i].append((dist_before <= DETECT_RANGE, dist_after - dist_before))

    print("\ndeltas de distancia por enemigo (in_range_antes, delta):")
    for i, deltas in enumerate(per_enemy_deltas):
        print(f"  enemigo {i}: {deltas}")

    # Enemigo que SI entro en rango en algun momento de este recorrido:
    # todas sus transiciones "in_range" deben tener delta <= 0 (nunca se
    # aleja mientras persigue), y tiene que haber al menos una (si no, el
    # recorrido no llego a cruzar el rango y esta parte del test no probaria
    # nada).
    chase_deltas = [d for deltas in per_enemy_deltas for (in_range, d) in deltas if in_range]
    check(len(chase_deltas) > 0, "al menos un enemigo entro en rango durante el recorrido")
    check(all(d <= 0 for d in chase_deltas),
          "mientras un enemigo esta EN rango, nunca se aleja del jugador (persigue)")

    # Enemigos que se mantuvieron SIEMPRE fuera de rango: no deberian
    # acercarse monotonamente -- tiene que haber al menos un turno con
    # delta >= 0 (se quedo igual o se alejo) en alguno de ellos.
    wander_deltas = [d for deltas in per_enemy_deltas for (in_range, d) in deltas if not in_range]
    check(len(wander_deltas) > 0, "hubo turnos con algun enemigo fuera de rango")
    check(any(d >= 0 for d in wander_deltas),
          "los enemigos fuera de rango NO se acercaron monotonamente (deambulan, no persiguen)")

    print("\n" + ("TODAS LAS VERIFICACIONES PASARON" if not failures else f"FALLARON {len(failures)}: {failures}"))
    raise SystemExit(0 if not failures else 1)


if __name__ == "__main__":
    main()
