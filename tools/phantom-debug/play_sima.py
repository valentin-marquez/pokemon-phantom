#!/usr/bin/env python3
"""Juega SIMA sin manos y graba un GIF.

El harness (phantom_dbg.emu.Emu) ya sabía pulsar teclas desde el principio; lo
que faltaba era exponerlo. Esto cierra el hueco de "no hay inyección de inputs"
que el proyecto arrastraba: permite verificar de verdad lo que solo se podía
mirar quieto -- que el jugador choca con los muros, que los enemigos hacen
daño, que el arma mata y que la escalera aparece al caer el último.

Uso:  PYTHONPATH=tools/phantom-debug python3 tools/phantom-debug/play_sima.py salida.gif
"""
import os
import sys

from phantom_dbg.emu import Emu

# OJO: aquí NO se importa PIL. El venv de libmgba no lo tiene (ni pip), así que
# este script solo vuelca PNG numerados y el montaje del GIF lo hace el Python
# del sistema. Separarlo evita tener que tocar el venv del harness.

ROM = "pokeemerald_modern_sima.gba"
BOOT_FRAMES = 320          # copyright + fundido + sala montada
SAMPLE_EVERY = 3           # 1 fotograma de cada 3 -> ~20 fps en el GIF


class Recorder:
    """Corre el emulador guardando fotogramas cada SAMPLE_EVERY."""

    def __init__(self, emu, tmpdir):
        self.emu, self.tmp, self.n, self.frames = emu, tmpdir, 0, []

    def run(self, frames, keys=()):
        core = self.emu._core
        for k in keys:
            core.add_keys(self.emu.KEY[k])
        for _ in range(frames):
            core.run_frame()
            self.n += 1
            if self.n % SAMPLE_EVERY == 0:
                p = os.path.join(self.tmp, f"f{self.n:05d}.png")
                self.emu.screenshot(p)
                self.frames.append(p)
        for k in keys:
            core.clear_keys(self.emu.KEY[k])

    def tap(self, key, held=4, gap=14):
        """Pulsación suelta. El ataque usa JOY_NEW, así que hay que soltar."""
        self.run(held, (key,))
        self.run(gap)


def main():
    tmp = sys.argv[1] if len(sys.argv) > 1 else "/tmp/sima_frames"
    os.makedirs(tmp, exist_ok=True)
    for f in os.listdir(tmp):
        os.remove(os.path.join(tmp, f))
    if True:
        emu = Emu(ROM)
        rec = Recorder(emu, tmp)

        rec.run(BOOT_FRAMES)          # llegar a la sala

        # El jugador entra por el arco de arriba a la izquierda, en la casilla
        # (1,0). Los enemigos del piso 1 estan en (3,6), (11,2) y (11,6).
        # A 1 px por frame, una casilla son 16 frames.
        rec.run(150, ("DOWN",))       # bajar hacia la fila del primer enemigo
        rec.run(40,  ("RIGHT",))      # acercarse a (3,6)
        for _ in range(4):            # repartir mandobles
            rec.tap("A")
        rec.run(30,  ("RIGHT",))
        for _ in range(3):
            rec.tap("A")
        rec.run(60,  ("RIGHT",))      # cruzar hacia el lado derecho
        for _ in range(3):
            rec.tap("A")
        rec.run(50,  ("UP",))
        for _ in range(3):
            rec.tap("A")
        rec.run(40)                   # dejar respirar el final

        print(f"{tmp}  ({len(rec.frames)} fotogramas, {rec.n} frames de emulacion)")


if __name__ == "__main__":
    main()
