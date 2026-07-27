#!/usr/bin/env python3
"""Captura de verificacion visual para el arreglo de anclaje de la espada
(casilla adyacente, no sobre el jugador) y el HUD de 5 corazones (ver
.superpowers/sdd/task-sword-side-report.md).

Tres capturas:
  - attack_left.png / attack_right.png: golpe en suelo abierto (pasillo
    fila 1), a mitad de la ventana ACTIVA -- misma tecnica que
    capture_attack_sides.py (tarea anterior), direcciones IWRAM
    RE-OBTENIDAS para esta tarea.
  - hud_full_hearts.png: pantalla completa nada mas montar la sala, con
    los 5 corazones llenos (SIMA_PLAYER_MAX_HP subido de 3 a 5).

Uso: PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python \
     tools/phantom-debug/capture_sword_and_hud.py <dir_salida>
"""
import os
import sys

from phantom_dbg.emu import Emu

ROM = "pokeemerald_modern_sima.gba"
BOOT_FRAMES = 320

ADDR_PLAYER_FACING = 0x03000eb0
ADDR_TURN_PHASE = 0x03000ebf

FACING_LEFT, FACING_RIGHT = 2, 3

# ATTACK_WINDUP_FRAMES=3, ATTACK_ACTIVE_FRAMES=4 (src/sima_actors.c): la
# ventana activa es sAttackTimer en {4,5,6,7}. sAttackTimer=1 se fija en el
# mismo frame que JOY_NEW(A) se lee; 5 frames despues cae a mitad de la
# ventana activa (timer=6).
FRAMES_TO_ACTIVE = 5


def tap(emu, key, held=2, settle=6, run_until_idle=True, max_extra=40):
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


def face(emu, direction_key, direction_val):
    if emu.mem_u8(ADDR_PLAYER_FACING) != direction_val:
        tap(emu, direction_key)


def attack_and_capture(emu, out_path):
    core = emu._core
    core.add_keys(emu.KEY["A"])
    emu.run(2)
    core.clear_keys(emu.KEY["A"])
    emu.run(FRAMES_TO_ACTIVE)
    emu.screenshot(out_path)
    print(f"  capturado: {out_path}")
    extra = 0
    while emu.mem_u8(ADDR_TURN_PHASE) != 0 and extra < 60:
        emu.run(1)
        extra += 1


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "/tmp/sima_sword_hud_captures"
    os.makedirs(out_dir, exist_ok=True)

    emu = Emu(ROM)
    emu.run(BOOT_FRAMES)

    # HUD con los 5 corazones llenos: recien montada la sala, sin recibir
    # golpes todavia.
    hud_path = os.path.join(out_dir, "hud_full_hearts.png")
    emu.screenshot(hud_path)
    print(f"  capturado: {hud_path}")

    # Al pasillo abierto (fila 1), lejos del arco de entrada.
    tap(emu, "DOWN")
    for _ in range(4):
        tap(emu, "RIGHT")

    print("jugador en suelo abierto (pasillo, lejos del arco de entrada)")

    face(emu, "LEFT", FACING_LEFT)
    attack_and_capture(emu, os.path.join(out_dir, "attack_left.png"))

    face(emu, "RIGHT", FACING_RIGHT)
    attack_and_capture(emu, os.path.join(out_dir, "attack_right.png"))

    print("listo")


if __name__ == "__main__":
    main()
