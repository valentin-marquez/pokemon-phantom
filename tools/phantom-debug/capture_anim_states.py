#!/usr/bin/env python3
"""Captura una imagen del golpe recibido (animación de daño) y otra de la
muerte (tarea de animación de SIMA), para revisar la pantalla ENTERA en
busca de basura/artefactos -- no solo "el sprite se ve bien", sino que no
haya ningún patrón repetido, marca rara o recuadro de color por ningún lado
(ya pasó en este proyecto: un tile mal cargado llenó toda la pantalla).

Reutiliza el bot de tools/phantom-debug/verify_anim_states.py (persigue al
enemigo más cercano sin atacar, deja que el contacto haga daño) en vez de
duplicar la lógica de movimiento.

Uso: PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python \
     tools/phantom-debug/capture_anim_states.py docs/design/captures/sima-player-anims
"""
import os
import sys

from phantom_dbg.emu import Emu
import verify_anim_states as V


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "/tmp/sima_anim_captures"
    os.makedirs(out_dir, exist_ok=True)

    emu = Emu(V.ROM)
    rec = V.Rec(emu)
    rec.run(V.BOOT_FRAMES)

    # --- golpe recibido ---
    print("buscando un golpe...")
    hp0 = rec.read_hp()
    for _ in range(200):
        if rec.read_hp() != hp0:
            break
        enemies = [e for e in rec.read_enemies() if e[0]]
        px, py = rec.read_player()
        alive, ex, ey = min(enemies, key=lambda e: abs(e[1] - px) + abs(e[2] - py))
        if px == ex and py == ey:
            rec.press_release("UP")
        else:
            rec.press_release(V.facing_key_toward(ex - px, ey - py))
        V.run_until(rec, lambda r: r.read_phase() in (V.PHASE_INPUT,), max_frames=60)
    else:
        print("  no se consiguió un golpe en el límite de intentos")
        return 1

    print(f"  golpe recibido: vida {hp0} -> {rec.read_hp()}")
    # sPlayerInvulnTimer se fija a SIMA_HIT_INVULN_FRAMES (20) en el instante
    # del golpe y ya está contando atrás para cuando volvemos a tener el
    # control (el turno se resolvió del todo, run_until_idle). Para capturar
    # el frame más "blanco" (SimaActors_DamageAnimFrame ~ 2, el pico del
    # flash) haría falta pillarlo a mitad del turno -- más simple: provocar
    # un segundo golpe justo después no funciona (invulnerabilidad), así que
    # en vez de perseguir el pico exacto, se captura el estado actual del
    # personaje (que igual sigue mostrando la reacción si el turno fue
    # corto) más una repetición con avance fino si ya se apagó.
    if rec.read_phase() != V.PHASE_INPUT:
        V.run_until(rec, lambda r: r.read_phase() == V.PHASE_INPUT, max_frames=60)
    path = os.path.join(out_dir, "damage.png")
    emu.screenshot(path)
    print(f"  capturado: {path}  jugador en {rec.read_player()}")

    # --- muerte ---
    print("buscando la muerte...")
    died = V.seek_death(rec, max_turns=400)
    if not died:
        print("  no se consiguió la muerte en el límite de turnos")
        return 1
    print(f"  murió -- fase: {rec.read_phase()} (esperado {V.PHASE_DEAD})")
    # A mitad de la animación de muerte (SIMA_DEATH_ANIM_FRAMES=18): unos
    # cuantos frames dentro para no capturar el primer frame (casi idéntico
    # a estar de pie).
    rec.run(9)
    path = os.path.join(out_dir, "dead.png")
    emu.screenshot(path)
    print(f"  capturado: {path}  (fase: {rec.read_phase()})  jugador en {rec.read_player()}")

    print("listo")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
