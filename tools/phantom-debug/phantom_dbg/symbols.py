"""Resuelve símbolos y offsets de struct para leer estado del juego."""
import re
from elftools.elf.elffile import ELFFile

VARS_START = 0x4000   # include/constants/vars.h
FLAGS_START = 0x0      # los flags se indexan desde 0 en el bit-array


class SymbolReader:
    def __init__(self, emu, map_path, elf_path):
        self.emu = emu
        self._globals = self._parse_map(map_path)
        self._elf_path = elf_path
        self._offset_cache = {}

    def _parse_map(self, map_path):
        g = {}
        # líneas tipo: "                0x03007328                gSaveBlock1Ptr"
        pat = re.compile(r"^\s+0x([0-9a-fA-F]{8,})\s+(\w+)\s*$")
        with open(map_path) as f:
            for line in f:
                m = pat.match(line)
                if m:
                    g.setdefault(m.group(2), int(m.group(1), 16))
        return g

    def global_addr(self, name):
        return self._globals[name]

    def struct_offset(self, struct, field):
        key = (struct, field)
        if key in self._offset_cache:
            return self._offset_cache[key]
        with open(self._elf_path, "rb") as f:
            dwarf = ELFFile(f).get_dwarf_info()
            for cu in dwarf.iter_CUs():
                for die in cu.iter_DIEs():
                    if (die.tag == "DW_TAG_structure_type"
                            and die.attributes.get("DW_AT_name")
                            and die.attributes["DW_AT_name"].value == struct.encode()):
                        for ch in die.iter_children():
                            if (ch.tag == "DW_TAG_member"
                                    and ch.attributes.get("DW_AT_name")
                                    and ch.attributes["DW_AT_name"].value == field.encode()):
                                off = ch.attributes["DW_AT_data_member_location"].value
                                self._offset_cache[key] = off
                                return off
        raise KeyError(f"offset no encontrado: {struct}.{field}")

    # --- alto nivel ---
    def _sb1(self):
        return self.emu.mem_u32(self.global_addr("gSaveBlock1Ptr"))

    def read_var(self, var_id):
        base = self._sb1() + self.struct_offset("SaveBlock1", "vars")
        return self.emu.mem_u16(base + 2 * (var_id - VARS_START))

    def read_flag(self, flag_id):
        base = self._sb1() + self.struct_offset("SaveBlock1", "flags")
        byte = self.emu.mem_u8(base + (flag_id >> 3))
        return bool(byte & (1 << (flag_id & 7)))

    def player_map(self):
        base = self._sb1() + self.struct_offset("SaveBlock1", "location")
        grp = self.emu.mem_u8(base + self.struct_offset("WarpData", "mapGroup"))
        num = self.emu.mem_u8(base + self.struct_offset("WarpData", "mapNum"))
        return (grp, num)

    def player_xy(self):
        base = self._sb1() + self.struct_offset("SaveBlock1", "location")
        x = self.emu.mem_u16(base + self.struct_offset("WarpData", "x"))
        y = self.emu.mem_u16(base + self.struct_offset("WarpData", "y"))
        return (x, y)
