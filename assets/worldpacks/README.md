# Worldpack language assets

Language packs live under `languages/v3`. Each JSON file has a stable `id`, a
pack `version`, a `lexicon`, and complete templates keyed by v3 meaning intent.
The meaning renderer loads a pack by its file name and keeps typed values in the
act. Names and numbers are copied into the selected clause template.

Packs are data. Add a new language by copying a pack, changing its `id`, and
providing every template needed by the intent set. A test pack can live in a
temporary directory and pass through `render(..., pack_root=...)`.
