# Raid, work and barter

Two independent teacher sessions played Chenric Wayfinder and Harthild
Shallowford in world 1201, day 181. Chenric is a woodcutter, Harthild a scribe.
Both have zero coins and a secure-livelihood goal. Both hold an account of the
Cinder Tithe taking 20 Wheat and 16 crowns from Thornford on day 172.

The search inspected 31 co-located pairs before finding this raid account.
The selected packet retained that owned account and each person's other state.
The complete snapshot and original outputs are in `raid-teachers.json.gz`.

The exchange developed from concern about earning money into a barter proposal:

- Chenric: "I'd take wheat for some of the work if someone offered it."
- Harthild: "I'd like to ask whether anyone needs writing work as well."

Both wanted separate pay. Chenric then chose `end_conversation`. This is the
first reviewed action target in the pilot. The collector stopped on that action.
The world stayed at day 181, and work remained a proposal.

Five of seven turns passed source and compact review. Two repeated agreements
remain preserved as exclusions. Each v2 input retains the raid account and the
latest received speech. Full teacher state, compiled input, decisions, cited
evidence and hashes are archived together. Reviews were performed by the root
agent. The teacher provider's exact model revision is unavailable.

`expanded-pilot.json.gz` contains `train.jsonl` with sixteen accepted turns from
worlds 1201, 1202 and 1203, plus `development.jsonl` with three turns from world
1200. It records source archive hashes. The development examples have informed
input design, so a final quality evaluation needs fresh worlds and identities.
The regular Zero loader accepts the corpus, verifies its masks and tokenizer,
and confirms disjoint world groups and target text across the two sets.

This remains a small corpus. Next collection should revisit the same people
under changed events, retain their own observed conversation memories, and test
whether their new replies reflect those changes. That creates direct examples
of stable identity with changing knowledge and plans.

## Return on day 224

`raid-continuity.json.gz` records a second meeting 43 days later. The same two
teacher sessions received their updated private packets and the latest held
raid account, dated day 214. Their own personal fields were unchanged.

The existing SQLite observation store preserved all seven events from the first
meeting under each person and the same world group. Before generation, the root
selected two exact speech excerpts per person: the earlier raid report and the
person's own work preference. The archive records each excerpt's source turn,
source hash and character offsets. This is a manual memory-selection experiment;
the general memory reader still returns whole observations.

Chenric said, "I'd still take wheat for cutting work." Harthild said, "I'd still
like writing work." Both opening student inputs retain the exact old statements
supporting those preferences. The conversation then adds agreeing when payment
is due and ends with Chenric's action. Four of five turns pass review; a repeated
pay-first proposal remains excluded. All original outputs are preserved.

`continuity-pilot.json.gz` adds these turns to the earlier corpus, for twenty
training turns and three development turns. Both meetings use world group
`1201-main-b3a8`. The simulation provides the new state; the earlier proposed
work remains a remembered proposal with no recorded outcome.

## Twenty-turn training check

`continuity-training.json.gz` preserves a 100-step warm-start run with seed19,
batch size2, 19,308 target tokens and the shipped 4,945,153 parameter model.
Final batch loss was 0.45876. Development loss was 3.26930 on the same three v2
development targets used earlier. Both sampled replies mix memorized food-count
and pay language, address the wrong person, and ignore the cattle question.
Native C and Python quantized outputs match exactly. This remains a failed
semantic result despite the lower development loss. Weights stay in the local
run directory recorded in the archive; the game keeps its shipped checkpoint.
