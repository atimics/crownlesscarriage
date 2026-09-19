# Dialogue syntax checkpoint

`model.ccv2` is a 4,945,153-parameter policy checkpoint for
`crownless-dialogue-syntax-v1`. It uses the existing tokenizer and the MIT license
in this directory. Human and Hra'khor wording comes from the shared renderer.

- Model SHA256: `321431dc03e6dd6ae7749b6e991004b64f9cc2f83930ddd3c385ab1a4863eb26`
- Tokenizer SHA256: `c572de53eb4e739a8ce941ac03d1d5fb6173623af786787cf786b41fd34e9af4`
- Architecture: width 192, eight layers, six heads, feed-forward width 624,
  vocabulary 4096, context 512. Export uses row-scaled int8 weights.
- Training: 1,200 steps from the existing core, then 800 repair steps. CPU,
  batch eight, seed 19. The second dataset has 8,299 training rows, 983
  development rows and 1,018 test rows.
- Final checks: all 1,018 test rows match the authored policy; all 112 sampled
  native outputs match Python byte for byte. Three real snapshot pairs complete
  five-turn exchanges. Each prediction is one person's public act.
  Six further fresh-world pairs complete 30 native turns matching the policy.

This is the first seven-move policy. Its test measures this finite vocabulary
and policy on held-out state combinations. Most decision patterns appear in
training; broader story reasoning and autonomous execution need further work.
The in-game Chat path still uses the existing core checkpoint. The native pair
runner selects this syntax checkpoint explicitly.

See `tools/dialogue/TRAIN-SYNTAX.md` for reproduction and native execution.
The two run archives under `docs/reviews/participant-minds-2026-09-19` preserve
training data, outputs, receipts and the first work-scene failure.
