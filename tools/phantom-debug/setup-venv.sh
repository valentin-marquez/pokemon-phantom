#!/usr/bin/env bash
# Requiere: sudo pacman -S --needed libmgba  (una vez; ya instalado)
set -euo pipefail
VENV="${PHANTOM_VENV:-$HOME/.venvs/mgba-py}"
uv venv --python 3.11 "$VENV"
uv pip install --python "$VENV" -r "$(dirname "$0")/requirements.txt"
echo "venv listo: $VENV  (usa: $VENV/bin/python -m phantom_dbg ...)"
