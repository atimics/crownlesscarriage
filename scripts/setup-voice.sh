#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PYTHON=${CROWNLESS_PYTHON:-}
if [ -z "$PYTHON" ] && command -v python3.11 >/dev/null 2>&1; then
    PYTHON=$(command -v python3.11)
fi
if [ -z "$PYTHON" ]; then
    echo "Python 3.11 is required for PocketTTS. Set CROWNLESS_PYTHON to its path." >&2
    exit 1
fi

ENV_DIR=${CROWNLESS_VOICE_ENV:-"$ROOT/.venv/crownless-voice"}
if [ ! -x "$ENV_DIR/bin/python" ]; then
    "$PYTHON" -m venv "$ENV_DIR"
fi
"$ENV_DIR/bin/python" -m pip install --upgrade pip
"$ENV_DIR/bin/python" -m pip install -r "$ROOT/tools/audio/requirements-pocket.txt"
printf 'Voice environment ready: %s\n' "$ENV_DIR"
