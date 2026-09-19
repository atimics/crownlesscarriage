# Participant baseline: dragon attack

World 1202, day 368. Harthild Underwick and Hartha Stonehewer each hold the account of Varkesh burning Gloamgate over 17 missing crowns. The simulation snapshot supplies each person separately.

**Harthild Underwick:** I bring word from further than here. Varkesh the Unappeased burned Gloamgate over money missing from the hoard.

**Hartha Stonehewer:** I forge the blades and the Varkesh the Unappeased can carry it next time.

**Harthild Underwick:** My hands are shaking. I should hide what little we have before they return.

**Hartha Stonehewer:** I forge the blades and the Varkesh the Unappeased. I have heard enough. If they raid on schedule, we can plan around it.

**Harthild Underwick:** My hands are shaking. I should hide what little we have before they return.

**Hartha Stonehewer:** I will be here if you hear more.

The shipped 5M model produced all six turns. These are pending review candidates. It repeats the shaking-hands line and produces malformed smith replies. The opening also claims to bring news from elsewhere while the speaker is in Gloamgate; the snapshot gives no direct support for that introduction. These observations make this a useful failure baseline.

`baseline.json.gz` is a JSON object mapping original filenames to their exact text. It contains both participant views, all six input/output records, the receipt, candidate rows, and worker logs. Its SHA-256 is `7164c3eff68809acac29a05e89e468595d02f7abe41ebaf2be31933cee6bbc81`. The receipt binds the model, probe, worker and runner identities used for this run.

The separate client Chat regression passes after the shared state builder and turn-policy change. Its sample was:

> You: The cold got in again last night. The stores are low and the winter is long.
> Mara Venn: The flock is lean, and the pasture is leaner.

This verifies the live code path. Semantic review of those lines remains part of the rebuild.
