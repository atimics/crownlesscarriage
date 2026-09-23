# Typed fact selection: dialogue samples

The typed selection model run against real grammar accounts. Each account's
parsed fields become typed facts; a natural question is parsed to a role; the
model picks a fact or defers; the renderer owns the wording.

Reproduce with the trained export (4,000 steps, seed 19):

```sh
python tools/dialogue/fact_demo.py --zero /path/to/zero \
  --model /tmp/fact-policy/last.ccv2 \
  --tokenizer assets/language/tokenizer.json
```

## Multiple facts from one event

The decisive capability: the same account answers different questions with
different fields.

**Notice (actor, place, object)**

- opening: In Yororormere, Yorororholt Kelumeth put up a notice about Yorilashfell Cup, according to the word going round.
- Q: Who posted that? → `report(actor)` → *I saw it myself: Yorororholt Kelumeth (on day 10).*
- Q: Where was the notice posted? → `report(place)` → *I saw it myself: Yororormere (on day 10).*
- Q: What was the notice about? → `report(object)` → *I saw it myself: Yorilashfell Cup (on day 10).*

**War declaration (actor, recipient)**

- opening: Kelenumden Kelowowmo and Yorenenford Yoraorme went to war.
- Q: Who made war? → `report(actor)` → *I saw it myself: Kelenumden Kelowowmo (on day 10).*
- Q: Who did they declare war on? → `report(recipient)` → *I saw it myself: Yorenenford Yoraorme (on day 10).*

**Dragon patron (actor, recipient)**

- opening: The dragon host gained funding from Yoraawick Yorethashf, who named Yorililfell Kelaenwi its champion.
- Q: Who funded the dragon host? → `report(actor)` → *I saw it myself: Yoraawick Yorethashf (on day 10).*
- Q: Who was chosen as champion? → `report(recipient)` → *I saw it myself: Yorililfell Kelaenwi (on day 10).*

## The two grounded-dialogue residual misses

The free-text follow-up (ZERO PR #59) answered the place field when the question
asked for the object or material. Typed selection gets both.

- paper mill — Q: What was the paper made from? → `report(material)` → *I saw it myself: Rags (on day 10).*
- harvest — Q: Where did the harvest fail? → `report(place)` → *I have a doubtful account: Kelilowden (on day 10).*

## Abstention and uncertainty

- death — opening says "Someone died after a life in Yoraamere". Q: Who died? → `defer` → *I do not hold that account.* The actor field is hidden, so the model refuses rather than inventing a name.
- Q: Where did they live? → `report(place)` → *I have a doubtful account: Yoraamere (on day 10).* Certainty travels with the fact.

## Known limitations

After the predicate parser and grammar-role reconciliation
(`fact_question.parse_question`, `fact_roles.resolve_field`), the earlier
mismatches are resolved:

- harvest — "Who needed the supplies?" resolves to the second `place` field and answers *Yorashormere*.
- bandit — "Which band did they join?" resolves the joined band to the `recipient` field and answers *Kelashorgate Kelenas*.
- war — "Who did they declare war on?" answers *Yorenenford Yoraorme*.
- dragon patron — "Who was chosen as champion?" answers *Yorililfell Kelaenwi*.

The remaining limits:

- horse breeding — "Which horses were bred?" names a pair, but the role resolves to one field, so it returns one of the two horses.
- Of 158 grammar rules, 17 label two spoken fields with the same role (one core meaning, 16 `*_grounded_*`/`*_source_*` rules). Those need predicate-level routing or a grammar revision before role-only routing works.

The parser maps all eight held-out `question-contrasts` questions to the right
role. The remaining misses are the natural-question parser and the grammar's
field roles, which are the next integration step.
