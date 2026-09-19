# Semantic-ID dialogue checkpoint

This 4,945,153-parameter checkpoint consumes and predicts semantic IDs under
`crownless-semantic-ids-v1`. The companion MIT license applies. Its vocabulary
mapping is defined in `tools/dialogue/semantic_ids.py`; the tokenizer metadata
in the `.ccv2` container is retained for native tensor-layout compatibility.

Model SHA256: `40325ff95fd53076f2419d7c39d74a0a19c6dfdd57e49608289c48f4eb526c74`.

The run warmed from the JSON syntax checkpoint for 800 CPU steps, batch 16,
seed 19, using the same 8,299 training cases. It matches the procedural policy
on all 1,018 test inputs and 112 sampled development inputs. All 112 native
checks match Python, and three real pairs complete 15 turns. Scope remains
the seven-move policy; see `tools/dialogue/KNOWLEDGE-COVERAGE.md` for the larger
simulation knowledge gaps.

Average test prefix length is 23.87 IDs, versus 222.75 BPE tokens for JSON.
Output is three semantic IDs and EOS, versus 24.75 tokens on average for JSON.
The act record occupies four bytes within a versioned protocol. Native timing
for 24 sequential paired process launches had medians of 45.6 ms for semantic
IDs and 346.1 ms for JSON, including process startup and model loading. These
are diagnostic timings on one host, not an in-game frame-time guarantee.

Build and run with the commands in `tools/dialogue/SEMANTIC-IDS.md`, using
`models/dialogue-semantic/model.ccv2` as the model path. This is the offline
native policy checkpoint; the in-game Chat path remains a separate integration.
