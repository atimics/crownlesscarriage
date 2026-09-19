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
