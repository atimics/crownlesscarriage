# Semantic IDs and coverage audit

The model now learns three semantic output IDs plus EOS. The C runtime accepts
integer prefix arrays and returns integer output arrays. The binary act codec
uses a four-byte little-endian record: opcode, argument, reply index. English
and Hra'khor still render the decoded meaning. JSON appears in inspection logs.

| Measure | JSON checkpoint | Semantic-ID checkpoint |
| --- | ---: | ---: |
| Mean test input tokens | 222.75 | 23.87 |
| Mean output tokens, including EOS | 24.75 | 4 |
| Test policy matches | 1,018/1,018 | 1,018/1,018 |
| Native/Python parity sample | 112/112 | 112/112 |
| Median native probe process time | 346.1 ms | 45.6 ms |

The timing comparison uses 24 sequential paired launches and includes process
startup and model loading. It compares the same private inputs and public acts.
Raw results, timings, datasets, source hashes and model receipts are saved in
`semantic-ids-training.json.gz`. Three saved real pairs complete 15 turns through
the semantic model and native renderer. The existing held-out tests concern a
small authored policy; broader knowledge coverage remains separate work.

Training used the existing 5M architecture with a fixed semantic-ID vocabulary,
800 steps, CPU, batch 16 and seed 19. The output projection scores the legal
semantic vocabulary, with no position-specific grammar mask. Whole acts are
validated after generation. The checkpoint is in `models/dialogue-semantic`.

The audit found 139 event kinds and 67 command kinds in the simulation. The
older account grammar covers 44 event kinds with 61 templates. The new policy
covers seven moves and three topics, with zero concrete event-claim forms.
`grammar-coverage.json` contains the exact covered/missing event lists and
source hashes. `tools/dialogue/KNOWLEDGE-COVERAGE.md` maps the missing personal
knowledge, evidence, memories, relationships, trade, war and action support.

Eight focused CTest suites passed, including C API input/reset tests, binary
codec validation, policy training contracts and existing language tests. The
13 existing participant compiler tests also passed.
