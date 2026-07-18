# phantom-debug

Harness headless de debugging visual para Pokémon Phantom, sobre `mgba` (bindings Python de libmgba). Permite cargar la ROM, avanzar frames, mandar inputs, sacar screenshots PNG, leer/escribir memoria y savestates — todo sin X/Qt/gstreamer. Es una herramienta versionada de desarrollo, no se distribuye con la ROM.

## Setup (una vez)

```bash
sudo pacman -S --needed libmgba   # una vez; probablemente ya instalado
./setup-venv.sh                  # crea ~/.venvs/mgba-py e instala requirements.txt (usa uv)
```

`setup-venv.sh` acepta `PHANTOM_VENV` para elegir otra ruta de venv.

## Uso

Corré scripts con `PYTHONPATH=tools/phantom-debug` apuntando al intérprete del venv:

```bash
PYTHONPATH=tools/phantom-debug ~/.venvs/mgba-py/bin/python - <<'PY'
from phantom_dbg import Emu

e = Emu("pokeemerald_modern.gba")
e.run(160)
e.screenshot("/tmp/title.png")
print(e.game_title)
PY
```

## API (`phantom_dbg.Emu`)

- `Emu(rom_path, savestate=None)` — carga la ROM (y opcionalmente un savestate) y arranca el core.
- `.press(key, held=2, release=10)` — mantiene una tecla `held` frames y suelta `release` frames. `key` puede ser un string (`"A"`, `"B"`, `"START"`, `"SELECT"`, `"UP"`, `"DOWN"`, `"LEFT"`, `"RIGHT"`, `"L"`, `"R"`) o una constante `KEY_*` cruda.
- `.run(frames)` — avanza N frames sin input.
- `.screenshot(path)` — guarda el framebuffer actual como PNG.
- `.save_state(path)` / `.load_state(path)` — savestate crudo a/desde disco (incluye RTC).
- `.mem_u8/mem_u16/mem_u32(addr)` — lectura de memoria por bus base-0 (direcciones GBA directas, p.ej. `0x03007328`).
- `.game_title` — título embebido en el header de la ROM.

## Gotchas verificados

- `set_video_buffer(image)` debe llamarse **antes** de `core.reset()`, o los frames renderizan en negro sólido.
- `mgba.log.silence()` es obligatorio para no inundar stdout con el log de BIOS/DMA.
- La memoria es un bus plano base-0 (`core.memory.u16[0x03007328]`, no offsets relativos).
- `load_raw_state` requiere envolver los bytes con `mgba._pylib.ffi.new("unsigned char[]", data)`, no pasar un `bytes`/`bytearray` pelado.

Más detalle de la API verificada: `docs/design/harness-fase2-visual.md`.
