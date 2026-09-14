# Fresh Crownless conversations

48 original dialogue drafts drawn from four new simulated worlds. Each uses a recorded held account and a short authored exchange. The twelve earlier user selections guide the writing: short responsive turns, concrete curiosity, plain warmth, and uncertainty within the claim. Training remains paused while these candidates receive review.

Open `index.html` for a twelve-scene sample with Keep, Change, and Skip. It pauses after five and ten choices, supports notes and undo, saves progress in the browser, and exports the review to a file. Read `dialogues.txt` for all 48 exchanges. The sample has twelve event types and four scenes from each confidence band.

## Sources and scope

Worlds 1201, 1202, 1203, and 1204 ran for 365 days each. The simulation finished with valid worlds and exported 136,482 reference rows. Selection produced 48 accounts across sixteen event kinds: bakery, bandits, calving, cattle loss, cult promotion, death, grain funding, dragon fire, dragon omen, flock cull, goblin raid, harvest, pony breeding, lambing, masonry, and paper.

The run uses the same pinned simulator and native model tools as the first review, built from Crownless commit `90dc717e745103e38351307cd82df0ced93f27e7`. It uses fresh seeds with that recorded simulation version. `generation.json` preserves the sampler revision, binary and model hashes, world receipts, and raw-file hashes. `accounts.jsonl` is an unchanged copy of this run's selected accounts. It also contains model-generated single lines as baseline evidence. The conversations in `drafts.json` were authored by assistants and edited separately.

The first speaker holds the source account. The listener learns through prior speech. Meetings, interests, questions, wishes, and reactions are authored. Statements about the world stay within the account. Predictions remain attributed to readers; grain funding remains distinct from delivery; the unnamed treasure stays unnamed; zero crowns taken stays zero. Hopes and opinions are speech, and future game actions need simulation support.

These are fresh world observations with familiar event grammar. Five accounts exactly match wording in the twelve-pair review; twenty-eight match wording in the earlier 96-account set. General quantities can make further accounts equivalent. This batch serves writing development. Held-out model evaluation will need a separate content-overlap check and separate worlds.

## Writing outcome

The batch contains 158 spoken lines and 155 distinct exact lines, across exchanges of two to five turns. Repeated lines are “Yes.”, “I’d have to ask.”, and “What happened to the meat?”, each twice. These describe the authored set. Human preference and trained-model quality remain separate results.

The source facts supply the news. The listener can ask where grain is intended to go, wonder about a calf, or respond to a death. Short direct exchanges appear alongside longer ones. The next review can show which of those choices carries the preferred tone into new situations.

The earlier review is bound by digest in `reference-selection.json`. Selected candidate IDs are preserved there. Exported choice timestamps and other personal review metadata are omitted from this research artifact.

## Format and next use

`records.jsonl` holds each conversation with its account, source ID, confidence, circulation, and world identity. `dialogues.txt` is the plain event-and-speech copy. JSON is a storage format for provenance. The intended model interface remains compact event lines followed by a spoken continuation.

These files are editorial candidates. A training compiler should use each speaker's available knowledge and the preceding speech, then score only the next spoken line. Any additional occupation, relationship, or need used as a condition must also be supplied by the game. This batch leaves those character traits unspecified.

## Reproduction and checks

The fresh run used:

```sh
python3 tools/build_speech_review.py \
  --bin-dir /path/to/pinned-native-tools \
  --output /path/to/new-output \
  --seeds 1201 1202 1203 1204 --days 365 \
  --accounts 48 --conversations 1
```

The extra native six-turn conversation is part of the sampling run receipt. It remains separate from the 48 authored candidates. Raw compressed traces are retained locally at `/private/tmp/crownless-fresh-worlds-1201-1204/raw` and can be recreated with the pinned inputs.

To rebuild this review, run `python3 docs/reviews/fresh-dialogue-2026-09-12/build.py`. The builder checks source hashes, valid-world receipts, seed membership, record coverage, event diversity, confidence balance, and turn counts. It writes the page, plain text, records, summary, and manifest. File hashes bind the published batch. Editing a draft changes the review digest and gives it a fresh browser save identity.

Validation on 12 September 2026: five sampler tests passed; the review builder passed; the native tool hashes matched the prior run exactly; the page script passed syntax checking. Browser checks covered Keep, Change, Skip, note retention, reload/resume, undo, both round pauses, completion, and export invocation. Browser test choices used a separate address from the user's review.
