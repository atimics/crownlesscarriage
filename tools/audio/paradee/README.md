# Repeatable Paradee voice training

This runner prepares one small speech model for one American or British English
Kokoro voice. Each run has its own source tree, environment, teacher files,
training data, checkpoints, logs, and exported models. Choose a separate run
folder for each voice.

The recipe follows [Paradee's published training instructions](https://github.com/sahilmahendrakar/paradee/blob/9c8b4d7504cbee7e64de2d0341bb690f0b0ab708/training/README.md).
It uses 12,000 source sentences, 200 held-out records, 8,000 text steps,
50,000 decoder steps, and two 5,000-step sound-quality stages. The last stage
uses the first eight data shards, as in the published recipe.

## Prepare a run

Python's standard library is enough for planning, source preparation, and cost
calculations. These commands work from the Crownless root:

```sh
python3 tools/audio/paradee/run.py plan \
  --voice af_heart --device mps --run out/paradee/af_heart
python3 tools/audio/paradee/run.py prepare \
  --voice af_heart --device mps --run out/paradee/af_heart
```

`plan` prints the settings and stage commands. `prepare` downloads the fixed
source commit and checks each training file against its SHA-256 checksum.
It then applies small source edits for device selection, stable data seeds,
atomic checkpoint writes, and saved random state. The adapted source also has
a checksum receipt. An existing run must keep its original settings and code.

The published training used an M4 Pro MacBook with 24 GB of memory. `cpu`,
`mps`, and `cuda` select the intended backend. The runner requires deterministic
operations. A backend with an unsupported operation reports an error. Test the
chosen backend during the first pilot.

Install uv 0.12.3, then prepare the environment and teacher files:

```sh
python3 tools/audio/paradee/run.py install \
  --voice af_heart --device mps --run out/paradee/af_heart
```

Installation uses Python 3.12.12 and the upstream `uv.lock` with `--frozen`.
The English pronunciation model has a fixed version and wheel checksum. Kokoro's
weights, configuration, and selected voice come from a fixed Hugging Face
revision. Their downloaded checksums are saved in `teacher.json`.

Installation needs network access and several GB of disk space. Allow about
30 GB per active voice run for data, dependencies, checkpoints, and temporary
files. The published teacher data occupied 6.9 GB. The final int8 model is about
9 MB. These sizes describe different parts of a run.

## Train and continue

```sh
python3 tools/audio/paradee/run.py run \
  --voice af_heart --device mps --run out/paradee/af_heart
```

Use the same command after an interruption. Completed stages have output
checksums in `state.json`. The runner checks those files before continuing.
An unfinished trainer continues from its last atomic checkpoint. Checkpoints
include the model, optimizer, step, and Python, NumPy, Torch, and available GPU
random states. Evaluation preserves the training random state. Only one
process can write a voice's run folder at a time.

To stop at a stage boundary, add `--until corpus`, `--until text`,
`--until decoder`, `--until gan10`, or `--until gan3`. Each stage writes a log
such as `out/paradee/af_heart/text-0.log`. Stage times are saved in `state.json`.

For a second voice, use the same steps with a new voice ID and run folder:

```sh
python3 tools/audio/paradee/run.py prepare \
  --voice bf_emma --device mps --run out/paradee/bf_emma
```

The selected voice and matching American or British pronunciation apply to all
teacher calls, including evaluation. Pick voices from
[Kokoro's voice list](https://huggingface.co/hexgrad/Kokoro-82M/blob/f3ff3571791e39611d31c381e3a41a3af07b4987/VOICES.md).
Each new voice is an experiment with the same small model layout.

## Inspect the result

Exports appear in `<run>/training/models/paradee/`:

- `paradee.onnx` and `paradee_int8.onnx` contain the speech models.
- `config.json` contains the phoneme vocabulary, sample rate, voice, and language.
- `validation.json` records file hashes, audio checks, and one-thread CPU speed.
- Held-out WAV files provide listening samples for each export.

The runner checks both ONNX graphs. It checks the int8 export on all 200 held-out
records for finite, audible audio between 0.15 and 25 seconds. Four records also
test the fp32 graph. Speed covers inference on saved phonemes. Text preparation,
loading, and game playback need their own measurements. Listening review should
cover clarity, voice identity, and naturalness before using a model in the game.

`environment.json` records Python, package versions, the backend, and CUDA GPU
details where available. Continue with the same environment. Repeat runs should
use the same hardware and backend. Compare learned tensors and listening samples
when checking repeatability across fresh runs.

The first full training and export run establishes backend support, sound
quality, wall time, and the cost of this recipe. The runner's automated checks
cover source preparation and checkpoint continuation on a small CPU training job.

## Cost scenarios

Runpod listed an RTX 4090 Pod at US$0.89/hour on October 18, 2026.
Standard network storage below 1 TB was US$0.07/GB/month.
[Provider prices](https://www.runpod.io/pricing)

| Assumed billed machine time | Compute cost at US$0.89/hour |
| --- | --- |
| 12 hours | US$10.68 |
| 24 hours | US$21.36 |
| 48 hours | US$42.72 |

These are arithmetic scenarios. Use billed time for source setup, teacher data,
training, and export when calculating the full cost. Retries add billed time.
Thirty GB of retained network storage adds US$2.10 per month at the quoted rate.
Other disk types have separate prices. Taxes and provider credits depend on the
account. Engineering and listening review are separate costs.

```sh
python3 tools/audio/paradee/run.py cost --hours 24 --hourly-rate 0.89
```

Measure the first complete voice, then use its billed hours to price the rest
of the cast. A short trial of each training stage can establish a budget before
the full run. Setting a provider time or spending limit belongs to the cloud
job setup.

## Check the runner

```sh
python3 tests/paradee_training_tests.py
PARADEE_SOURCE_ARCHIVE=out/paradee/af_heart/source.tar.gz \
  python3 tests/paradee_training_tests.py
```

The real checkpoint continuation test also runs when Torch and NumPy are
available. It compares uninterrupted training against training resumed from a
saved checkpoint, including dropout and random batches. CI checks the pinned
source archive and compiles all adapted trainers.

The Paradee source and Kokoro weights use Apache 2.0. The original WikiText
sentence corpus uses CC BY-SA 3.0. Keep their notices with redistributed source
and data. The run directory retains the upstream license and README in `notices/`.
