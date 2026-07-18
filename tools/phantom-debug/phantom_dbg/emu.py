"""Driver headless de mGBA para Pokémon Phantom (verificado con libmgba)."""
import mgba.core
import mgba.image
import mgba.log

mgba.log.silence()  # mata el spam de BIOS/DMA


class Emu:
    def __init__(self, rom_path, savestate=None):
        self._core = mgba.core.load_path(rom_path)
        if self._core is None:
            raise RuntimeError(f"no se pudo cargar la ROM: {rom_path}")
        w, h = self._core.desired_video_dimensions()
        self._img = mgba.image.Image(w, h)
        self._core.set_video_buffer(self._img)   # ANTES de reset()
        self._core.reset()
        if savestate is not None:
            self.load_state(savestate)

    # --- teclas ---
    @property
    def KEY(self):
        c = self._core
        return {"A": c.KEY_A, "B": c.KEY_B, "START": c.KEY_START,
                "SELECT": c.KEY_SELECT, "UP": c.KEY_UP, "DOWN": c.KEY_DOWN,
                "LEFT": c.KEY_LEFT, "RIGHT": c.KEY_RIGHT, "L": c.KEY_L, "R": c.KEY_R}

    def run(self, frames):
        for _ in range(frames):
            self._core.run_frame()

    def press(self, key, held=2, release=10):
        k = self.KEY[key] if isinstance(key, str) else key
        self._core.add_keys(k)
        self.run(held)
        self._core.clear_keys(k)
        self.run(release)

    # --- video ---
    def screenshot(self, path):
        with open(path, "wb") as f:
            self._img.save_png(f)
        return path

    # --- memoria (bus base-0: direcciones GBA directas) ---
    def mem_u8(self, addr):  return self._core.memory.u8[addr]
    def mem_u16(self, addr): return self._core.memory.u16[addr]
    def mem_u32(self, addr): return self._core.memory.u32[addr]

    # --- savestate ---
    def save_state(self, path):
        with open(path, "wb") as f:
            f.write(bytes(self._core.save_raw_state()))

    def load_state(self, path):
        import mgba._pylib
        with open(path, "rb") as f:
            data = f.read()
        buf = mgba._pylib.ffi.new("unsigned char[]", data)
        self._core.load_raw_state(buf)

    @property
    def game_title(self): return self._core.game_title
