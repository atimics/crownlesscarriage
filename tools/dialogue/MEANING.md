# Participant meaning and world outcomes

The v3 food loop connects typed observations, one person's decision, a language
pack, native commands, and remembered outcomes. The existing nine-goal v2 policy
remains available. V3 supplies a deeper working example for food and help.

## Meaning

A participant owns a bounded list of food-store observations. Each observation
contains its place, stock, reserve target, price, date, source, and disclosure
flag. The native probe reads a local store while both people are present.
Public speech retains its speaker and reply link. Each decision uses one
participant's view and the public exchange.

The policy creates complete possible acts with explicit payer, beneficiary,
quantity, unit price, total price, and condition. The model chooses a candidate
ID. The native decoder limits that choice to the current candidates. The caller
checks the chosen act against the current view before speaking or executing it.

The numeric input preserves exact quantities and prices with bounded integer
encoding. It includes recent public acts, relevant facts, relationship trust,
hunger, stress, and memory outcomes. Stable world IDs remain in the host records;
the model sees self/other/place roles and local fact slots. Renaming a person
preserves their decision, while changing their role in an outcome changes its
meaning. Each candidate contains its intent and typed arguments. Speech strings
stay in the language layer.

The reference choices conserve a severely depleted store, reserve enough money
for the helper's own meal where possible, allow smaller counteroffers, and check
current prices. These are authored policy preferences. Agreement with them
measures policy imitation. Native resource changes and human reading of the
result provide separate checks.

## Language packs

`assets/worldpacks/languages/v3` contains versioned human and Hra'khor packs.
Packs provide whole clause templates, concept forms, source phrases, and reasons.
They render directly from the act. A pack can reorder clause slots. Names and
numbers enter only through typed slots. Hra'khor keeps its mixed English voice;
ash, ashes, and daylight are shared, and vault is grak'rakh.

The renderer speaks as the act's actor. The runner binds that actor to the
current speaker. Source and date remain attached to the act; the spoken line
uses a short observation or hearsay phrase where relevant. A recalled outcome
names its effect rather than printing event identifiers.

## Native world loop

`world_dialogue.py` loads an existing native save. It refreshes both participant
snapshots through native probes before each turn. An accepted food proposal is
recorded, accepted, and executed through the native food agreement API. The
native layer owns current resource checks, consent, payment, food consumption,
relationship changes, persistence, and duplicate protection.

Run against a copy of a save when comparing experiments. The runner records its
before and after save checksums, initial and final participant states, typed
turns, speech, and command receipts. Partial progress is saved if execution
fails. `--reunion` starts a fresh exchange from the saved result so the people
can recall the purchase.

```sh
python tools/dialogue/world_dialogue.py --world /tmp/world.ccsave \
  --food-probe /path/to/crownless_food_relief_probe \
  --participant-probe /path/to/crownless_participant_probe \
  --first PERSON_ID --second PERSON_ID --language hrakhor --reunion \
  --output /tmp/conversation.json
```

Add `--model /path/to/last.ccv2 --probe /path/to/model-probe` to use the trained
5M. The model checksum and meaning-source checksum must match its manifest.
Without those arguments, the same runner uses the procedural teacher.

## Training and checks

`train_meaning.py` trains fresh weights in the native 4.95M architecture. Only
the selected participant choice contributes to its loss. The input includes the
legal alternatives and their arguments. Related world episodes share a split,
and exact input overlap is checked. Training reports distinguish validity from
reference preference agreement.

```sh
python tools/dialogue/train_meaning.py --zero /path/to/pinned-zero \
  --reference models/dialogue-syntax/model.ccv2 \
  --tokenizer assets/language/tokenizer.json \
  --output /tmp/meaning-run --worlds 1024 --steps 2500
```

The release workflow checks the exported native model against Python, then runs
quantity and purse counterfactuals, a generated-world purchase, a reunion, and a
replayed execution. It preserves training and failed-run reports. The world
proof uses actual generated people and reports their identities and day.

Food agreements are the first executable meaning family. Other policies can
use the same boundary by adding typed observations, candidate arguments, native
actor commands, and outcome memories. The game client continues to use its
existing account dialogue path; the v3 runner operates on native saved worlds.
