# itch.io release

The browser build of Crownless Carriage is published on itch.io as an HTML
game. One command builds and uploads:

```sh
scripts/itch-release.sh --push          # build + upload
scripts/itch-release.sh                 # build only (dry run)
```

## One-time setup

1. Install butler (do NOT use `brew install butler` — that cask is a
   different application). Download the real one from itch:

   ```sh
   mkdir -p ~/.local/bin
   curl -sL -o /tmp/butler.zip "https://broth.itch.zone/butler/darwin-arm64/15.31.0/archive/default" \
     && unzip -o /tmp/butler.zip -d ~/.local/bin
   butler login
   ```

2. Create the project on itch.io: dashboard -> "Create new project", and set
   the URL to `crownless` so it lives at `ratimics.itch.io/crownless`.

3. Project settings for the browser build:

   - Kind of project: **HTML**
   - Upload channel: `html` (what the script pushes)
   - Check "This file will be played in the browser"
   - Embed size: 1280 x 720 (the client is fixed 16:9)
   - If the browser console reports a `SharedArrayBuffer` error, enable the
     "SharedArrayBuffer support" toggle on the upload.

## Release checklist

- `make build-web` passes and the bundle in `out/build/web/site` starts.
- Bump `VERSION` in `scripts/itch-release.sh` (or pass `VERSION=x.y.z`) for
  each upload so itch can track patches.
- Push, then set the new version live on the dashboard if "block new patches
  until you test them" is enabled.