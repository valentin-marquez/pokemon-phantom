#!/usr/bin/env python3
"""Verificacion por memoria de la tarea de animacion de SIMA (idle/move/
damage/dead/teleport, ver .superpowers/sdd/task-player-anims-report.md).

Lo que una foto fija NO puede probar (y esto si, leyendo IWRAM):

  1. Al llegar la vida a 0, el jugador reaparece en la casilla de inicio del
     piso con la vida llena, los enemigos vuelven a estar todos vivos, y
     sTurnPhase acaba en PLAYER_INPUT (no se queda colgado en la fase de
     muerte ni a mitad de fundido).
  2. Durante la animacion de muerte y la de teleport, pulsar una direccion
     NO mueve al jugador (sTurnPhase no lee input en esas fases).
  3. Tras bajar por la escalera (que ahora pasa por la animacion de
     teleport antes del fundido), el estado sigue siendo coherente
     (sTurnPhase vuelve a PLAYER_INPUT, el jugador aparece en el spawn del
     piso -- con SIMA_FLOOR_COUNT=1 hoy, el mismo piso).

Nota tecnica: un tap() que espera con run_until_idle a que la fase vuelva a
PLAYER_INPUT SE TRAGA la secuencia entera de muerte (animación + fundido +
reset) sin que el script pueda observarla a mitad de camino -- por eso este
script avanza frame a frame (no con el tap/run_until_idle de los otros
scripts de tools/phantom-debug/) mientras persigue al enemigo, para poder
pillar el instante exacto en que sTurnPhase pasa a PLAYER_DEAD.

Uso: PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python \
     tools/phantom-debug/verify_anim_states.py
"""
from phantom_dbg.emu import Emu

ROM = "pokeemerald_modern_sima.gba"
BOOT_FRAMES = 320

# Direcciones IWRAM (arm-none-eabi-nm pokeemerald_modern_sima.elf), re-verificadas
# para esta tarea -- sTurnPhase/sEnemyAlive/X/Y se desplazaron respecto a la
# tarea anterior porque se añadieron sPlayerIdleAnimStep/Timer/DeathTimer/
# TeleportTimer antes en el archivo (ver src/sima_actors.c).
ADDR_PLAYER_X = 0x03000eac        # s16
ADDR_PLAYER_Y = 0x03000eae        # s16
ADDR_PLAYER_FACING = 0x03000eb0   # u8: 0=DOWN 1=UP 2=LEFT 3=RIGHT
ADDR_PLAYER_HP = 0x03000eb6       # u8
ADDR_TURN_PHASE = 0x03000eba      # u8: 0=INPUT 1=MOVE 2=ATTACK 3=ENEMY_STEP 4=DEAD 5=TELEPORT
ADDR_ENEMY_ALIVE = 0x03000ed4     # bool8[3]
ADDR_ENEMY_X = 0x03000ee0         # s16[3]
ADDR_ENEMY_Y = 0x03000ee8         # s16[3]

TILE = 16
SPAWN = (1 * TILE, 0 * TILE)    # sRoomSpawn[0], src/sima_rooms_data.h
STAIRS = (13 * TILE, 8 * TILE)  # sRoomStairs[0]
PHASE_INPUT, PHASE_MOVE, PHASE_ATTACK, PHASE_ENEMY_STEP, PHASE_DEAD, PHASE_TELEPORT = range(6)
FACING_DOWN, FACING_UP, FACING_LEFT, FACING_RIGHT = range(4)
FACING_OF_KEY = {"LEFT": FACING_LEFT, "RIGHT": FACING_RIGHT}

