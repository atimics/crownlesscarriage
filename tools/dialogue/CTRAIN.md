# Training the 5M in C

`ctrain/ctrain.c` trains the native 5M dialogue-choice policy without Python or
PyTorch. It reproduces `train_meaning.py`: fresh weights, the same architecture,
the last-position loss over the legal candidate IDs, AdamW (lr 3e-4 to 3e-5 by
cosine, weight decay 0.01), global-norm clipping at 1.0 and intent-balanced
sampling. It writes the `CCOREV2` container that `src/story/cc_core_model.c`
loads. Only the tensors the policy path reads are trained: the token embedding,
the six always-on situation and company rows, the blocks and the final norm.
Every other tensor is written as zeros.

## Run

```sh
python3 tools/dialogue/export_c_dataset.py --output /tmp/data --worlds 1024
cc -std=c11 -O3 tools/dialogue/ctrain/ctrain.c -framework Accelerate -lm -lpthread -o /tmp/ctrain   # macOS
cc -std=c11 -O3 -DCT_NO_BLAS tools/dialogue/ctrain/ctrain.c -lm -lpthread -o /tmp/ctrain           # portable
/tmp/ctrain train --reference models/dialogue-syntax/model.ccv2 \
  --train /tmp/data/train.txt --dev /tmp/data/development.txt --test /tmp/data/test.txt \
  --out /tmp/c-run --steps 2500 --threads 6 --seed 19
```

The reference container fixes the architecture, the tensor layout and the
metadata; the trainer checks that its layout matches the reference byte for
byte and copies its header with a new payload hash. `--warm-start` loads the
reference weights instead of a fresh initialisation. `ctrain eval --model M
--data FILE [--picks]` scores any container.

The dataset generator (the teacher policy) is still Python; the exporter needs
only the standard library. The trainer reads its plain-text output.

## Checks

- `tests/dialogue_ctrain_tests.c` (CTest `dialogue_ctrain_gradients`) compares
  every hand-written gradient with central differences in double precision on
  a tiny model.
- `verify_ctrain_parity.py` builds the native probe for a model and compares its
  choice on every row with the trainer's forward pass. The shipped syntax
  checkpoint and a C-trained checkpoint both match on all 692 test rows.

## Results (1024 worlds, 2500 steps, batch 32, CPU)

| Seed | Development | Test (float) | Test (exported int8) |
| --- | --- | --- | --- |
| 19 | 96.47% | 95.09% | 95.09% |
| 1 | 98.49% | 99.42% | 99.42% |
| 2 | 97.98% | 98.84% | 98.84% |
| 3 | 99.06% | 99.42% | 99.42% |

Each run takes about 170 s with six threads on a 14-core Apple laptop
(Accelerate). The MEANING_RESULTS.md figure for the PyTorch run is 99.57% on
the same test split; I did not rerun PyTorch, so the seed spread here (95.1% to
99.4%) is the available measure of run-to-run variation. Errors fall on the
quantity-dependent choices (offer, counter, accept, decline).

## Bucketed inputs

`export_c_dataset.py --style` selects how numbers enter the prefix.

- `digits` (default): every value as base-128 digit tokens, one token per digit.
  Each value below 128 is an unrelated token, so the model must learn the
  ordering of the values one by one.
- `buckets`: magnitude words. Exact counts up to a dozen, then named bands
  (13-19, 20-31, 32-49, 50-99, 100-199, 200-499, 500+); trust as
  distrust / neutral / trust; stress as low / middle / high.
- `afford`: `buckets` plus, for each store, how many units the purse buys at its
  price.

The same rows are produced in every style (same order, same targets); only the
input tokens change, and they stay within the native limits, so the runtime is
unchanged. Exact numbers stay in the typed act arguments. Three seeds each,
development plus test rows together (2,081), same recipe:

| Style | Seed 1 | Seed 2 | Seed 3 | Mean | Accuracy |
| --- | --- | --- | --- | --- | --- |
| digits | 2056 | 2045 | 2064 | 2055.0 | 98.75% |
| buckets | 2073 | 2073 | 2065 | 2070.3 | 99.48% |
| afford | 2074 | 2069 | 2076 | 2073.0 | 99.62% |

The worst bucketed run beats the best digit run, and seed-to-seed spread falls
from 19 rows to 7-8. A model trained on `afford` inputs matches the native
runtime on all 692 test rows (691 correct). Recipe changes (Muon, zero-init,
softcap, label smoothing, WSD) were not distinguishable from noise with digit
inputs. The runtime path (`choose`, `world_dialogue`) still builds digit
prefixes; a checkpoint trained on bucketed inputs needs `encode_input(...,
style=...)` there and a new format version before it ships.

## Differences from the PyTorch trainer

- Random streams differ (splitmix64 and Box-Muller), so weights are not
  bit-identical for the same seed.
- Unused tables (roles, kinds, voices, copy heads) are zeros, not random.
- Without Accelerate the portable matrix code is about 30 times slower (the 692-row test forward pass takes 53 s instead of 1.7 s), so a portable training run would take hours. A BLAS such as OpenBLAS is the next step for Linux CI.
