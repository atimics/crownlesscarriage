# Reproducible semantic 5M training

The `Semantic 5M training` workflow runs on `v*.*.*` release tags and on
manual dispatch. Relevant source changes select release runs; manual runs always train. It compares the dialogue sources with the previous release,
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
The default run uses 4,000 steps and fresh weights for the nine-goal participant policy.

Build native libraries with `CC_BUILD_CLIENT=OFF`, then run
`tools/dialogue/build_syntax_probe.py`. The probe invocation is:

```sh
probe MODEL --policy-prefix COMMA_IDS --generate
```

The helper `verify_semantic_training.py` writes `native-parity.json` and keeps
failure details in the run artifact. A failed training, build, or gate keeps
its partial checkpoint and logs through the always-uploaded artifact.

The workflow uses a standard Ubuntu CPU runner, a 30-minute job limit, and an
18-minute training limit. The normal run has 4,000 steps. Manual requests accept
1 through 6,000 steps; the same full evaluation gate applies. The verified
model artifact lasts 90 days. It is available for review and later integration
into the game. The workflow leaves the shipped checkpoint under version control.

The current target has nine goal families and 38 acts. See [POLICIES.md](POLICIES.md)
for state grounding, the compact wire, and local rollout commands. Event grammar
coverage is checked before training. The seven-move trainer remains available
as `train_semantic_ids.py` for reproducing earlier experiments.
