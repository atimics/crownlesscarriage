# Procedural syntax prototype

The model-facing output is a public act with closed symbols and reply indices.
One person's state and the public exchange determine each turn. Human and
Hra'khor text are separate views of the same act. The native goblin vocabulary
is shared with existing game speech.

Three real snapshot pairs produced 15 procedural turns: Jory and Bren in
Silverwick, Chenric and Harthild in Thornford, and Ilyra and Tomas in Gloamgate.
The Silverwick snapshot was freshly exported with the new build at seed 1202,
day 1. The other two reuse the prior reviewed snapshots. All are existing
training-world groups. The archive contains source snapshots, per-person
training rows, both surface renderings and generation hashes.

The exchanges cover safety, seeking paid work with a pay condition, and asking
about food with a distribution preference. Each ends after five turns. These
are authored policy examples, with limited decision variety. They demonstrate
the meaning boundary and renderer path. They are a starting point for richer
state-driven policies and a future syntax-target trainer.

Validation passed all five focused CTest suites: dialogue syntax (eight Python
cases), native Hra'khor, existing Hra'khor pairs, generated account rules, and
native account parity. Checks include malformed acts, wrong reply targets,
unsupported needs, incompatible conditions, refusal, terminal turns, separate
private inputs, source-state preservation and native goblin output. Built with
warnings as errors.

See `tools/dialogue/SYNTAX.md` for commands, vocabulary, policy assumptions and
the next training step. `syntax-prototype.json.gz` preserves the sample results.
