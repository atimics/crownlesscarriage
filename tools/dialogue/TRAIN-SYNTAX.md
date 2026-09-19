# Training the dialogue policy

The syntax model predicts a public dialogue act from one person's state and
the observed acts. The human and Hra'khor renderers supply spoken words.

The first training run distils the seven-move procedural policy. The dataset
contains 6,400 explicitly synthetic examples across 400 own-state combinations.
Those combinations vary goal, hunger, stress, courage and coins. Public histories
cover food, safety and paid work, including conditions, acceptance and refusal.
The test concerns this finite policy. Richer choices require additional rules.

Whole state combinations are assigned by a stable hash: 5,152 training rows,
608 development rows and 640 test rows. The compiler checks group and exact
model-input separation. Shared act targets are intentional: they are the public
vocabulary. The first run evaluates a fixed balanced sample of 112 rows from
each held-out split. The old checkpoint uses those same test inputs as a baseline.

The prefix gives the model own goal, courage and coins plus two state predicates:
hungry (`hungry_days > 0`) and distressed (`stress >= 60`). These predicates
match the procedural policy. Public acts use self/other roles. Identity remains
with the caller. Only one person's private view enters each prediction.

## Reproduce

Use a ZERO checkout containing the native-compatible `crownless_v2.py`,
`crownless_v2_export.py` and `train_crownless_participant.py`. The original run
used commit `49e6bfedc6d91196e652d89309b23cafd8fc2ce9`. Source hashes appear in
the run manifest. Install the dependencies required by those scripts in a
Python environment; the recorded run used PyTorch 2.8.0 on CPU.

```sh
python tools/dialogue/syntax_training.py --zero /path/to/zero \
  --reference assets/language/core.ccv2 \
  --tokenizer assets/language/tokenizer.json \
  --output /tmp/syntax-run --steps 1200 --batch-size 8 --eval-count 112
```

This uses the 4,945,153-parameter native architecture and the existing tokenizer.
Loss covers only the next act and EOS. The run preserves datasets, their hashes,
step losses, baseline outputs, final development/test outputs, a float training
checkpoint and a quantized `.ccv2` export. Failed runs preserve a partial model
and an error receipt. Metrics are raw greedy output: valid acts and exact policy
matches are counted separately. Model output is checked before rendering.

## Native execution

Build `core_model_probe` and `core_account_probe` in a matching Crownless build.
The ZERO checker compiles a probe with this checkpoint's exact tensor tables:

```sh
python /path/to/zero/scripts/check_participant_native.py \
  --run /tmp/syntax-run --crownless /path/to/crownless --build /path/to/build
python tools/dialogue/syntax_student.py --snapshot /tmp/pair.json \
  --model /tmp/syntax-run/last.ccv2 --probe /tmp/syntax-run/native/probe \
  --surface-probe /path/to/build/core_account_probe --output /tmp/syntax-people
```

The pair runner uses each person's own view in turn. Its result contains raw
native bytes, public acts, human speech and optional goblin speech. Validation
failures remain in the result and end that attempt. The simulation stays at
the captured day; acceptance records an intention. Actor-specific command
execution and the in-game dialogue UI are separate integration steps.
