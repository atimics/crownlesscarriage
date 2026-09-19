# Compact student inputs

The six independent teacher turns compile into the native 512-token context.
Input selection is independent of the answer. Each row keeps the complete
latest heard turn, current needs, identity and dragon account. The resulting
token IDs match the native participant inference entry point exactly.

| Turn | Speaker | Input tokens | Reply tokens | Earlier speech omitted |
| --- | --- | ---: | ---: | --- |
| 0 | Harthild | 225 | 122 | none |
| 1 | Hartha | 336 | 140 | none |
| 2 | Harthild | 345 | 127 | turn 0 |
| 3 | Hartha | 341 | 124 | turns 0–1 |
| 4 | Harthild | 351 | 127 | turns 0–2 |
| 5 | Hartha | 341 | 116 | turns 0–3 |

Turn 2 also omits band and faction. Each row records every omitted optional
record. The input plus reply stays within 478 tokens for these samples; EOS is
the final prediction. The loss masks the input and learns one actor's output.

This shows that the compact format can carry this encounter into training and
native inference. It also shows how quickly older dialogue leaves a 512-token
window. Review of durable memory selection and a longer context belongs in the
next model comparison. The source contribution review still applies: turns 3
and 4 repeat an agreed plan. Every compiled row awaits compact-context review.

`compact-inputs.json.gz` maps original artifact names to exact text: six previews,
an empty rejection file, and the receipt. SHA-256:
`5eb2516460f21ce776c6362dc13a587d817d6502c872b5a6906de5f32e059eb6`.
The receipt binds the compiler, native probe and source rows. The source rows
come from `paired-teacher.json.gz` in this directory.

Validation passed: 11 focused Python tests; native model parity; conversation
and participant grounding checks; model table checks; paired worker protocol.
The worker transport test covers fixed identity and exact failed-output bytes.
The trained participant checkpoint remains future work.
