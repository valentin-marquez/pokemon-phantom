#!/usr/bin/env bash
# Lee una expresión C del ROM en marcha vía el stub GDB de mGBA (BEST-EFFORT).
#
# Requiere un emulador con stub GDB escuchando en :2345, p.ej.:
#   mgba-qt pokeemerald_modern.gba -g &      # mgba-qt AÚN NO instalado en este entorno
#
# Uso: test/gdb-read.sh 'gSaveBlock1Ptr->vars[0x404E-0x4000]'   # VAR_PHANTOM_TIME
#
# Patrón: pone un breakpoint en un símbolo que corre a menudo (para tener un frame
# válido), continúa hasta alcanzarlo y evalúa la expresión estando el target DETENIDO.
# NO usa `continue &`/`interrupt` (que en modo all-stop no hace background y cuelga).
# Solo breakpoints + lecturas: los watchpoints del stub de mGBA están rotos.
#
# NOTA: no verificado end-to-end todavía (falta mgba-qt en este entorno). La
# verificación autoritativa del proyecto es el smoke test (./test/smoke.sh); este
# helper es una comodidad para inspección manual cuando haya GUI.
set -euo pipefail
cd "$(dirname "$0")/.."
EXPR="${1:?uso: gdb-read.sh '<expr C>'   (ej: 'gSaveBlock1Ptr->vars[0x404E-0x4000]')}"
BREAK_AT="${GDB_BREAK_AT:-CB2_Overworld}"   # símbolo frecuente; override con env GDB_BREAK_AT
timeout "${GDB_TIMEOUT:-30}" gdb -q -batch pokeemerald_modern.elf \
  -ex 'target remote localhost:2345' \
  -ex "break $BREAK_AT" \
  -ex 'continue' \
  -ex "print $EXPR" \
  -ex 'detach' 2>&1 | grep -E '\$[0-9]+ =' || {
    echo "gdb-read: sin resultado (¿emulador con -g en :2345? ¿se alcanzó $BREAK_AT? ¿timeout?)" >&2
    exit 1
  }
