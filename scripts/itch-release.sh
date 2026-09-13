#!/bin/sh
# Build and push the Crownless Carriage browser bundle to itch.io.
#
# Usage:
#   scripts/itch-release.sh          # build only (dry run)
#   scripts/itch-release.sh --push   # build and upload to itch.io
#
# Requirements:
#   - Emscripten on PATH (emcmake, emmake)
#   - butler installed and logged in (butler login)
#   - The itch project must already exist at the ITCH_USER/ITCH_GAME URL below.

set -eu

ITCH_USER="${ITCH_USER:-ratimics}"
ITCH_GAME="${ITCH_GAME:-crownless-carriage}"
ITCH_CHANNEL="${ITCH_CHANNEL:-html}"
VERSION="${VERSION:-0.1.0}"
SITE_DIR="out/build/web/site"

if command -v butler >/dev/null 2>&1; then
    BUTLER=butler
else
    BUTLER="$HOME/.local/bin/butler"
fi
if [ ! -x "$(command -v "$BUTLER" 2>/dev/null || echo "$BUTLER")" ]; then
    echo "error: butler not found. Install it from https://itch.io/docs/butler/" >&2
    exit 1
fi

echo "==> Building web bundle"
make build-web

for f in index.html index.js index.wasm; do
    if [ ! -f "$SITE_DIR/$f" ]; then
        echo "error: $SITE_DIR/$f is missing" >&2
        exit 1
    fi
done

echo "==> Bundle ready: $SITE_DIR ($(du -sh "$SITE_DIR" | cut -f1))"

if [ "${1:-}" != "--push" ]; then
    echo "==> Dry run. Re-run with --push to upload $ITCH_USER/$ITCH_GAME:$ITCH_CHANNEL"
    exit 0
fi

"$BUTLER" push "$SITE_DIR" "$ITCH_USER/$ITCH_GAME:$ITCH_CHANNEL" \
    --userversion "$VERSION"
echo "==> Uploaded $ITCH_USER/$ITCH_GAME:$ITCH_CHANNEL version $VERSION"