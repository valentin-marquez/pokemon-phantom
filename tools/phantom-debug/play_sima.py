#!/usr/bin/env python3
"""Juega SIMA sin manos y graba un GIF.

El harness (phantom_dbg.emu.Emu) ya sabía pulsar teclas desde el principio; lo
que faltaba era exponerlo. Esto cierra el hueco de "no hay inyección de inputs"
que el proyecto arrastraba: permite verificar de verdad lo que solo se podía
mirar quieto -- que el jugador choca con los muros, que los enemigos hacen
daño, que el arma mata y que la escalera aparece al caer el último.

Reescrito para el cambio a POR TURNOS (ver el informe de la tarea,
.superpowers/sdd/task-turnbased-report.md): ya no tiene sentido "aguantar N
frames en una direccion" a ojo -- con movimiento por rejilla + deslizamiento,
un turno del jugador dura SIMA_PLAYER_SLIDE_FRAMES (8) y el de los enemigos
otros SIMA_ENEMY_SLIDE_FRAMES (8), asi que basta con leer la posicion en
memoria (Emu.mem_u8/u16, igual que tools/phantom-debug/verify_turns.py) y
decidir el siguiente paso turno a turno: un bot minimo que persigue al
enemigo vivo mas cercano, ataca en cuanto lo tiene al lado, y al final baja
las escaleras. Es mas robusto que una lista fija de pulsaciones porque los
enemigos tambien se mueven cada turno -- no hay "la casilla exacta en la que
va a estar el rat en el frame 214".

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

# Direcciones IWRAM de src/sima_actors.c (arm-none-eabi-nm pokeemerald_modern_sima.elf),
# mismas que tools/phantom-debug/verify_turns.py.
ADDR_PLAYER_X = 0x03000eac    # s16
ADDR_PLAYER_Y = 0x03000eae    # s16
ADDR_PLAYER_FACING = 0x03000eb0  # u8: 0=DOWN 1=UP 2=LEFT 3=RIGHT (enum SimaFacing)
ADDR_TURN_PHASE = 0x03000eb6  # u8: 0=PLAYER_INPUT 1=PLAYER_MOVE 2=PLAYER_ATTACK 3=ENEMY_STEP
ADDR_ENEMY_ALIVE = 0x03000ed0  # bool8[3]
ADDR_ENEMY_X = 0x03000edc     # s16[3]
ADDR_ENEMY_Y = 0x03000ee4     # s16[3]

TILE = 16
FACING_DOWN, FACING_UP, FACING_LEFT, FACING_RIGHT = 0, 1, 2, 3
KEY_OF_FACING = {FACING_DOWN: "DOWN", FACING_UP: "UP", FACING_LEFT: "LEFT", FACING_RIGHT: "RIGHT"}


def s16(v):
    return v - 0x10000 if v >= 0x8000 else v


class Recorder:
    """Corre el emulador guardando fotogramas cada SAMPLE_EVERY, con acceso
    directo a memoria para decidir el siguiente turno (bot de persecucion)."""

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

    def tap(self, key, held=2, run_until_idle=True, max_extra=60):
        """Pulsación suelta de un frame o dos (JOY_NEW/JOY_HELD solo se leen
        en SIMA_TURN_PLAYER_INPUT). Si run_until_idle, sigue corriendo hasta
        que sTurnPhase vuelva a PLAYER_INPUT (turno resuelto del todo) o se
        agote max_extra -- evita encadenar pulsaciones a mitad de un turno
        ajeno."""
        self.run(held, (key,))
        if run_until_idle:
            extra = 0
            while self.read_phase() != 0 and extra < max_extra:
                self.run(1)
                extra += 1

    # --- memoria ---
    def read_player(self):
        return s16(self.emu.mem_u16(ADDR_PLAYER_X)), s16(self.emu.mem_u16(ADDR_PLAYER_Y))

    def read_phase(self):
        return self.emu.mem_u8(ADDR_TURN_PHASE)

    def read_enemies(self):
        out = []
        for i in range(3):
            alive = self.emu.mem_u8(ADDR_ENEMY_ALIVE + i) != 0
            x = s16(self.emu.mem_u16(ADDR_ENEMY_X + i * 2))
            y = s16(self.emu.mem_u16(ADDR_ENEMY_Y + i * 2))
            out.append((alive, x, y))
        return out


def step_toward(rec, tx, ty):
    """Un turno del jugador hacia la casilla (tx, ty) en pixeles: elige el
    eje que mas lo acerca (misma prioridad que SimaActors_EnemyStepTarget) y
    pulsa esa direccion. Si esta bloqueada, el juego ya lo gira sin gastar
    turno -- este bot simplemente lo intenta con el otro eje en la siguiente
    llamada si hace falta."""
    px, py = rec.read_player()
    dx, dy = tx - px, ty - py
    if abs(dx) >= abs(dy) and dx != 0:
        key = "RIGHT" if dx > 0 else "LEFT"
    elif dy != 0:
        key = "DOWN" if dy > 0 else "UP"
    elif dx != 0:
        key = "RIGHT" if dx > 0 else "LEFT"
    else:
        return False  # ya esta ahi
    rec.tap(key)
    return True


def facing_key_toward(dx, dy):
    if abs(dx) >= abs(dy):
        return "RIGHT" if dx > 0 else "LEFT"
    return "DOWN" if dy > 0 else "UP"


def hunt_enemies(rec, max_turns=60):
    """Bot minimo: mientras queden enemigos vivos, se acerca al mas cercano
    y ataca en cuanto esta en la casilla de al lado (adyacente en un eje,
    mismo valor en el otro -- igual que decide SimaActors_EnemyStepTarget
    que un paso "llega" a la casilla del jugador).

    Deteccion de "ping-pong": acercarse en linea recta a un enemigo que
    tambien viene derecho hacia el jugador puede acabar en un empate
    perfecto -- el jugador entra en la casilla de al lado justo cuando le
    toca el turno al enemigo, este ataca y el empujon lo devuelve EXACTAMENTE
    a la casilla de partida (misma distancia, mismo eje dominante), así que
    el bot repetiria la misma jugada para siempre. No es un bug del turno
    (verificado aparte por verify_turns.py) -- es que perseguir en linea
    recta no es una buena tactica contra algo que tambien te persigue en
    linea recta. Si se detecta la oscilacion (misma posicion que hace dos
    turnos), este bot rompe el empate moviendose por el eje PERPENDICULAR un
    turno, para dejar de estar perfectamente alineado.
    """
    history = []
    for _ in range(max_turns):
        enemies = [e for e in rec.read_enemies() if e[0]]
        if not enemies:
            print("  todos los enemigos muertos")
            return
        px, py = rec.read_player()
        history.append((px, py))
        oscillating = len(history) >= 4 and history[-1] == history[-3] and history[-2] == history[-4]

        alive, ex, ey = min(enemies, key=lambda e: abs(e[1] - px) + abs(e[2] - py))
        dx, dy = ex - px, ey - py
        adjacent = (dx == 0 and abs(dy) == TILE) or (dy == 0 and abs(dx) == TILE)

        if adjacent:
            key = facing_key_toward(dx, dy)
            rec.tap(key)          # gira hacia el enemigo (gratis, sin turno) si no miraba ya para ahi
            rec.tap("A")          # golpea: consume turno, mata de un golpe
        elif oscillating:
            # Rompe el empate: se mueve por el eje PERPENDICULAR al que
            # estaba usando (si iba derecho hacia abajo/arriba -- dx==0 --
            # prueba izquierda/derecha, y viceversa).
            if dx == 0:
                side_key = "RIGHT" if px <= TILE else "LEFT"
            else:
                side_key = "DOWN" if py <= TILE else "UP"
            rec.tap(side_key)
        else:
            step_toward(rec, ex, ey)
    print("  max_turns agotado, puede que quede algun enemigo vivo")


def go_to_stairs(rec, max_turns=40):
    """Casilla de la escalera del piso 1 (tools/sima-editor/salas.json,
    src/sima_rooms_data.h: sRoomStairs[0] = {13, 8}). Una vez encima, si ya
    esta desbloqueada, src/sima.c arranca la transicion de piso sola."""
    stx, sty = 13 * TILE, 8 * TILE
    for _ in range(max_turns):
        px, py = rec.read_player()
        if (px, py) == (stx, sty):
            print("  jugador sobre la escalera")
            return
        if not step_toward(rec, stx, sty):
            return
    print("  max_turns agotado acercandose a la escalera")


def main():
    tmp = sys.argv[1] if len(sys.argv) > 1 else "/tmp/sima_frames"
    os.makedirs(tmp, exist_ok=True)
    for f in os.listdir(tmp):
        os.remove(os.path.join(tmp, f))

    emu = Emu(ROM)
    rec = Recorder(emu, tmp)

    rec.run(BOOT_FRAMES)          # llegar a la sala

    print("cazando enemigos...")
    hunt_enemies(rec, max_turns=14)   # demo corta: no hace falta limpiar la sala entera para la captura

    if all(not alive for alive, _, _ in rec.read_enemies()):
        print("todos muertos -- bajando a la escalera...")
        go_to_stairs(rec)

    rec.run(90)                   # dejar respirar el final / correr la transicion de piso si la hubo

    print(f"{tmp}  ({len(rec.frames)} fotogramas, {rec.n} frames de emulacion)")


if __name__ == "__main__":
    main()
