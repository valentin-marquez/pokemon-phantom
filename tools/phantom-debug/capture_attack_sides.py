#!/usr/bin/env python3
"""Captura un golpe a IZQUIERDA y otro a DERECHA sobre suelo abierto (tarea
"cambio de sensacion" de SIMA -- vista de perfil pura + ataque solo
izquierda/derecha, ver .superpowers/sdd/task-profile-detect-report.md).

Posiciona al jugador en el pasillo superior (fila 1, abierto de x=1 a x=10,
lejos del arco de entrada en (1,0) que es fondo con mucho detalle) y ataca
mirando a cada lado, capturando un fotograma durante la ventana ACTIVA del
golpe (el arma en el frame de impacto, FRAME_WEAPON_HORIZ_B).

Uso: PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python \
     tools/phantom-debug/capture_attack_sides.py docs/design/captures/sima-profile-sensation
"""
import os
import sys

from phantom_dbg.emu import Emu

ROM = "pokeemerald_modern_sima.gba"
BOOT_FRAMES = 320

ADDR_PLAYER_FACING = 0x03000eb0
ADDR_TURN_PHASE = 0x03000eb6
ADDR_ATTACK_TIMER = None  # no hace falta: contamos frames a mano (ver mas abajo)

FACING_LEFT, FACING_RIGHT = 2, 3

# ATTACK_WINDUP_FRAMES=3, ATTACK_ACTIVE_FRAMES=4 (src/sima_actors.c): la
# ventana activa es sAttackTimer en {4,5,6,7}. sAttackTimer=1 se fija en el
# mismo frame que JOY_NEW(A) se lee y sube de 1 en 1 cada frame siguiente,
# asi que 5 frames despues de soltar A (contando el de la pulsacion) caemos
# a mitad de la ventana activa (timer=6).
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
    """Gira hacia direction_key si hace falta (tap-to-turn, JOY_NEW): una
    pulsacion basta si NO se miraba ya hacia ahi -- no consume turno."""
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
    # deja que el turno termine del todo antes de la siguiente accion
    extra = 0
    while emu.mem_u8(ADDR_TURN_PHASE) != 0 and extra < 60:
        emu.run(1)
        extra += 1


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "/tmp/sima_attack_captures"
    os.makedirs(out_dir, exist_ok=True)

    emu = Emu(ROM)
    emu.run(BOOT_FRAMES)

    # Al pasillo abierto (fila 1): DOWN desde el spawn, luego RIGHT varias
    # veces para alejarnos del arco de entrada (1,0)/(1,1) hacia suelo liso.
    tap(emu, "DOWN")
    for _ in range(4):
        tap(emu, "RIGHT")   # facing por defecto ya es RIGHT: cada pulsacion mueve

    print("jugador en suelo abierto (pasillo, lejos del arco de entrada)")

    face(emu, "LEFT", FACING_LEFT)
    attack_and_capture(emu, os.path.join(out_dir, "attack_left.png"))

    face(emu, "RIGHT", FACING_RIGHT)
    attack_and_capture(emu, os.path.join(out_dir, "attack_right.png"))

    print("listo")


if __name__ == "__main__":
    main()
