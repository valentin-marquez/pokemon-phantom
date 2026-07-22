#!/usr/bin/env python3
"""Verificacion por memoria del cambio a turnos de SIMA (ver el informe de la
tarea en .superpowers/sdd/task-turnbased-report.md).

No mira pixeles: lee directamente sPlayerX/sPlayerY/sEnemyX/sEnemyY/sTurnPhase
de IWRAM (direcciones sacadas de `arm-none-eabi-nm pokeemerald_modern_sima.elf`,
build con PHANTOM_DEBUG_SIMA=1) para demostrar, con numeros exactos, que:

  1. Sin pulsar nada, nadie se mueve ni un solo pixel (el turno no avanza solo).
  2. Una pulsacion de DOWN mueve al jugador exactamente UNA casilla (16px).
  3. Al terminar ese turno, cada enemigo vivo se desplazo exactamente 0 o 16px
     (nunca una cantidad intermedia -- el turno completo, deslizamiento
     incluido, ya se resolvio del todo).

Uso: PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python \
     tools/phantom-debug/verify_turns.py
"""
from phantom_dbg.emu import Emu

ROM = "pokeemerald_modern_sima.gba"
BOOT_FRAMES = 320  # copyright + fundido + sala montada (igual que play_sima.py)

# Direcciones IWRAM (arm-none-eabi-nm pokeemerald_modern_sima.elf | grep sPlayer/sEnemy):
ADDR_PLAYER_X = 0x03000eac   # s16
ADDR_PLAYER_Y = 0x03000eae   # s16
ADDR_TURN_PHASE = 0x03000eb6  # u8: 0=PLAYER_INPUT 1=PLAYER_MOVE 2=PLAYER_ATTACK 3=ENEMY_STEP
ADDR_ENEMY_X = 0x03000edc    # s16[3]
ADDR_ENEMY_Y = 0x03000ee4    # s16[3]


def s16(v):
    return v - 0x10000 if v >= 0x8000 else v


def read_player(emu):
    return s16(emu.mem_u16(ADDR_PLAYER_X)), s16(emu.mem_u16(ADDR_PLAYER_Y))


def read_enemies(emu):
    out = []
    for i in range(3):
        x = s16(emu.mem_u16(ADDR_ENEMY_X + i * 2))
        y = s16(emu.mem_u16(ADDR_ENEMY_Y + i * 2))
        out.append((x, y))
    return out


def read_phase(emu):
    return emu.mem_u8(ADDR_TURN_PHASE)


def main():
    emu = Emu(ROM)
    emu.run(BOOT_FRAMES)

    print("--- fase 1: nadie se mueve sin input ---")
    p0 = read_player(emu)
    e0 = read_enemies(emu)
    print(f"jugador antes: {p0}  enemigos antes: {e0}  fase: {read_phase(emu)}")
    emu.run(90)   # metro y medio de margen (mas que de sobra para un turno entero, 16 frames)
    p1 = read_player(emu)
    e1 = read_enemies(emu)
    print(f"jugador despues (90 frames sin pulsar nada): {p1}  enemigos: {e1}  fase: {read_phase(emu)}")
    assert p0 == p1, f"FALLO: el jugador se movio solo ({p0} -> {p1})"
    assert e0 == e1, f"FALLO: algun enemigo se movio solo ({e0} -> {e1})"
    print("OK: nadie se movio sin pulsar nada.\n")

    print("--- fase 2: una pulsacion de DOWN mueve al jugador UNA casilla ---")
    core = emu._core
    core.add_keys(emu.KEY["DOWN"])
    emu.run(2)               # el D-pad solo necesita leerse un frame en PLAYER_INPUT
    core.clear_keys(emu.KEY["DOWN"])
    emu.run(40)               # sobra para: deslizamiento del jugador (8) + turno de enemigos (8) + margen

    p2 = read_player(emu)
    e2 = read_enemies(emu)
    phase2 = read_phase(emu)
    print(f"jugador tras el turno: {p2}  enemigos: {e2}  fase: {phase2}")

    dpx = p2[0] - p1[0]
    dpy = p2[1] - p1[1]
    print(f"desplazamiento del jugador: dx={dpx} dy={dpy}")
    assert (dpx, dpy) == (0, 16), f"FALLO: el jugador no avanzo exactamente 16px hacia abajo (dx={dpx}, dy={dpy})"
    assert phase2 == 0, f"FALLO: el turno no volvio a PLAYER_INPUT (fase={phase2})"
    print("OK: el jugador avanzo exactamente 16px (una casilla) hacia abajo, y el turno volvio a PLAYER_INPUT.\n")

    print("--- desplazamiento de cada enemigo en ese mismo turno ---")
    all_ok = True
    for i, ((ex0, ey0), (ex1, ey1)) in enumerate(zip(e1, e2)):
        dx, dy = ex1 - ex0, ey1 - ey0
        dist = abs(dx) + abs(dy)  # axis-aligned: Manhattan == Chebyshev == modulo del paso
        ok = dist in (0, 16)
        all_ok &= ok
        print(f"enemigo {i}: ({ex0},{ey0}) -> ({ex1},{ey1})  dx={dx} dy={dy}  |paso|={dist}  {'OK' if ok else 'FALLO'}")
    assert all_ok, "FALLO: algun enemigo se movio una cantidad que no es 0 ni 16px"
    print("\nOK: cada enemigo avanzo exactamente 0 o 16px (nunca una cantidad intermedia).")

    print("\n--- fase 3: girarse contra un muro no consume turno ---")
    # El jugador esta ahora en (1,1) (tile), mirando abajo. La casilla (0,1)
    # es muro (columna 0, fila 1 de sRoomSolid) -- pulsar IZQUIERDA debe
    # girarlo sin moverlo, y el turno no debe tardar los 16 frames de un paso
    # de verdad: unos pocos frames bastan para confirmar que no hay
    # deslizamiento en marcha.
    p3_before = read_player(emu)
    core.add_keys(emu.KEY["LEFT"])
    emu.run(2)
    core.clear_keys(emu.KEY["LEFT"])
    emu.run(6)   # muy por debajo de los 16 frames de un turno real -- si esto NO fuera gratis, seguiria a mitad de deslizamiento
    p3_after = read_player(emu)
    phase3 = read_phase(emu)
    print(f"jugador antes de LEFT: {p3_before}  despues (6 frames): {p3_after}  fase: {phase3}")
    assert p3_before == p3_after, f"FALLO: girarse contra un muro movio al jugador ({p3_before} -> {p3_after})"
    assert phase3 == 0, f"FALLO: girarse contra un muro no debia dejar un turno a medias (fase={phase3})"
    print("OK: girarse contra la pared no movio al jugador ni dejo un turno a medias (fase sigue en PLAYER_INPUT).")

    print("\nTODAS LAS VERIFICACIONES POR MEMORIA PASARON.")


if __name__ == "__main__":
    main()
