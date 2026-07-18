#!/usr/bin/env bash
# Smoke test headless: build de test + mgba-rom-test + parseo de checkpoints.
set -euo pipefail
cd "$(dirname "$0")/.."

ROM=pokeemerald_modern.gba
ROMTEST=tools/mgba/mgba-rom-test

echo ">> build PHANTOM_TEST=1"
make PHANTOM_TEST=1 DINFO=1 modern -j"$(nproc)"

echo ">> run $ROMTEST"
LOG=$(mktemp)
set +e
stdbuf -oL "$ROMTEST" -S 0x27 -R r0 -l 15 "$ROM" | tee "$LOG"
EXIT=${PIPESTATUS[0]}
set -e

echo ">> checkpoints:"
grep ':P' "$LOG" || { echo "!! sin checkpoints (¿NDEBUG/log?)"; rm -f "$LOG"; exit 2; }

if grep -q ':P FAIL' "$LOG"; then echo "SMOKE: FAIL"; rm -f "$LOG"; exit 1; fi
if [ "$EXIT" -ne 0 ]; then echo "SMOKE: exit=$EXIT (!=0)"; rm -f "$LOG"; exit "$EXIT"; fi
echo "SMOKE: OK"
rm -f "$LOG"
