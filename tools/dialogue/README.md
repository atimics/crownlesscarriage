# Participant dialogue

For the procedural abstract-syntax prototype and shared human/goblin meaning,
see [SYNTAX.md](SYNTAX.md).
The trained 5M syntax checkpoint and native pair runner are documented in
[TRAIN-SYNTAX.md](TRAIN-SYNTAX.md).

Each worker is one person. It receives that person's view and produces one
speech turn or an action. The runner delivers accepted speech to the other
worker as an observation. Each worker keeps its identity throughout an episode.

## Native grounding

Build `crownless_participant_probe` and `core_model_probe` with CMake. List people
or capture a pair from a seed, or load a saved campaign:

```sh
BUILD/crownless_participant_probe --seed 1202 --days 367 --list
BUILD/crownless_participant_probe --seed 1202 --days 367 \
  --first 1369094286720630904 --second 1369094286720630894 > snapshot.json
BUILD/crownless_participant_probe --load campaign.ccsave \
  --first PERSON_ID --second OTHER_ID > snapshot.json
```

Both participants must be present in the same settlement. `--days` advances
only the tool's in-memory copy. Observation preserves its world hash. IDs are
JSON strings. The pair snapshot includes a world hash for the evaluator.

A participant view contains:

- Own identity, trade, goal, activity, age, stress, courage, money, hunger and shelter.
- Current place, home, faction and band identity.
- Listener ID and name, plus the speaker's directed relationship to that listener.
- Own player-interaction memories and private knowledge references.
- Own held accounts, including their telling, source, confidence and event date.

The participant builder uses the simulation's personal-account query. Scheduled
death dates and the listener's private state stay in the simulation. The game
uses the same builder for its supported model inputs. Band identity is a
separate field; the old model's voice table receives an occupation. Its two
memory slots receive other held accounts, with the current event kept separate.

Character memory records currently describe player interactions. Personal
knowledge references are preserved as references. Richer autobiographical
memory, named attack victims, family bonds and persistent emotional consequences
are further simulation work.

## Worker protocol

Workers read JSON requests from stdin and write exactly one JSON object followed
by a newline to stdout. Use stderr for logs and flush stdout after each turn.
The request includes `protocol`, `instruction`, `participant`,
`remembered_observations`, and `observed_turns`. Only the current person's view
is sent to that process. Previous speech keeps the actual speaker ID.

Speech:

```json
{"kind":"speech","text":"Come with me when the wagons leave."}
```

Action:

```json
{"kind":"action","action":"end_conversation"}
```

The action closes the episode. Actor-specific resource transfers and travel
need simulation command support before becoming available actions. For now the
world remains at its captured day while the exchange takes place.

Each accepted turn is checked for protocol shape and length. Semantic review
is a separate step. `training-candidates.jsonl` therefore labels every row
`review_status: pending`. Each row contains one person's input and own output.
Complete transcripts are evaluator artifacts. Related scenes and their turns
must share a split when building a training set.

The runner saves every attempted turn, including invalid output, partial output
on timeout, and worker failures. The receipt records input and artifact hashes,
worker commands, local worker/model file hashes, and the runner hash. Use a
fresh output directory for each attempt.

## Paired baseline

The included `native_worker.py` runs the shipped checkpoint using its existing
account/mind interface. It supports the occupation, goal, stress, courage,
hunger, shelter, travel, trust, obligation, witness and held-account inputs.
The first turn is cued as an opening, followed by personal reactions. The
worker enforces its identity even though the old checkpoint has limited
personal conditioning. Its output provides a baseline for a new teacher and
student; inspect the sample under `docs/reviews/participant-minds-2026-09-19`.

Pass each worker command as a JSON argv array:

```sh
python3 tools/dialogue/paired_participants.py --snapshot snapshot.json \
  --first-worker '["python3","tools/dialogue/native_worker.py","--probe","BUILD/core_model_probe","--model","assets/language/core.ccv2","--event","648518346341353816"]' \
  --second-worker '["python3","tools/dialogue/native_worker.py","--probe","BUILD/core_model_probe","--model","assets/language/core.ccv2","--event","648518346341353816"]' \
  --output /tmp/dragon-pair --turns 6 \
  --memory-db /tmp/people.sqlite --world experiment-world-1202 --episode first-meeting
```

Each worker runs in its own process. A stronger teacher can implement the same
one-person protocol. Record its model version and inference settings with its
worker command or configuration file.

## Personal observed memory

The optional SQLite store records accepted speech and episode actions separately
for each observer. Records include speaker, simulation day, episode and turn.
The explicit world ID scopes people to one simulation history. Use a new world
ID for a new seed, branch, reset, or independent history. Episode IDs are unique
within a world. Opening a snapshot older than recorded memories fails.

The next episode receives the person's last 16 observations. The store keeps
all observations; this context limit is independent of retention. Private
participant fields remain in the person's view. This store is an experiment
artifact; integrating it into game saves is a later delivery.

## Rebuild sequence and completion evidence

The compact student compiler and experimental native worker are described in
[TRAINING.md](TRAINING.md). Their six-turn token and context-loss report is in
`docs/reviews/participant-minds-2026-09-19/compact-inputs.md`.

1. Shared participant snapshots, separate workers, one-person targets, observed
   memory and the same-event correction policy: implemented here.
2. A bounded paired-teacher collection from real needs, relationships and
   danger; review each turn and preserve every failure. Include trade, love,
   war, work, community, adventure, and fear/loss as their state becomes available.
3. The first compact participant encoder shares its prompt builder with the
   experimental native worker and records omitted context. Compare its text
   fields with explicit copying and longer context during student training.
4. Train fresh and warm-start 5M candidates at matched token budgets. Evaluate
   responsiveness, useful contribution, coherence, grounding, fluent speech,
   identity, memory, and actions across complete held-out encounters.
5. Compare explicit memory with a small per-person adapter. Keep dated episodes
   in durable storage; measure what the adapter adds to recall and character.
6. Integrate actor-specific actions and personal memory into saved game state,
   then verify native/browser cost, Hra'khor, normal Chat, and save/reload.

Tests cover private-view isolation, one-person targets, actor actions, saved
observations, failed worker receipts, real simulated participants, and the
same-event condition on corrections. Ordinary client tests exercise Chat
through the shared state builder.
