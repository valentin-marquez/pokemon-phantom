#!/usr/bin/env bash
# Lee una expresión C del ROM en marcha vía el stub GDB de mGBA.
# Uso: test/gdb-read.sh 'VarGet(VAR_PHANTOM_TIME)'  (con mgba-qt ... -g corriendo)
# Requiere: mgba-qt pokeemerald_modern.gba -g &   (stub en :2345)
set -euo pipefail
cd "$(dirname "$0")/.."
EXPR="${1:?uso: gdb-read.sh '<expr C>'}"
gdb -q -batch pokeemerald_modern.elf \
  -ex 'target remote localhost:2345' \
  -ex 'continue &' -ex 'interrupt' \
  -ex "print $EXPR" \
  -ex 'detach' 2>&1 | grep -A1 "\$"
