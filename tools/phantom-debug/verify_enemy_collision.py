#!/usr/bin/env python3
"""Verificacion por memoria de la colision jugador-enemigo de SIMA
(reconstruccion tras el apagon -- existia antes, se perdio; ver
.superpowers/sdd/task-sword-side-report.md).

Sin la colision, el jugador podia caminar ENCIMA de la casilla de un
enemigo vivo: sprites superpuestos y un golpe sin telegrafo. El arreglo
(SimaActors_TileMatchesEnemy + TileHasLiveEnemy en src/sima_actors.c) ya
tiene su test PURO en el harness in-ROM (sima-enemy-collision-*, ver
src/phantom_test.c) -- este script comprueba el efecto REAL jugando: guia
al jugador hacia un enemigo vivo real del piso 1 y confirma, leyendo
sPlayerX/Y por memoria, que el intento de pisar su casilla NUNCA aterriza
ahi mientras el enemigo siga vivo.

Estrategia (adaptativa, no una ruta fija a mano): en cada turno del
jugador, si hay un enemigo vivo a distancia Manhattan 1, se intenta pisar
su casilla (el caso prohibido) y se verifica que el jugador NO se mueve.
Si no hay ninguno a distancia 1 todavia, se da un paso hacia el mas
cercano. Como los enemigos persiguen deterministamente dentro de
SIMA_ENEMY_DETECT_RANGE (sin RNG en ese camino) pero pueden golpear al
jugador y empujarlo (knockback) antes de que se de la casilla exacta, el
bucle simplemente reintenta turno a turno hasta ver el caso "distancia 1,
casilla objetivo == la del enemigo, jugador no se mueve" o agota un tope
de turnos.

Uso: PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python \
     tools/phantom-debug/verify_enemy_collision.py
"""
from phantom_dbg.emu import Emu

ROM = "pokeemerald_modern_sima.gba"
BOOT_FRAMES = 320

# Direcciones IWRAM (arm-none-eabi-nm pokeemerald_modern_sima.elf | grep
# sPlayer/sEnemy/sTurnPhase), RE-OBTENIDAS para esta tarea (espada +
# corazones + colision) -- no reusar direcciones de scripts anteriores.
ADDR_PLAYER_X = 0x03000eac        # s16
ADDR_PLAYER_Y = 0x03000eae        # s16
ADDR_PLAYER_FACING = 0x03000eb0   # u8: 0=DOWN 1=UP 2=LEFT 3=RIGHT
ADDR_PLAYER_HP = 0x03000eb8       # u8
ADDR_TURN_PHASE = 0x03000ebf      # u8: 0=PLAYER_INPUT 1=PLAYER_MOVE 2=PLAYER_ATTACK 3=ENEMY_STEP ...
ADDR_ENEMY_ALIVE = 0x03000ed8     # bool8[3]
ADDR_ENEMY_X = 0x03000ee4         # s16[3]
ADDR_ENEMY_Y = 0x03000eec         # s16[3]

TILE = 16
FACING_DOWN, FACING_UP, FACING_LEFT, FACING_RIGHT = 0, 1, 2, 3
PHASE_INPUT = 0
MAX_TURNS = 60


def s16(v):
    return v - 0x10000 if v >= 0x8000 else v


class State:
    def __init__(self, emu):
        self.emu = emu

    def player_tile(self):
        x = s16(self.emu.mem_u16(ADDR_PLAYER_X)) // TILE
        y = s16(self.emu.mem_u16(ADDR_PLAYER_Y)) // TILE
        return x, y

    def facing(self):
        return self.emu.mem_u8(ADDR_PLAYER_FACING)

    def hp(self):
        return self.emu.mem_u8(ADDR_PLAYER_HP)

    def phase(self):
        return self.emu.mem_u8(ADDR_TURN_PHASE)

    def enemies(self):
        out = []
        for i in range(3):
            alive = self.emu.mem_u8(ADDR_ENEMY_ALIVE + i) != 0
            x = s16(self.emu.mem_u16(ADDR_ENEMY_X + i * 2)) // TILE
            y = s16(self.emu.mem_u16(ADDR_ENEMY_Y + i * 2)) // TILE
            out.append((alive, x, y))
        return out

    def nearest_alive(self, px, py):
        best = None
        for alive, ex, ey in self.enemies():
            if not alive:
                continue
            dist = abs(ex - px) + abs(ey - py)
            if best is None or dist < best[0]:
                best = (dist, ex, ey)
        return best  # (dist, ex, ey) o None


