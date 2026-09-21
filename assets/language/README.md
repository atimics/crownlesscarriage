# Crownless conversation core

The shipped in-game conversation model. It is a 4,945,153-parameter decoder-only
transformer trained on held simulation accounts and prior spoken lines. The
model and tokenizer use the included MIT license.

- Model SHA256: `7d1c3cd5e46738ba7cd60673e58f05e12535ac46204b07a8449f41a6fcb936c9`
- Tokenizer SHA256: `c572de53eb4e739a8ce941ac03d1d5fb6173623af786787cf786b41fd34e9af4`
- Grammar (canonical, from the model header): `ad017639e911c482e86e25f063207504769353dcd05e263d31dd9a6ef9409fb6`
- Frozen identity receipt: [`model_identity_receipt.json`](model_identity_receipt.json)

The receipt pins the model, tokenizer, grammar source, native reference cases,
runtime, compiler and evaluator hashes. Run
`python3 tools/language/model_identity.py --check` before any comparison. A
changed artifact or a changed model header fails the check.

## Architecture

The `.ccv2` container header reports: width 192, eight layers, six heads,
feed-forward width 624, vocabulary 4,096, context 512, 70 tensors and 61
meanings. The runtime's event-kind vocabulary has 136 entries. The export uses
row-scaled int8 weights. The C runtime loads the artifact and expands weights to
floats. Attention memory belongs to the caller. The step API spreads work across
frames. Each completed reply can become a spoken event for the next avatar. Held
accounts supply meaning, knowledge labels, and literal copy fields.

The typed meta channel carries role, knowledge, provenance, event kind, stance
(voice, goal, stress, courage), situation (hungry, sheltered, in transit), social
state (owes, trusts, faction, far) and the copy gates as integer ids rather than
prompt text. The earlier conversation model that carried stance as text is
superseded by this checkpoint.

Run `python3 tools/language/compile_model.py --check` to verify the compiled
tokenizer and tensor tables. Their Unicode category ranges are pinned in the
compiler inputs. The native reference in `tests/data/core-model-reference.json`
holds 145 sentences and 302 tokenizer cases; `tests/core_model_tests.py` checks
the native runtime against it and rejects a damaged model.

## Quality claims

The shipped checkpoint's held-out dialogue quality is the pending comparison in
[issue 809](https://github.com/atimics/crownlesscarriage/issues/809). Do not
attribute the older conversation numbers to this file. The 1,247/1,250 main
responses and 1,124/1,250 new-question-word responses were measured on the
earlier 4,935,937-parameter conversation model and are recorded in ZERO's
`experiments/crownless-conversation/`. The account grammar and its coverage are
described in [`docs/core-v2-accounts.md`](../../docs/core-v2-accounts.md) and
[`docs/core-language-corpus.md`](../../docs/core-language-corpus.md).

## In-game path

Ordinary conversations offer **Chat** and **Farewell**. Chat exchanges news
through the simulation command, then generates the player's line from the
player's held version. The NPC receives that exact spoken line alongside their
own held version. The last four spoken events form the conversation history. The
screen retains the player's line above the NPC reply. Leaving the conversation
clears its local history.

The model file occupies 5,055,187 bytes. The runtime uses about 26 MB for
expanded weights and attention memory. Native builds include the model beside
the game. Browser builds fetch it as a separate asset so it can be cached
independently of the startup pack. The compiled tokenizer supplies the runtime
vocabulary.
