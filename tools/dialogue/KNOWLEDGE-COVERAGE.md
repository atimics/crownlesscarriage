# Dialogue grammar and simulation knowledge

The current dialogue policy covers seven moves, three topics, three plans and
three conditions. It handles food, safety and seeking paid work. Its input uses
own goal, hunger, distress, courage, coins and public dialogue acts. These are
the current model's learning boundaries.

The simulation declares 139 event kinds and 67 non-empty command kinds. The
account grammar now has 158 rules covering all 139 event kinds. The earlier
inventory covered 44. This measures event-kind coverage: each kind has a
source-backed parsing and rendering form. Each kind can have further wording
variants, and distorted retellings can still fall outside these forms.

`QUEST_PROGRESS` now records actual advances caused by quest evidence. Its
parent links to the evidence event. Historical `PLAYER_AMBUSH` formats remain
supported for old accounts, with their source commit recorded in a fixture.

Run `python3 tools/dialogue/audit_grammar.py --check --output coverage.json`.
CI requires a rule for every declared event, checks source wording against
simulation formats, and checks the generated native grammar. Regression tests
add an event, remove its rule, and invent a source pattern to prove failure.
Native tests exercise every rule's rendered forms and recovered field spans.

The compact 5M checkpoint still learns seven dialogue moves. The event fact
bridge below supplies the data contract for a future event-selection policy.

| Simulation knowledge | Existing representation | Current dialogue policy | Next grammar work |
| --- | --- | --- | --- |
| Needs and intentions | Four goals, six activities, hunger, shelter, stress, courage, money | Uses a small state subset; discusses food, safety and work | Shelter, travel, recovery, preparation, competing needs |
| News and events | 139 event kinds, named entities, dates and causal event links | Generic topics | Report or ask about a selected known fact |
| Personal knowledge | Rumor, witness account, immediate stake, offer; eight slots | Fields remain outside the policy input | Select owned fact slots and reason over their typed contents |
| Evidence | Doubtful, told, witnessed; named source, day, private flag | Fields remain outside the policy input | Source, certainty, date and disclosure rules |
| Memory | Four player-interaction kinds; subject/event/day; four slots | Only current public dialogue history | Recall promises, help and withdrawal, then track their outcomes |
| Relationships | Affinity, trust, obligation; friends, former partners, rivals, coworkers | Fields remain outside the policy input | Help, refusal, trust, affection, reconciliation and obligations |
| Trade | Goods, stock, prices, quantities, purses and commands | Discusses paid work and agreeing pay | Typed offers, counteroffers, quantities and resource checks |
| War and dragons | Raids, wars, omens, retaliation, battles, losses, aftermath | Generic concern | Named threats, warnings, routes, losses and response plans |
| Execution | 67 command kinds, many scoped to the player | Conversation ending; other acts express intentions | Actor-scoped command validation and observed results |

## Facts before more moves

The event bridge builds a bounded table of facts owned by the participant.
Each fact needs a stable reference, predicate, entity references, typed values,
date, source, certainty and disclosure status. Preserve the original evidence
alongside the compact model input. Select from the person's own held accounts,
knowledge and observations. Global world state becomes knowledge through the
simulation's observation rules.

Then add public acts such as `report(fact_slot)`, `ask(predicate, entity_slot)`,
`warn(fact_slot)`, `offer(give, receive)`, `promise(action)` and `recall(memory_slot)`.
The act references a fact; the renderer chooses the language. The listener gets
the communicated claim with its source and certainty, and can compare it with
their own information. Reply references and proposal terms remain explicit.

`event_facts.py` now provides the first bounded bridge. `build_facts` reads only
the participant's `held_accounts`, gives each account a stable owner and event
digest reference, and keeps its kind, text, source, day, confidence, and private
flag. Knowledge records supply the simulation's `doubtful`, `told`, or
`witnessed` certainty. `validate_act` accepts only `report`, `ask` and `warn`
references from the current participant snapshot. Private facts stay private.
`validate_event_registry` checks all 139 simulation event kinds. The renderer
quotes held text with source and day attribution. Certainty applies when the
knowledge record has the same text as the held account. This bridge is ready
for event-selection training; the current compact checkpoint uses its existing
seven-move policy.

The current four-byte act record is suitable for its small fixed vocabulary.
General facts and actions need additional typed records: entity references,
quantities, dates and lists of conditions. Extend the versioned format with
bounded payloads as these capabilities are implemented.

## Love, terror and family loss

Affection and conflict can build on existing relationship values and histories.
The simulation has ancestry IDs and character deaths, while the current dragon
retaliation path changes aggregate population, stocks, buildings and routes.
It records a named dragon burning a settlement. A specific claim such as
watching one's family die needs named victim links, family relationships,
presence/witness evidence and a lasting personal memory. Those links need
simulation work as well as grammar work. More horror wording alone would leave
that grounding gap.

Suggested order: owned fact references and reports; needs and typed trade;
promises and remembered outcomes; relationship exchanges; then witnessed harm
and grief after the simulation records the required personal events.
