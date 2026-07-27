#!/usr/bin/env python3
"""Verificacion por memoria de la tarea "mantener para caminar" de SIMA (ver
el informe en .superpowers/sdd/task-hold-walk-report.md).

El pedido del dueño del proyecto: "deberiamos poder caminar al mantener
direcciones... se siente muy rigido el no podernos mover". Antes, caminar de
lado exigia soltar y volver a pulsar por cada casilla (JOY_NEW); arriba/abajo
si se podian mantener (JOY_HELD). El arreglo: un margen de giro
(SIMA_TURN_GRACE_FRAMES, include/sima.h) que separa "toque corto = solo
apuntar" de "mantener pulsado = caminar", en las 4 direcciones.

Cuatro cosas, leidas SOLO de IWRAM (nada de mirar pixeles a ojo):
  1. Toque corto (< margen) hacia el lado contrario: gira, el jugador NO se
     mueve (0px), la fase vuelve a PLAYER_INPUT.
  2. Mantener hacia el lado contrario: primero gira (gratis), y pasado el
     margen empieza a caminar, encadenando varias casillas sin soltar.
  3. Mantener hacia el lado que YA se mira: camina de inmediato (sin
     margen) y encadena.
  4. Los enemigos siguen dando su paso ENTRE casilla y casilla del jugador
     mientras este mantiene pulsado (mantener no les roba el turno).

Uso: PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python \
     tools/phantom-debug/verify_hold_walk.py
"""
from phantom_dbg.emu import Emu

ROM = "pokeemerald_modern_sima.gba"
BOOT_FRAMES = 320  # copyright + fundido + sala montada (igual que los otros verify_*.py)

# Direcciones IWRAM (arm-none-eabi-nm pokeemerald_modern_sima.elf | grep
# sPlayer/sTurn/sEnemy), RE-OBTENIDAS para esta tarea -- sTurnGraceActive/
# sTurnGraceTimer son estaticos NUEVOS insertados junto a sPlayerFacing en
# src/sima_actors.c, asi que TODO lo que vive despues de ellos (empezando
# por sTurnPhase) se corrio respecto a los scripts anteriores
# (verify_turns.py / verify_profile_detect.py) -- no reusar esas direcciones.
ADDR_PLAYER_X = 0x03000eac          # s16
ADDR_PLAYER_Y = 0x03000eae          # s16
ADDR_PLAYER_FACING = 0x03000eb0     # u8: 0=DOWN 1=UP 2=LEFT 3=RIGHT (enum SimaFacing)
ADDR_TURN_GRACE_ACTIVE = 0x03000eb2  # bool8
ADDR_TURN_GRACE_TIMER = 0x03000eb3   # u8
ADDR_TURN_PHASE = 0x03000ebc        # u8: 0=PLAYER_INPUT 1=PLAYER_MOVE 2=PLAYER_ATTACK 3=ENEMY_STEP 4=PLAYER_DEAD 5=PLAYER_TELEPORT
ADDR_ENEMY_ALIVE = 0x03000ed8       # bool8[3]
ADDR_ENEMY_X = 0x03000ee4           # s16[3]
ADDR_ENEMY_Y = 0x03000eec           # s16[3]

TILE = 16
FACING_DOWN, FACING_UP, FACING_LEFT, FACING_RIGHT = 0, 1, 2, 3
FACING_NAME = {0: "DOWN", 1: "UP", 2: "LEFT", 3: "RIGHT"}
PHASE_INPUT, PHASE_MOVE, PHASE_ATTACK, PHASE_ENEMY, PHASE_DEAD, PHASE_TELEPORT = range(6)
GRACE_FRAMES = 8  # SIMA_TURN_GRACE_FRAMES (include/sima.h) -- mismo numero, a mano


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

    def grace_active(self):
        return self.emu.mem_u8(ADDR_TURN_GRACE_ACTIVE)

    def grace_timer(self):
        return self.emu.mem_u8(ADDR_TURN_GRACE_TIMER)

    def enemies(self):
        out = []
        for i in range(3):
            alive = self.emu.mem_u8(ADDR_ENEMY_ALIVE + i) != 0
            x = s16(self.emu.mem_u16(ADDR_ENEMY_X + i * 2))
            y = s16(self.emu.mem_u16(ADDR_ENEMY_Y + i * 2))
            out.append((alive, x, y))
        return out


def run_until_idle(emu, max_extra=80):
    """Corre hasta que sTurnPhase vuelva a PLAYER_INPUT (0) o se agote
    max_extra -- para no leer memoria a mitad de un deslizamiento. Solo
    tiene sentido cuando NINGUNA tecla de movimiento sigue pulsada (si se
    sigue manteniendo, el juego reencadena a MOVE en el mismo frame en que
    vuelve a INPUT y esta funcion nunca ve un 0 -- ver el bucle de muestreo
    de la fase 2/3, mas abajo, que NO usa esta funcion mientras mantiene)."""
    extra = 0
    while emu.mem_u8(ADDR_TURN_PHASE) != PHASE_INPUT and extra < max_extra:
        emu.run(1)
        extra += 1


