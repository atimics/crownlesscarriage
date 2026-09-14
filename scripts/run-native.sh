#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD_DIR=${CROWNLESS_BUILD_DIR:-"$ROOT/out/build/release"}
APP="$BUILD_DIR/crownless_carriage.app"
BINARY="$APP/Contents/MacOS/crownless_carriage"
CACHE=${CROWNLESS_VOICE_CACHE:-"$HOME/.cache/crownless/speech-v1"}
VOICE_PORT=${CROWNLESS_VOICE_PORT:-8766}
VOICE_PID=

if [ ! -x "$BINARY" ]; then
    echo "Native build not found: $BINARY" >&2
    echo "Build it with: cmake --preset release && cmake --build out/build/release --parallel" >&2
    exit 1
fi

if command -v curl >/dev/null 2>&1 && curl -fsS --max-time 1 "http://127.0.0.1:$VOICE_PORT/health" >/dev/null 2>&1; then
    echo "Crownless voice worker: already running on port $VOICE_PORT"
else
    PYTHON=${CROWNLESS_PYTHON:-}
    if [ -z "$PYTHON" ] && [ -x "$ROOT/.venv/crownless-voice/bin/python" ]; then
        PYTHON="$ROOT/.venv/crownless-voice/bin/python"
    fi
    if [ -z "$PYTHON" ] && command -v python3.11 >/dev/null 2>&1; then
        PYTHON=$(command -v python3.11)
    fi
    if [ -n "$PYTHON" ] && "$PYTHON" -c 'import pocket_tts' >/dev/null 2>&1; then
        ALLOW_DOWNLOAD=${CROWNLESS_VOICE_ALLOW_DOWNLOAD:-1}
        set -- "$PYTHON" "$ROOT/tools/audio/speech_worker.py" \
            --engine pocket --port "$VOICE_PORT" --cache "$CACHE"
        if [ "$ALLOW_DOWNLOAD" = 1 ]; then
            set -- "$@" --allow-download
        fi
        PYTHONPATH="$ROOT/tools/audio" "$@" &
        VOICE_PID=$!
        trap 'kill "$VOICE_PID" 2>/dev/null || true' EXIT INT TERM
        echo "Crownless voice worker: starting on port $VOICE_PORT"
    else
        echo "Crownless voice worker: unavailable; captions and packaged audio remain enabled"
    fi
fi

cd "$ROOT"
exec "$BINARY"
