# Reproducible semantic 5M training

The `Semantic 5M training` workflow runs on semantic model release tags and on
manual dispatch. It compares the dialogue sources with the previous release,
trains on a CPU runner, and keeps the full run directory and logs for 14 days.
The model artifact is published only after every test row matches the authored
policy and the native C probe produces the same semantic IDs as Python.

The workflow checks out ZERO from the public repository at
`49e6bfedc6d91196e652d89309b23cafd8fc2ce9`. It pins torch 2.8.0, numpy 2.2.6,
and tokenizers 0.22.0. ZERO stays a separate checkout because that repository
does not provide a license for vendoring.

For a local run, create a Python 3.12 environment and install torch 2.8.0 from
the CPU index, then install
`tools/dialogue/requirements-semantic-training.txt`. Point `--zero` at the
same pinned ZERO checkout. Use the tracked
`models/dialogue-syntax/model.ccv2` and `assets/language/tokenizer.json` inputs.
The normal 800-step CPU run takes about 90 seconds on the reference machine.

Build native libraries with `CC_BUILD_CLIENT=OFF`, then run
`tools/dialogue/build_syntax_probe.py`. The probe invocation is:

```sh
probe MODEL --semantic-prefix COMMA_IDS --generate
```

The helper `verify_semantic_training.py` writes `native-parity.json` and keeps
failure details in the run artifact. A failed training, build, or gate keeps
its partial checkpoint and logs through the always-uploaded artifact.