# Copia EXACTA de sRoomSolid[0] (src/sima_rooms_data.h, GENERADO por
# graphics/sima/rooms.py desde tools/sima-editor/salas.json) -- el golpe solo
# sale a izquierda/derecha (vista de perfil pura), así que perseguir a un
# enemigo que está en otra fila necesita rodear muros de verdad, no solo
# "acercarse por el eje dominante" (eso se queda encajado contra una pared,
# como demostró el primer intento de este script). Con el mapa conocido de
# antemano, un BFS tile a tile es simple y determinista. Si el piso 1 cambia
# en el editor, esta copia hay que refrescarla a mano (no hay import directo
# de un .h de C a Python aquí).
ROOM_W, ROOM_H = 15, 10
ROOM_SOLID = [
    [1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
    [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1],
    [1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1],
    [1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1],
    [1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1],
    [1, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1],
    [1, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1],
    [0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
    [0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1],
    [1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1],
]


def bfs_path(start_tile, goal_tile):
    """BFS sobre ROOM_SOLID: lista de direcciones ('UP'/'DOWN'/'LEFT'/'RIGHT'),
    una por casilla, desde start_tile hasta goal_tile. [] si ya está ahí,
    None si no hay camino (no debería pasar en este piso)."""
    if start_tile == goal_tile:
        return []
    dirs = [("UP", 0, -1), ("DOWN", 0, 1), ("LEFT", -1, 0), ("RIGHT", 1, 0)]
    visited = {start_tile: None}
    queue = [start_tile]
    while queue:
        cur = queue.pop(0)
        if cur == goal_tile:
            break
        cx, cy = cur
        for key, dx, dy in dirs:
            nx, ny = cx + dx, cy + dy
            if not (0 <= nx < ROOM_W and 0 <= ny < ROOM_H):
                continue
            if ROOM_SOLID[ny][nx]:
                continue
            nxt = (nx, ny)
            if nxt in visited:
                continue
            visited[nxt] = (cur, key)
            queue.append(nxt)
    if goal_tile not in visited:
        return None
    path = []
    node = goal_tile
    while node != start_tile:
        prev, key = visited[node]
        path.append(key)
        node = prev
    path.reverse()
    return path


def s16(v):
    return v - 0x10000 if v >= 0x8000 else v


class Rec:
    def __init__(self, emu):
        self.emu = emu
        self.core = emu._core

    def run(self, frames=1):
        for _ in range(frames):
            self.core.run_frame()

    def press_release(self, key, held=2):
        self.core.add_keys(self.emu.KEY[key])
        self.run(held)
        self.core.clear_keys(self.emu.KEY[key])

    def tap(self, key, held=2, settle=2, run_until_idle=True, max_extra=200):
        self.press_release(key, held)
        self.run(settle)
        if run_until_idle:
            extra = 0
            while self.read_phase() != PHASE_INPUT and extra < max_extra:
                self.run(1)
                extra += 1

    def read_player(self):
        return s16(self.emu.mem_u16(ADDR_PLAYER_X)), s16(self.emu.mem_u16(ADDR_PLAYER_Y))

    def read_facing(self):
        return self.emu.mem_u8(ADDR_PLAYER_FACING)

    def read_hp(self):
        return self.emu.mem_u8(ADDR_PLAYER_HP)

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


def facing_key_toward(dx, dy):
    if abs(dx) >= abs(dy):
        return "RIGHT" if dx > 0 else "LEFT"
    return "DOWN" if dy > 0 else "UP"


def face(rec, key):
    """Gira hacia `key` (solo LEFT/RIGHT importan para el ataque, tap-to-turn
    es horizontal) SOLO si el jugador no miraba ya hacia ahí -- pulsar una
    segunda vez en la dirección a la que ya se mira MUEVE (consume turno),
    lo que aquí sería un paso indeseado sobre/a través de la casilla del
    enemigo (hueco conocido, ver seek_death) en vez de solo girar. Mismo
    patrón que tools/phantom-debug/capture_attack_sides.py."""
    want = FACING_OF_KEY.get(key)
    if want is not None and rec.read_facing() == want:
        return
    rec.tap(key)


def move_one(rec, key, **kw):
    """Ejecuta UN paso de rejilla en `key`, respetando tap-to-turn:
    ARRIBA/ABAJO mueven directamente (rec.tap ya basta); IZQUIERDA/DERECHA
    necesitan face() primero (gira gratis si hacía falta) y LUEGO un tap
    real -- si ya se miraba hacia ahí, face() no hizo nada y este tap es el
    que mueve; si face() giró, este tap es la "segunda pulsación,  ya
    mirando hacia allí" que sí mueve."""
    if key in ("LEFT", "RIGHT"):
        face(rec, key)
        rec.tap(key, **kw)
    else:
        rec.tap(key, **kw)


def move_one_watch_teleport(rec, key):
    """Como move_one, pero SIN esperar a que el turno se resuelva del todo
    (run_until_idle=False) -- si el paso pisa la escalera desbloqueada, el
    tap() normal se tragaría la animación de teleport + fundido + warp
    ENTERA dentro de su espera (igual que pasaba con la muerte en
    seek_death), así que aquí también hay que vigilar la fase frame a frame
    y devolver el control en cuanto sea PLAYER_TELEPORT. Devuelve la fase
    alcanzada (PHASE_INPUT si el paso fue un movimiento normal, o
    PHASE_TELEPORT si pisó la escalera)."""
    if key in ("LEFT", "RIGHT"):
        face(rec, key)   # gira gratis si hacía falta (no dispara nada por sí solo)
    rec.press_release(key)
    if not run_until(rec, lambda r: r.read_phase() != PHASE_INPUT, max_frames=10):
        return PHASE_INPUT   # giro gratis (tap-to-turn): no se consumió turno
    if rec.read_phase() == PHASE_TELEPORT:
        return PHASE_TELEPORT
    run_until(rec, lambda r: r.read_phase() in (PHASE_INPUT, PHASE_TELEPORT), max_frames=60)
    return rec.read_phase()


def tile_of(px, py):
    return (px // TILE, py // TILE)


def kill_enemy_bfs_step(rec, ex, ey):
    """UN paso hacia la casilla adyacente en horizontal más cercana al
    enemigo en (ex, ey) (píxeles), recalculando el camino (BFS sobre
    ROOM_SOLID, esquivando muros de verdad) cada vez -- el enemigo puede
    haberse movido desde la última llamada. El golpe solo sale a
    izquierda/derecha (vista de perfil pura), así que NUNCA se aproxima por
    arriba/abajo para atacar; si ya está en la casilla adyacente horizontal,
    ataca directamente en vez de recalcular un camino de longitud 0."""
    etx, ety = tile_of(ex, ey)
    px, py = rec.read_player()
    ptx, pty = tile_of(px, py)

    if pty == ety and abs(etx - ptx) == 1:
        key = "RIGHT" if etx > ptx else "LEFT"
        face(rec, key)
        rec.tap("A")
        return

    candidates = [(cx, ety) for cx in (etx - 1, etx + 1)
                  if 0 <= cx < ROOM_W and not ROOM_SOLID[ety][cx]]
    best_path = None
    for c in candidates:
        p = bfs_path((ptx, pty), c)
        if p is not None and (best_path is None or len(p) < len(best_path)):
            best_path = p
    if best_path is None:
        # Sin candidato horizontal alcanzable (no debería pasar en este
        # piso): al menos acercarse a la casilla del enemigo por el camino
        # que sea, para no quedarse parado sin hacer nada.
        best_path = bfs_path((ptx, pty), (etx, ety))
    if not best_path:
        return
    move_one(rec, best_path[0])   # UN solo paso; la siguiente llamada recalcula


def run_until(rec, predicate, max_frames=400):
    """Avanza frame a frame hasta que `predicate(rec)` sea TRUE o se agote
    max_frames. Devuelve TRUE si se cumplio la condicion."""
    for _ in range(max_frames):
        if predicate(rec):
            return True
        rec.run(1)
    return predicate(rec)


def seek_death(rec, max_turns=400):
    """Persigue al enemigo vivo mas cercano SIN atacar (Tarea 7) -- sin
    ataque propio, la unica forma de perder vida es el contacto enemigo.
    Simplemente aproximarse turno a turno basta: en cuanto la distancia
    llega a 1 (adyacente), el propio paso del enemigo hacia el jugador ese
    MISMO turno de enemigos aterriza en la casilla del jugador (ataque, ver
    StartEnemyTurn). El empujón aleja al jugador tras cada golpe, así que la
    aproximación se repite sola turno tras turno hasta que la ventana de
    invulnerabilidad (SIMA_HIT_INVULN_FRAMES) vuelve a dejar pasar otro
    golpe.

    A diferencia de un simple bucle de tap(run_until_idle=True), este avanza
    con run_until_idle=False y comprueba la fase DESPUES de cada pulsación,
    frame a frame -- así puede devolver el control exactamente en el
    instante en que sTurnPhase pasa a PLAYER_DEAD, antes de que la animación
    (y el reset automático que la sigue) se lo trague."""
    for turn in range(max_turns):
        enemies = [e for e in rec.read_enemies() if e[0]]
        if not enemies:
            print("  (todos los enemigos murieron sin que los atacara -- no deberia pasar)")
            return False
        px, py = rec.read_player()
        alive, ex, ey = min(enemies, key=lambda e: abs(e[1] - px) + abs(e[2] - py))
        if px == ex and py == ey:
            # Hueco conocido: el paso del jugador no comprueba ocupación de
            # enemigo, así que puede acabar EN su casilla. Cualquier
            # pulsación que consuma turno basta para que el enemigo
            # reaccione.
            rec.press_release("UP")
        else:
            rec.press_release(facing_key_toward(ex - px, ey - py))
        # Esperar a que la fase deje PLAYER_INPUT (arrancó el turno) O a que
        # pase a PLAYER_DEAD directamente -- lo que llegue antes.
        if not run_until(rec, lambda r: r.read_phase() != PHASE_INPUT, max_frames=10):
            continue  # giro gratis (tap-to-turn): no se consumió turno, reintentar
        if rec.read_phase() == PHASE_DEAD:
            return True
        # Turno en curso (MOVE/ATTACK/ENEMY_STEP): dejarlo resolver, pero
        # vigilando fase DEAD en cada frame por si el golpe llega a mitad.
        if run_until(rec, lambda r: r.read_phase() in (PHASE_INPUT, PHASE_DEAD), max_frames=60):
            if rec.read_phase() == PHASE_DEAD:
                return True
    return False


def main():
    emu = Emu(ROM)
    rec = Rec(emu)
    rec.run(BOOT_FRAMES)

    ok = True

    # ------------------------------------------------------------------
    print("--- test 1 y 2: muerte -> animación bloquea input, luego reaparece en spawn con vida llena ---")
    hp0 = rec.read_hp()
    print(f"  vida inicial: {hp0}")
    died = seek_death(rec)
    if not died:
        print("  [FALLO] no se consiguio que la vida llegara a 0 en el limite de turnos")
        ok = False
    else:
        phase_at_death = rec.read_phase()
        hp_at_death = rec.read_hp()
        print(f"  vida al morir: {hp_at_death}  fase: {phase_at_death} (esperado PHASE_DEAD={PHASE_DEAD})")
        if phase_at_death != PHASE_DEAD or hp_at_death != 0:
            print("  [FALLO] no se capturó el instante exacto de PLAYER_DEAD con vida en 0")
            ok = False
        else:
            print("  [OK] vida en 0 y fase en PLAYER_DEAD, capturado en el frame exacto")

            # Test 2 (parte muerte): pulsar durante la animación no mueve.
            before = rec.read_player()
            rec.press_release("LEFT")
            rec.run(10)
            rec.press_release("RIGHT")
            rec.run(10)
            after = rec.read_player()
            phase = rec.read_phase()
            print(f"  antes de pulsar: {before}  después de LEFT/RIGHT (10+10 frames): {after}  fase: {phase}")
            if before != after:
                print("  [FALLO] el jugador se movió durante la animación de muerte")
                ok = False
            else:
                print("  [OK] el jugador NO se movió durante la animación de muerte")
            if phase != PHASE_DEAD:
                print("  [FALLO] la fase cambió sola sin que la animación (SIMA_DEATH_ANIM_FRAMES) terminara")
                ok = False

            # Dejar correr el resto: animación de muerte + fundido out/in +
            # reset. Con margen generoso.
            run_until(rec, lambda r: r.read_phase() == PHASE_INPUT, max_frames=250)
            px, py = rec.read_player()
            hp = rec.read_hp()
            phase = rec.read_phase()
            enemies = rec.read_enemies()
            print(f"  tras dejar correr todo: jugador={(px, py)} (spawn esperado {SPAWN})  vida={hp}  fase={phase}")
            print(f"  enemigos: {enemies}")

            if (px, py) != SPAWN:
                print("  [FALLO] el jugador no reapareció en el spawn del piso")
                ok = False
            else:
                print("  [OK] el jugador reapareció exactamente en el spawn del piso")

            if hp != hp0:
                print(f"  [FALLO] la vida no volvió al máximo ({hp} != {hp0})")
                ok = False
            else:
                print("  [OK] la vida volvió al máximo")

            if phase != PHASE_INPUT:
                print("  [FALLO] sTurnPhase no volvió a PLAYER_INPUT -- el juego se quedó colgado")
                ok = False
            else:
                print("  [OK] sTurnPhase volvió a PLAYER_INPUT (no colgado)")

            if not all(a for a, _, _ in enemies):
                print("  [FALLO] no todos los enemigos volvieron a estar vivos")
                ok = False
            else:
                print("  [OK] los 3 enemigos volvieron a estar vivos")

    # ------------------------------------------------------------------
    print("\n--- test 3: teleport de escalera -- limpiar el piso y bajar ---")
    # OJO: el golpe SOLO sale a izquierda/derecha (vista de perfil pura, ver
    # el comentario de cabecera de src/sima_actors.c) -- así que perseguir
    # con BFS (kill_enemy_bfs_step, arriba) sobre el mapa real en vez de
    # "acercarse por el eje dominante": ese primer intento se quedaba
    # encajado contra un muro en cuanto el enemigo más cercano estaba en
    # otra fila con una pared de por medio.
    print("  limpiando el piso (persiguiendo y atacando a los 3 enemigos)...")
    cleared = False
    for _ in range(400):
        if rec.read_phase() == PHASE_DEAD:
            # Puede morir de camino (no hay invulnerabilidad infinita) --
            # dejar que el reset automático termine y seguir intentando; el
            # test 1/2 ya demostró que ese reset es correcto, aquí solo nos
            # interesa que la limpieza no se cuelgue.
            run_until(rec, lambda r: r.read_phase() == PHASE_INPUT, max_frames=250)
            continue
        enemies = [e for e in rec.read_enemies() if e[0]]
        if not enemies:
            cleared = True
            break
        px, py = rec.read_player()
        alive, ex, ey = min(enemies, key=lambda e: abs(e[1] - px) + abs(e[2] - py))
        kill_enemy_bfs_step(rec, ex, ey)
    print(f"  piso limpio: {cleared}  enemigos: {rec.read_enemies()}  vida: {rec.read_hp()}")

    if not cleared:
        print("  [FALLO] no se consiguió limpiar el piso para probar la escalera")
        ok = False
    else:
        print("  yendo a la escalera...")
        stx, sty = tile_of(*STAIRS)
        phase = rec.read_phase()
        for _ in range(60):
            if phase == PHASE_TELEPORT:
                break
            px, py = rec.read_player()
            ptx, pty = tile_of(px, py)
            if (ptx, pty) == (stx, sty):
                break
            path = bfs_path((ptx, pty), (stx, sty))
            if not path:
                break
            phase = move_one_watch_teleport(rec, path[0])
        px, py = rec.read_player()
        print(f"  posición sobre la escalera: {(px, py)} (esperado {STAIRS})")

        phase = rec.read_phase()
        print(f"  fase tras pisar la escalera: {phase} (esperado {PHASE_TELEPORT})")
        if phase != PHASE_TELEPORT:
            print("  [FALLO] la animación de teleport no arrancó al pisar la escalera desbloqueada")
            ok = False
        else:
            print("  [OK] la animación de teleport arrancó")
            before = rec.read_player()
            rec.press_release("UP")
            rec.run(10)
            after = rec.read_player()
            print(f"  posición antes/después de pulsar UP durante el teleport: {before} / {after}")
            if before != after:
                print("  [FALLO] el jugador se movió durante la animación de teleport")
                ok = False
            else:
                print("  [OK] el jugador NO se movió durante la animación de teleport")

            run_until(rec, lambda r: r.read_phase() == PHASE_INPUT, max_frames=250)
            final_phase = rec.read_phase()
            final_pos = rec.read_player()
            print(f"  tras el teleport completo: fase={final_phase} (esperado {PHASE_INPUT})  jugador={final_pos}")
            if final_phase != PHASE_INPUT:
                print("  [FALLO] el estado no quedó coherente tras el teleport (sTurnPhase no volvió a PLAYER_INPUT)")
                ok = False
            else:
                print("  [OK] el estado quedó coherente tras el teleport")
            if final_pos != SPAWN:
                print("  [FALLO] el jugador no apareció en el spawn del piso tras el teleport")
                ok = False
            else:
                print("  [OK] el jugador apareció en el spawn del piso tras el teleport (SIMA_FLOOR_COUNT=1: mismo piso)")

    print("\n" + ("TODAS LAS VERIFICACIONES PASARON" if ok else "ALGUNA VERIFICACIÓN FALLÓ"))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
