# Dialogue membership checks

Issue [809](https://github.com/atimics/crownlesscarriage/issues/809) starts with
three scoring regressions. Run their tests with:

```sh
python3 tests/language_membership_tests.py
```

CTest also runs these as `language_membership_metrics`. The tests use the
Python standard library.

## Input evidence

`prompt_text(row)` reads this capture when supplied:

```json
{
  "model_input": {
    "prefix": "The decoded input prefix after encoding and truncation",
    "copy_spans": ["The exact accessible copy string"]
  }
}
```

The capture producer must collect these values from the generation input.
Keep the expected answer in `output` and alternative answers in `accepted`.
The scorer gives the capture priority over the original row. This keeps
trimmed history and inaccessible fields outside the name evidence.

Existing corpus rows use `prefix` as a legacy input-only diagnostic. Runtime
capture through the Chat and ambient adapters is a separate delivery under
809. Use captured inputs for the planned checkpoint comparison.

Name membership uses whole Latin-alphabet tokens, case folding, Unicode
normalization, and straight/curly possessive normalization. An optional alias
map explicitly relates a single-token variant to its canonical name. For
example, `invented(text, evidence, common, {'Rosie': 'Rose'})` permits Rosie
when Rose appears in the evidence. Set aliases before evaluating candidates.
Treat this as a name-token diagnostic. Multiword identity, roles, quantities,
source and offer scope need their own checks. Lowercase names and names shared
with common words remain limitations of this capitalization heuristic.

## Frozen development calibration

`field_calibration.json` contains common words from the first 20,000 rows of
Zero's `out/crownless-moves-v7/train.jsonl`. The receipt records the full source
file SHA-256, row count, format version and sorted word list. Training outputs
and accepted training alternatives are eligible development material.

Create a new calibration artifact before opening a held-out set:

```sh
python3 tools/language/measure_field_membership.py --zero /path/to/zero \
  --calibration /tmp/new-calibration.json --freeze-calibration
```

The exclusive file creation preserves an existing frozen artifact. Evaluation
loads the saved list and prints its SHA-256. It reads held-out targets only as
candidates to score. Run the old audit with:

```sh
python3 tools/language/measure_field_membership.py \
  --zero /path/to/zero --braid /path/to/braid \
  --calibration tools/language/field_calibration.json --at-scale
```

On the local historical audit, the frozen calibration produces 4/51 reference
flags, 2/4 content flags, and 0/22 lexical-ok flags. The legacy corpus-prefix
check flags 485/5,000 authored targets. Preserve these results as diagnostic
flags: review missing input context and common-word coverage before interpreting
an authored-target flag as a model error. The old zero-false-positive claim
used targets as evidence and wording-holdout calibration.

## Complete memory quotes

`quotes_memory(candidate, memories)` requires a complete memory string, with
case and whitespace normalized and word boundaries preserved. Quantities,
roles, negation and punctuation remain part of the quoted claim. Pass
`[selected_memory]` to check one selected claim. Passing all held memories,
as the historical CLI does, measures any-held-memory quote coverage.

A truncated memory or paraphrase requires separate review. Publish these
misses. A full quote can coexist with another false assertion, so quote
coverage is one component of a fidelity evaluation. Memory selection relevance
also needs a separate score.

## Model identity receipt

`assets/language/model_identity_receipt.json` pins the exact shipped model,
tokenizer, grammar source, native reference, runtime, compiler and evaluator
hashes, together with the parsed model header (parameters, meanings, event
kinds, config). Build it from the artifacts and check it:

```sh
python3 tools/language/model_identity.py --write
python3 tools/language/model_identity.py --check
```

`--check` recomputes every hash and re-parses the `.ccv2` payload hash rather
than trusting the header. A changed file, a stale README claim, or a substituted
checkpoint fails the check. `tests/model_identity_tests.py` covers the pinned
facts and drift detection.

## Next evaluation step

This change establishes scoring regressions, frozen calibration and the model
identity receipt. The 24-family pilot, authored/current-model comparison,
device timings, and play checks remain in issue 809. Freeze those inputs and
decision rules after the dialogue repairs in issue 804.