def wait_idle(emu, st, max_extra=80):
    extra = 0
    while st.phase() != PHASE_INPUT and extra < max_extra:
        emu.run(1)
        extra += 1


def tap(emu, st, key):
    core = emu._core
    core.add_keys(emu.KEY[key])
    emu.run(2)
    core.clear_keys(emu.KEY[key])
    emu.run(6)
    wait_idle(emu, st)


def press_toward(emu, st, dx, dy):
    """Un intento de PASO hacia (dx, dy) (uno de los dos, el de mayor
    magnitud primero). Para horizontal respeta el tap-to-turn: si el
    jugador no miraba ya hacia ahi, el primer tap solo gira (gratis) y
    hace falta un segundo tap para mover -- se hacen ambos aqui."""
    if abs(dx) >= abs(dy) and dx != 0:
        key = "RIGHT" if dx > 0 else "LEFT"
        want_facing = FACING_RIGHT if dx > 0 else FACING_LEFT
        if st.facing() != want_facing:
            tap(emu, st, key)   # solo gira
        tap(emu, st, key)       # intenta mover (o, si es el caso prohibido, se queda quieto)
    elif dy != 0:
        key = "DOWN" if dy > 0 else "UP"
        tap(emu, st, key)
    else:
        return False
    return True


def main():
    emu = Emu(ROM)
    st = State(emu)
    emu.run(BOOT_FRAMES)

    print("Enemigos al arrancar:", st.enemies())
    px, py = st.player_tile()
    print(f"Jugador en ({px},{py}), HP={st.hp()}")

    for turn in range(MAX_TURNS):
        px, py = st.player_tile()
        nearest = st.nearest_alive(px, py)
        if nearest is None:
            print("FALLO: no queda ningun enemigo vivo, no se pudo probar la colision")
            return 1

        dist, ex, ey = nearest
        if dist == 1:
            # EL CASO PROHIBIDO: intentar pisar la casilla del enemigo.
            dx, dy = ex - px, ey - py
            print(f"turno {turn}: jugador ({px},{py}) adyacente a enemigo vivo "
                  f"({ex},{ey}) -- intentando pisarlo (dx={dx}, dy={dy})")
            press_toward(emu, st, dx, dy)
            new_px, new_py = st.player_tile()
            still_alive_there = any(
                a and ax == ex and ay == ey for a, ax, ay in st.enemies()
            )
            print(f"  tras el intento: jugador en ({new_px},{new_py}), "
                  f"enemigo sigue vivo en ({ex},{ey})? {still_alive_there}")
            if (new_px, new_py) == (ex, ey):
                print("FALLO: el jugador SI entro en la casilla del enemigo vivo "
                      "-- colision rota")
                return 1
            if (new_px, new_py) != (px, py):
                print("FALLO: el jugador se movio a una casilla inesperada "
                      f"({new_px},{new_py}), no la de partida ({px},{py})")
                return 1
            print("OK: el jugador se quedo en su casilla, no piso al enemigo vivo.")
            print(f"HP final: {st.hp()}")
            return 0

        # Todavia no adyacente: un paso hacia el enemigo mas cercano.
        dx, dy = ex - px, ey - py
        moved = press_toward(emu, st, dx, dy)
        if not moved:
            print("FALLO: sin direccion util hacia el enemigo (dx=dy=0 pero dist!=1?)")
            return 1

    print("FALLO: se agotaron los turnos sin llegar a probar la colision")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
