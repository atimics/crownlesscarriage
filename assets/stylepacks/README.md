# Style packs

Each subdirectory is one style pack: a `style.json` manifest plus, optionally,
the pack's own shaders. A style pack is Crownless's answer to Sierra's
AGI/SCI split -- one engine, and the game's look chosen by data instead of
compiled in. See `docs/design/style-packs.md` for the manifest schema, the
validation rules, and the design for later (live, non-pre-rendered) painterly
passes.

Pick a pack with `--style <id>` or the `CROWNLESS_STYLE` environment
variable; `classic` is the default and is what a plain `crownless_carriage`
invocation has always looked like. A style path in a manifest is relative to
that pack's own directory. A pack that does not change a given shader may
reuse the shared original by pointing at it with a relative path such as
`../../shaders/world_lit.fs`, rather than shipping a byte-identical copy --
`classic` does this for every shader, and `sierra_pixel` does it for every
shader except the two it changes.

- `classic/` -- reproduces the game's original look exactly. Its manifest is
  the data-driven mirror of the constants `src/client/cc_style_pack.c` also
  carries as a compiled-in fallback, used if no pack can be loaded at all.
- `sierra_pixel/` -- classic plus an ordered-dither pass (`ditherStrength`)
  in `shaders/world_lit.fs` and `shaders/painted_environment.fs`, in the
  spirit of a King's Quest-era palette. Nothing else changes.

A broken pack (an unknown field, a missing shader role, an unsupported
`schema_version`, a shader path that does not resolve) is rejected and logged
by `CcStylePackLoad()` (`src/client/cc_style_pack.c`), which then falls back
to the compiled-in classic pack -- a broken style pack should never produce a
black screen. `tools/validate_style_packs.py` runs the same checks, in
Python, as a fast CI gate that does not require building the client.