def main():
    emu = Emu(ROM)
    st = State(emu)
    emu.run(BOOT_FRAMES)
    core = emu._core

    failures = []

    def check(cond, msg):
        status = "OK" if cond else "FALLO"
        print(f"  [{status}] {msg}")
        if not cond:
            failures.append(msg)

    print("--- fase 0: estado inicial ---")
    p0 = st.player()
    print(f"jugador: {p0}  facing: {FACING_NAME[st.facing()]}  fase: {st.phase()}")
    check(p0 == (1 * TILE, 0 * TILE), "spawn en (1,0)*16px")
    check(st.facing() == FACING_RIGHT, "facing inicial RIGHT")

    # Bajamos una casilla para entrar al corredor horizontal abierto (fila
    # 1, columnas 1..10 -- ver sRoomSolid en src/sima_rooms_data.h). DOWN es
    # vertical: mueve de inmediato y no toca el facing.
    core.add_keys(emu.KEY["DOWN"])
    emu.run(2)
    core.clear_keys(emu.KEY["DOWN"])
    run_until_idle(emu)
    p_corridor = st.player()
    print(f"tras bajar al corredor: {p_corridor}  facing: {FACING_NAME[st.facing()]}  fase: {st.phase()}")
    check(p_corridor == (1 * TILE, 1 * TILE), "en el corredor, casilla (1,1)")
    check(st.facing() == FACING_RIGHT, "el paso vertical no toco el facing")

    # -----------------------------------------------------------------
    # PUNTO 3: mantener hacia el lado que YA se mira (RIGHT, ya facing
    # RIGHT) -- debe caminar de inmediato (sin margen, sTurnGraceActive
    # nunca se activa) y encadenar varias casillas mientras se mantiene.
    # De paso, esto nos deja mas adentro del corredor con sitio de sobra
    # para el punto 2 (que camina hacia la izquierda).
    # -----------------------------------------------------------------
    print("\n--- punto 3: mantener hacia el lado que YA se mira -> camina de inmediato y encadena ---")
    p_before_walk3 = st.player()
    core.add_keys(emu.KEY["RIGHT"])
    emu.run(1)
    # Un solo frame de HELD, ya facing RIGHT: el turno debe estar en marcha
    # YA (PLAYER_MOVE), sin haber pasado nunca por WAIT/margen.
    check(st.phase() == PHASE_MOVE, "un solo frame de HELD ya arranca a caminar (sin margen)")
    check(st.grace_active() == 0, "el margen nunca se activo (ya miraba hacia alli)")

    visited_walk3 = {p_before_walk3[0]}
    grace_ever_active_walk3 = False
    for _ in range(40):   # 40*4 = 160 frames: de sobra para varias casillas + turnos de enemigos
        emu.run(4)
        if st.grace_active():
            grace_ever_active_walk3 = True
        x, y = st.player()
        if (x - p_before_walk3[0]) % TILE == 0:
            visited_walk3.add(x)

    core.clear_keys(emu.KEY["RIGHT"])
    run_until_idle(emu)
    p_after_walk3 = st.player()
    visited_walk3.add(p_after_walk3[0])

    tiles_walk3 = sorted(visited_walk3)
    print(f"casillas (x) visitadas manteniendo RIGHT: {tiles_walk3}")
    print(f"posicion final: {p_after_walk3}  facing: {FACING_NAME[st.facing()]}  fase: {st.phase()}")
    check(len(tiles_walk3) >= 4, f"encadeno al menos 3 casillas seguidas manteniendo RIGHT (visito {tiles_walk3})")
    check(p_after_walk3[1] == p_before_walk3[1], "se mantuvo en la misma fila (movimiento puramente horizontal)")
    check(not grace_ever_active_walk3, "el margen de giro NUNCA se activo (ya miraba hacia RIGHT desde el principio)")
    check(st.phase() == PHASE_INPUT, "al soltar, el turno se resolvio del todo (de vuelta a PLAYER_INPUT)")

    # -----------------------------------------------------------------
    # PUNTO 1: toque corto (menos que el margen) hacia el lado CONTRARIO
    # (facing RIGHT, pulsa LEFT): gira, el jugador NO se mueve (0px), la
    # fase vuelve a PLAYER_INPUT, y el margen se cancela al soltar.
    # -----------------------------------------------------------------
    print("\n--- punto 1: toque corto (< margen) hacia el lado contrario ---")
    p_before_tap = st.player()
    core.add_keys(emu.KEY["LEFT"])
    emu.run(GRACE_FRAMES - 3)   # 5 frames HELD, por debajo de SIMA_TURN_GRACE_FRAMES=8
    core.clear_keys(emu.KEY["LEFT"])
    # OJO: la fase ya esta en PLAYER_INPUT (nunca se movio), asi que
    # run_until_idle() no correria ni un frame por si sola (su condicion de
    # salida ya se cumple) -- hace falta un frame REAL sin ninguna tecla
    # pulsada para que UpdatePlayerInput tome la rama "nada pulsado" que
    # cancela sTurnGraceActive (ver el comentario de esa rama en
    # src/sima_actors.c). Sin este run(2) explicito, grace_active() seguiria
    # leyendo el valor que tenia a mitad del toque.
    emu.run(2)
    run_until_idle(emu)
    p_after_tap = st.player()
    print(f"antes: {p_before_tap}  despues: {p_after_tap}  facing: {FACING_NAME[st.facing()]}  "
          f"fase: {st.phase()}  grace_active: {st.grace_active()}")
    check(st.facing() == FACING_LEFT, "el facing SI cambio a LEFT (giro gratis)")
    check(p_after_tap == p_before_tap, "el jugador NO se movio ni un pixel (0px)")
    check(st.phase() == PHASE_INPUT, "la fase volvio a PLAYER_INPUT (no se consumio turno)")
    check(st.grace_active() == 0, "el margen se cancelo al soltar antes de agotarse")

    # Toque corto de vuelta a RIGHT (misma mecanica, para dejar el escenario
    # listo para el punto 2: necesitamos "girar de verdad" ahi, no "ya
    # miraba hacia alli").
    core.add_keys(emu.KEY["RIGHT"])
    emu.run(2)
    core.clear_keys(emu.KEY["RIGHT"])
    run_until_idle(emu)
    check(st.facing() == FACING_RIGHT, "toque de preparacion: vuelto a mirar RIGHT")
    check(st.player() == p_before_tap, "el toque de preparacion tampoco movio al jugador")

    # -----------------------------------------------------------------
    # PUNTO 2: mantener hacia el lado CONTRARIO (facing RIGHT, mantiene
    # LEFT) -- primero gira (gratis, instantaneo), luego espera dentro del
    # margen, y pasado SIMA_TURN_GRACE_FRAMES empieza a caminar,
    # encadenando varias casillas mientras se sigue pulsando.
    # -----------------------------------------------------------------
    print("\n--- punto 2: mantener hacia el lado contrario: gira y, pasado el margen, encadena varias casillas ---")
    p_before_walk2 = st.player()
    enemies_before_walk2 = st.enemies()
    core.add_keys(emu.KEY["LEFT"])
    emu.run(1)
    check(st.facing() == FACING_LEFT, "primer frame de HELD: gira al instante")
    check(st.player() == p_before_walk2, "ese primer frame NO movio al jugador")
    check(st.grace_active() != 0, "el margen arranca activo justo despues de girar")
    check(st.phase() == PHASE_INPUT, "sigue en PLAYER_INPUT mientras el margen corre (no es un turno)")

    # El frame de arriba (girar) ya dejo sTurnGraceTimer en 0; hacen falta
    # GRACE_FRAMES incrementos MAS manteniendo pulsado para que el margen se
    # agote (ver SimaActors_ResolveHorizInput: cada frame en WAIT
    # incrementa el contador y compara CONTRA SIMA_TURN_GRACE_FRAMES) -- de
    # esos, GRACE_FRAMES-1 se quedan en WAIT (contador 1..GRACE_FRAMES-1,
    # todos < GRACE_FRAMES) y el ULTIMO es el que dispara WALK.
    emu.run(GRACE_FRAMES - 1)   # contador ahora en GRACE_FRAMES-1: sigue en WAIT, es el ultimo frame antes de caminar
    check(st.grace_active() != 0, "sigue dentro del margen justo antes de agotarse")
    check(st.player() == p_before_walk2, "todavia no se movio mientras el margen corre")

    emu.run(1)   # este frame lleva el contador a GRACE_FRAMES: el margen se agota
    check(st.phase() == PHASE_MOVE, "al agotarse el margen, sin soltar, arranca a caminar")
    check(st.grace_active() == 0, "el margen se consume al arrancar a caminar")

    visited_walk2 = {p_before_walk2[0]}
    enemy_snapshots = [enemies_before_walk2]
    for _ in range(40):   # 40*4 = 160 frames: de sobra para varias casillas + turnos de enemigos
        emu.run(4)
        x, y = st.player()
        if (p_before_walk2[0] - x) % TILE == 0 and x not in visited_walk2:
            visited_walk2.add(x)
            enemy_snapshots.append(st.enemies())

    core.clear_keys(emu.KEY["LEFT"])
    run_until_idle(emu)
    p_after_walk2 = st.player()
    if p_after_walk2[0] not in visited_walk2:
        visited_walk2.add(p_after_walk2[0])
        enemy_snapshots.append(st.enemies())

    tiles_walk2 = sorted(visited_walk2, reverse=True)
    print(f"casillas (x) visitadas manteniendo LEFT: {tiles_walk2}")
    print(f"posicion final: {p_after_walk2}  facing: {FACING_NAME[st.facing()]}  fase: {st.phase()}")
    check(len(tiles_walk2) >= 4, f"encadeno al menos 3 casillas seguidas manteniendo LEFT (visito {tiles_walk2})")
    check(p_after_walk2[1] == p_before_walk2[1], "se mantuvo en la misma fila (movimiento puramente horizontal)")
    check(st.phase() == PHASE_INPUT, "al soltar, el turno se resolvio del todo")

    # -----------------------------------------------------------------
    # PUNTO 4: los enemigos siguen dando su paso ENTRE casilla y casilla del
    # jugador mientras este mantiene pulsado -- comprobado con las
    # instantaneas de enemigos tomadas en cada parada del punto 2 de arriba
    # (una por cada casilla que cruzo el jugador sin soltar LEFT).
    # -----------------------------------------------------------------
    print("\n--- punto 4: los enemigos dan su paso entre casilla y casilla (mantener no les roba el turno) ---")
    print(f"instantaneas de enemigos tomadas: {len(enemy_snapshots)}")
    moved_pairs = 0
    for i in range(len(enemy_snapshots) - 1):
        before_e = enemy_snapshots[i]
        after_e = enemy_snapshots[i + 1]
        any_moved = any(
            (bx, by) != (ax, ay)
            for (b_alive, bx, by), (a_alive, ax, ay) in zip(before_e, after_e)
            if b_alive and a_alive
        )
        print(f"  paso jugador {i} -> {i+1}: enemigos antes {before_e}  despues {after_e}  "
              f"{'algun enemigo se movio' if any_moved else 'NINGUN enemigo se movio'}")
        if any_moved:
            moved_pairs += 1
    check(len(enemy_snapshots) >= 4, "se tomaron instantaneas de sobra (>= 4) para juzgar el patron")
    check(moved_pairs >= 1, "al menos un enemigo se movio entre dos casillas consecutivas del jugador")
    # NOTA: no se exige que TODOS los pares muestren movimiento -- es
    # comportamiento real y valido que un enemigo se quede plantado un turno
    # (perseguir bloqueado en ambos ejes, o "atacar" aterrizando en la
    # casilla del jugador, que golpea sin desplazar el sprite del enemigo --
    # ver el comentario grande en StartEnemyTurn). Una mayoria abrumadora de
    # pares con movimiento ya descarta "mantener pulsado le roba el turno a
    # los enemigos".
    print(f"pares con movimiento: {moved_pairs}/{len(enemy_snapshots) - 1}")
    check(moved_pairs >= len(enemy_snapshots) - 1 - 2,
          "los enemigos se movieron en (casi) todos los pasos del jugador -- como mucho 2 excepciones "
          "legitimas (bloqueo/ataque sin desplazamiento), ningun turno de enemigos se salto sistematicamente")

    # Prueba COMPLEMENTARIA y determinista (no depende de que un enemigo se
    # desplace de verdad, que puede fallar por RNG/bloqueo): mientras LEFT
    # sigue pulsado sin soltar ni un frame, sTurnPhase DEBE visitar
    # SIMA_TURN_ENEMY_STEP (3) mas de una vez -- eso es, literalmente, "los
    # enemigos dieron su turno" varias veces sin que soltar el boton lo haya
    # evitado nunca.
    print("\n--- punto 4 (complemento determinista): sTurnPhase visita ENEMY_STEP varias veces sin soltar ---")
    core.add_keys(emu.KEY["RIGHT"])   # volvemos a mover (ahora hacia la derecha) para otra tanda, sin soltar nunca
    enemy_step_visits = 0
    was_enemy_step = False
    for _ in range(200):
        emu.run(1)
        is_enemy_step = (st.phase() == PHASE_ENEMY)
        if is_enemy_step and not was_enemy_step:
            enemy_step_visits += 1
        was_enemy_step = is_enemy_step
    core.clear_keys(emu.KEY["RIGHT"])
    run_until_idle(emu)
    print(f"veces que sTurnPhase entro en ENEMY_STEP mientras RIGHT seguia pulsado sin soltar: {enemy_step_visits}")
    check(enemy_step_visits >= 3, "el turno de los enemigos se disparo varias veces (>= 3) sin soltar el boton ni un frame")

    print("\n" + ("TODAS LAS VERIFICACIONES PASARON" if not failures else f"FALLARON {len(failures)}: {failures}"))
    raise SystemExit(0 if not failures else 1)


if __name__ == "__main__":
    main()
