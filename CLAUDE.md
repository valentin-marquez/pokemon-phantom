# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

**Pokémon Phantom** is a ROM hack built on the [pokeemerald](https://github.com/pret/pokeemerald) decompilation of Pokémon Emerald (GBA). It is a from-scratch hack on *vanilla* pokeemerald (single `include/config.h`, **not** pokeemerald-expansion). The project's own content/story is in Spanish; the surrounding decomp code and docs are the upstream English ones.

The output is a Game Boy Advance ROM (`pokeemerald.gba`), assembled from C, ARM/THUMB assembly, and a large amount of declarative data (constants, JSON maps, PNG graphics, MIDI songs).

**It is a horror game, not a monster-collecting game.** Phantom is a linear, narrative, ~4-5h psychological-horror experience set on the island of Sombraluna — no grinding, no gyms, never leaves the island. Explicit horror is built from atmosphere, text, and engine effects (palette shifts, mosaic, scanline, sound), **never gore tilesets**. Narrative reference: *Fear & Hunger*. Design lives in docs, not (yet) in code — read these before touching game content or story:

- `docs/superpowers/specs/2026-07-17-pokemon-phantom-design.md` — **the design spec** (story, cast, systems, scope, cut list). Start here.
- `docs/design/informe-fear-and-hunger.md` — narrative principles + engine techniques.
- `docs/design/factibilidad-y-harness.md` — per-system feasibility with verified `file:line` hooks, and the **AI testing harness** (how to build/boot/drive/debug the ROM headlessly — mGBA + GDB stub + `mgba-rom-test` + libmgba-py).

Two standing rules for this project: (1) borrow from references **by function, never by literal object** — nothing lifted straight from F&H; re-encarnate it in something native to Sombraluna. (2) the `arm-none-eabi-gcc` toolchain is installed and `make modern` builds a working ROM (Phase 0 is done) — `make PHANTOM_TEST=1 modern` builds a separate test artifact (`pokeemerald_modern_test.gba`) with the in-ROM harness compiled in; `make modern` alone stays the clean release build.

## Build & run

The toolchain assumes `arm-none-eabi-*` binaries on PATH (or via `$DEVKITARM`). `make tools` and asset generation run automatically as part of a normal build.

```sh
make -j$(nproc)          # non-modern (matching) build with agbcc     -> pokeemerald.gba
make modern -j$(nproc)   # modern build with arm-none-eabi-gcc        -> pokeemerald_modern.gba
make compare -j$(nproc)  # non-modern build + verify against rom.sha1
make tools               # build the host tools in tools/ only
make clean               # remove ROM, objects, tools, and generated assets
make tidy                # remove ROM + objects but keep tools/assets (fast rebuild)
```

Convenience scripts: `./run.sh` = `make -j12`. `./build_tools.sh` is **deprecated** — use `make tools`.

- `make` / `make compare` use **agbcc** (`tools/agbcc`, tracked as a subproject) and are the *matching* build — they aim to reproduce the original ROM byte-for-byte (`rom.sha1` = `f3ae088181bf...`). agbcc is a period compiler; matching requires its exact codegen quirks.
- `make modern` uses the system `arm-none-eabi-gcc`. This is what the VS Code default build task and the debugger target (`pokeemerald_modern.elf`), so **modern is the day-to-day iteration build** for this hack.
- `MODERN`, `COMPARE`, and other flags are set implicitly by the `modern`/`compare` goals; you rarely pass them by hand. `KEEP_TEMPS=1` writes `.i`/`.s` intermediates for a C file; `DINFO=1` adds debug info (the VS Code modern task uses it).

There is **no test suite and no linter** — correctness is verified by the ROM building, and for matching builds by `make compare` passing the sha1 check. When touching non-modern code, a matching build breaking (compare fails / `asmdiff` shows a delta) *is* the failing signal.

## Debugging & diffing

- **mGBA + GDB (VS Code):** the "Debug with mGBA" launch config runs `make modern` (via `DINFO=1`), starts mGBA's GDB stub on `:2345` through `.vscode/mgba-gdb.sh`, and attaches `arm-none-eabi-gdb`. Requires `mgba-qt` and `arm-none-eabi-gdb` installed.
- **`./asmdiff.sh <addr> <len>`** disassembles the same address range from `baserom.gba` and `pokeemerald.gba` and diffs them — the standard tool for chasing a non-matching function. (`asmdiff.ps1` is the PowerShell equivalent.) Needs a `baserom.gba` present.

## Architecture: how source becomes a ROM

The decomp is **heavily data-driven and preprocessed** — most edits are to data, not imperative code, and several source forms are generated at build time.

**Source layout:**
- `src/*.c` — game logic (~300 files). Headers in `include/`, matching filename.
- `asm/*.s`, `data/*.s` — hand-written / raw-data assembly still to be decompiled or intentionally kept as asm. `*.inc.c` files are `#include`d into other `.c` files, not compiled directly.
- `include/constants/*.h` and `constants/*.inc` — symbolic constants shared between C (`.h`) and assembly (`.inc`) for the same game concepts.
- `data/` — declarative game data: `scripts/` and `event_scripts.s` (map/event scripting), `maps/` + `layouts/` (JSON map data), `text/`, `tilesets/`, `specials.inc`, battle/AI script tables.
- `graphics/` — source PNG/`.pal` art; `sound/` — songs (`.mid`) and voice/direct-sound data.
- `libagbsyscall/` — small syscall lib linked into the ROM; built as its own sub-make.

**Build pipeline (see `Makefile`):** each `src/*.c` is run through `cpp` → **`preproc`** (applies `charmap.txt`, turning game text/string literals into GBA character bytes) → the C compiler (agbcc or cc1) → `as`. `scaninc` computes dependencies since the compiler can't. The linker uses `ld_script.ld` (non-modern) or `ld_script_modern.ld` (modern) plus generated RAM symbol scripts from `sym_bss.txt` / `sym_common.txt` / `sym_ewram.txt`.

**Generated assets** (the `*_rules.mk` files, included by the Makefile): graphics PNG→`.4bpp`/`.gbapal` etc. via `gbagfx`; JSON maps→`.inc` via `mapjson` (the `generated` target, run before dependency scanning); MIDI→`.s` via `mid2agb`; JSON→C via `jsonproc`. These outputs are build artifacts, **not** committed — edit the PNG/JSON/MIDI source, never the generated `.inc`/`.4bpp`/`.bin`. `make clean-assets` removes them.

**Practical implication:** to add or change game content (a Pokémon stat, a move, a map, an item, dialogue), the change is almost always to a data table / constant header / JSON / PNG under `data/`, `constants/`, `include/constants/`, or `graphics/` — and often needs a matching entry added in several parallel files (a `constants/*.h` enum + a `data/*.h` table entry + graphics). Grep the constant name across `include/constants`, `src/data`, and `data/` to find every place a new entry must be registered.

## Editor / LSP

`.vscode/c_cpp_properties.json` seeds include paths for the MS C/C++ extension. For clangd, generate `compile_commands.json` with `bear -- make modern` (see `.vscode/README.md`). agbcc is a non-standard C dialect (`-std=gnu89 -nostdinc` against `tools/agbcc/include`), so IntelliSense against non-modern builds is imperfect; prefer indexing the modern build.
