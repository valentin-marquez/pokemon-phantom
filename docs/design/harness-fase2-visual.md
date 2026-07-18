# Harness Fase 2 — debugging visual autónomo (verificado)

> Verificado hands-on jul 2026 en este entorno (CachyOS). Da control autónomo del juego headless: cargar ROM, inputs, frames, screenshot PNG, leer/escribir memoria, savestates. Sin X, sin Qt, sin gstreamer. Complementa el smoke test de la Fase 0-1 (ese verifica lógica; esto verifica lo visual/interactivo).

## Setup (una vez)
- **Un solo sudo:** `sudo pacman -S --needed libmgba` (limpio; NO instalar `mgba-qt` — bloqueado por conflicto gstreamer/gst-libav). Ya instalado jul 2026.
- **venv sin sudo:**
  ```bash
  uv venv --python 3.11 ~/.venvs/mgba-py     # el paquete mgba solo tiene wheels cp310/cp311
  uv pip install --python ~/.venvs/mgba-py mgba
  ```
  El paquete PyPI `mgba` (0.10.2) es un wrapper cffi que dlopen'ea el `libmgba.so.0.10` del sistema.

## API clave (paquete `mgba`)
- `core = mgba.core.load_path(rom)`; `mgba.log.silence()` (mata el spam de BIOS/DMA).
- **Orden load-bearing:** `image = mgba.image.Image(*core.desired_video_dimensions()); core.set_video_buffer(image); core.reset()` — el buffer DEBE asignarse ANTES de `reset()` o los frames salen negros.
- Inputs: `core.add_keys(core.KEY_A)` / `core.clear_keys(...)`; `core.run_frame()` (1 frame).
- Screenshot: `image.save_png(fileobj)` (writer propio de mgba, sin PIL).
- Memoria: `core.memory.u8/u16/u32[addr]` (espacio de bus completo, base 0 — direcciones GBA directas: `0x03007328`, etc.).
- Savestate: `core.save_raw_state()` / `core.load_raw_state(buf)` (envolver bytes con `ffi.new("unsigned char[]", data)`). El RTC va dentro del savestate.

## Rendimiento y determinismo
- ~1378 fps (~23× realtime) headless. Determinismo verificado in-process y cross-process (savestate a disco → otro proceso reanuda al mismo frame).
- **RTC:** Emerald lee el reloj del host por defecto. Estrategia simple y ya verificada: savestate + secuencia de inputs fija (el savestate incluye el RTC). Existe override `mCoreSetRTC(RTC_FIXED)` pero no trazado E2E.

## Leer estado por símbolo
- **Globales:** `.map` / `arm-none-eabi-nm` dan la dirección fija (p.ej. `gSaveBlock1Ptr @ 0x03007328`, un puntero cuyo valor se lee en vivo tras el boot).
- **Offsets de struct:** el `arm-none-eabi-gdb` de este toolchain tiene el DWARF5 roto (`DW_FORM_line_strp used without required section`). Usar **`pyelftools`** (lee el mismo DWARF limpio) para resolver `SaveBlock1.field → offset` (verificado: `vars` = 0x139C, igual que el comentario del header). Fallback manual: los headers auto-documentan offsets en comentarios (`/*0x139C*/ u16 vars[...]`).
- Ejemplo: `ptr = u32[0x03007328]; vars = ptr + 0x139C; VAR = u16[vars + 2*(0x404E-0x4000)]`.

## Gotchas
- **DWARF stale:** `DINFO=1` no fuerza recompilar archivos sin cambios → el `.elf` puede no tener DWARF de código de juego. Para depender de DWARF: `make clean && make DINFO=1 modern` (o tocar el archivo). Para el smoke test normal no hace falta.
- `set_video_buffer` antes de `reset()` (ver arriba).
- `load_raw_state` exige `ffi.new("unsigned char[]", data)`, no un `bytearray` pelado.

## Rol vs. otras piezas
- **Smoke test (`./test/smoke.sh`)**: pass/fail de lógica headless, autoritativo. No cambia.
- **Este harness (mgba-py)**: verificación visual/interactiva (mapas, escenas, la desaturación de paleta) e inspección de estado en vivo.
- **GDB stub** (`mgba-sdl -g`): complemento opcional para breakpoints; `mgba-sdl` instala limpio si se quiere (no requerido para el path Python).
