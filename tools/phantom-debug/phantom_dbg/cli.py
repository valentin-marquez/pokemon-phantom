"""CLI del harness de debug visual: screenshot/boot/read sobre la ROM debug.

La ROM por defecto es la de PHANTOM_DEBUG_BOOT (arranca directo al overworld,
sin navegar título ni minijuego) -- ver Makefile y src/intro.c.
"""
import argparse

from .emu import Emu
from .symbols import SymbolReader

DEF_ROM = "pokeemerald_modern_debug.gba"
DEF_MAP = "pokeemerald_modern_debug.map"
DEF_ELF = "pokeemerald_modern_debug.elf"


def boot(emu, frames=300):
    """La ROM debug (PHANTOM_DEBUG_BOOT) arranca directo en el overworld: solo avanza frames."""
    emu.run(frames)


def main(argv=None):
    p = argparse.ArgumentParser(prog="phantom_dbg")
    p.add_argument("--rom", default=DEF_ROM)
    p.add_argument("--map", default=DEF_MAP)
    p.add_argument("--elf", default=DEF_ELF)
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("screenshot")
    s.add_argument("out")
    s.add_argument("--frames", type=int, default=300)
    b = sub.add_parser("boot")
    b.add_argument("--screenshot")
    b.add_argument("--frames", type=int, default=300)
    b.add_argument("--read", action="append", default=[])
    r = sub.add_parser("read")
    r.add_argument("kind", choices=["var", "flag"])
    r.add_argument("id")
    r.add_argument("--frames", type=int, default=300)
    args = p.parse_args(argv)

    emu = Emu(args.rom)
    emu.run(args.frames)
    if args.cmd == "screenshot":
        print(emu.screenshot(args.out))
    elif args.cmd == "boot":
        if args.screenshot:
            print(emu.screenshot(args.screenshot))
        if args.read:
            sr = SymbolReader(emu, args.map, args.elf)
            for tok in args.read:
                print(tok, "=", sr.read_var(int(tok, 0)))
    elif args.cmd == "read":
        sr = SymbolReader(emu, args.map, args.elf)
        v = sr.read_var(int(args.id, 0)) if args.kind == "var" else sr.read_flag(int(args.id, 0))
        print(v)
    return 0
