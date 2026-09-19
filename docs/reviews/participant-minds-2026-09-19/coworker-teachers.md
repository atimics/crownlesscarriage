# Coworkers looking for work

World 1202, day 366, hash `96d693c4067d6d4e`. The native probe places Jory Fen
and Bren Alder together in Silverwick. Both have zero coins. Jory is a
71-year-old shepherd seeking aid, with stress 100. Bren is a 34-year-old farmer
working, with stress 28. Their directed relationships both say coworkers,
affinity 1, trust 2 and obligation 0.

Two fresh, independent teacher sessions each received one person's state.
The collector selected the held account about winter fodder and sheep in
Rosespire before generation. Each session then received the other person's
actual speech, one turn at a time. These outputs are preserved as returned.

**Jory:** Bren, I am worried about how I will earn my keep. I have no coins. Have you heard of anyone here who needs help with sheep? I would like steady work, if there is work I can manage.

**Bren:** I have no coins either, Jory, and I want steady work too. I heard Ros espire had to slaughter a sheep when winter fodder ran short. Could we ask around Silverwick about work gathering fodder or tending sheep? What sort of work would suit you?

**Jory:** Tending sheep would suit me best. At seventy-one, I would want to ask how much walking and lifting a job takes. Shall we ask together whether anyone in Silverwick needs a shepherd or help with fodder, and what they would pay?

**Bren:** Yes, let us ask together. We can ask about the pay, the hours, and how much walking and lifting each job takes. I would like to hear whether the work could last through the season. Who shall we speak to first?

**Jory:** Could we start by asking someone here who keeps sheep in Silverwick? That might give us a person to speak to about work. I can explain that I am a shepherd, and you could ask whether they need two people through the season. Would that suit you?

**Bren:** That suits me, Jory. You can ask about shepherding, and I will ask about farm work or fodder work for myself. If someone needs us both, we can hear the terms together. Shall we go and ask who keeps sheep here?

## Review and next experiment

The exchange brings age, physical demands, pay and work duration into a shared
plan. Jory reveals his age in turn 2, and Bren receives that fact through speech.
Employment and payment remain conditional proposals. After turn 3, asking a
nearby person would let the simulation supply useful new evidence.

Turn 1 is marked `rejected_entity_spelling`: Bren changes Rosespire to
"Ros espire". Its original text remains in the archive and in subsequent heard
speech. A future training corpus should exclude this target. This teacher error
also gives the name-preservation evaluation a concrete failure case.

All six turns pass the speech protocol and fit the compact compiler. Their
input-plus-reply lengths are 424, 457, 469, 461, 476 and 460 tokens. Yet the
compiler omits the sheep account from turns 1–5. Turn 1 therefore also loses
the evidence for its news claim. A size check alone would miss that problem.
The other five targets await human and compact-context review.

This scene supports a direct next comparison: better selection at 512 tokens
versus a longer context, with source coverage and held-out conversation quality
measured together. Compare exact-name copying as well. The 5M student rebuild
still needs a broader reviewed corpus and actual training.

## Grounding and receipts

`coworker-teachers.json.gz` contains the full native snapshot, six original
outputs, normalized teacher inputs, student candidates, compact previews and
their omissions. Teacher packets carried four account fields: event ID, day,
kind and account text. Student candidates restore the remaining metadata for
that same owned account from the snapshot. The archive records this difference
explicitly. Provider request receipts and the exact model revision are unavailable.

The archive also records a bounded search for former partners who were present
together: seeds 1200, 1201, 1203 and 1204 at day 366, with 3,139 same-place pair
attempts. The probe found zero qualifying pairs in that search. Affection scenes
need another real encounter or a simulation change that brings such people
together. That search result applies to those snapshots.

SHA-256: `26cd2a0fecf9f07a101b5ea52a73b36c97221cf52e4dab79a52deb4e13f7645f`.
The native snapshot was regenerated with seed 1202 and 365 elapsed days. It
matches the earlier saved world's day and state hash.
