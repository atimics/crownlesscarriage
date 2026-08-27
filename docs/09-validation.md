# Validation and Go/No-Go Gates

## Philosophy

The project does not expand because more systems compile. It expands when the
current slice demonstrates deterministic causality, player comprehension,
meaningful agency, and repeatable fun.

## Automated gates

### 1. Determinism

Two headless runs using the same seed and command log must produce identical
state hashes regardless of rendering frame rate.

Target: exact agreement across every validated test platform.

### 2. Persistence

Saving and loading at arbitrary days, during travel, and before or after local
interventions must reproduce the uninterrupted run.

The persisted hash includes world tick, sub-day time, journey phase and
progress, encounter/ambush decisions, reserved fare, and carriage state. A
schema-3 save verifies its historical hash before being migrated to this
runtime state.

Previously altered route and dungeon segments must retain authoritative
mutations.

Map identity, ownership, survey claims, legality, price, and carriage map-case
capacity must also survive an exact state-hash round trip.

Journal recovery tests keep a checkpoint behind the committed head and require
suffix replay to reconstruct the uninterrupted hash, including batched
real-time travel ticks. Checkpoint cursors must match their committed row,
ordinals must be contiguous, SQL update/delete attempts must be rejected, and
a privileged hash-chain modification must make loading fail rather than accept
divergent state. Schema-3 and schema-4 snapshots must verify their historical
hashes before migration to the current schema.

### 3. Referential integrity

No active route, shipment, situation, character memory, or causal event may
reference a missing or generation-mismatched entity.

Every physical map must reference an existing route and maker, have exactly one
valid owner, and be refused for travel when it is not carried or when the
carriage is not at one of its route endpoints.

### 4. Economic invariants

Across batch simulations:

- No negative inventories
- No undeclared goods creation or destruction
- No invalid treasury values
- No shipments exceeding route capacity
- No prices outside documented bounds
- No permanent collapse without a declared recovery path
- Food stocks remain below their physical storage limits after spoilage
- Toll, debt, tribute, theft, and hoard recovery move tracked crowns instead of
  minting or deleting them
- A diplomatic relation changes only after a courier arrives, unless a
  distorted message records an explicit false result
- A dragon host removes its Food, Tools, and Weapons from allied settlements;
  defeat consumes them and victory returns only the real cave hoard

### 5. Stability

Run at least 1,000 seeds for ten simulated years. Record:

- Settlement collapse rate
- Crisis duration and recovery time
- Price volatility
- Route concentration
- Faction dominance
- Bandit population pressure
- Monster outbreak duration
- Simultaneous situations per region
- Situations invalidated before player arrival
- Courier loss and distortion rate
- Time to alliance, dragon-host defeat rate, and time to hoard recovery

The acceptable band must be established through playtesting; universal
prosperity and universal collapse are both failures.

The current automated long-run band samples four deterministic worlds for 120
years after a 20-year burn-in. No sample may have average hunger at 60 or more.
The same run must still contain local crises with maximum hunger at 40 or more,
quiet years with every settlement below 25 hunger, and lasting population loss.
This is a guardrail, not a claim that the final balance is proven fun.

Recovery must keep its material cost. A badly reduced population eats less, a
starving community can fall back on subsistence work, and residents can reopen
a peaceful road with unpaid labor. Famine convoys may draw a granary below its
normal reserve and travel with an escort, but their food and crowns remain real.
None of these paths restores population, prosperity, road condition, or public
trust for free.

### 5a. Locomotion invariants

Both locomotion families must pass renderer-free tests. Robotic morphologies
must preserve every rigid segment, respect swing/support budgets, retain pinned
contacts, and produce bounded support feedback. Biomechanical morphologies must
retain their bone graph and mass, keep joints inside anatomical limits, recruit
the correct side of an antagonistic muscle pair, generate passive ligament
response, fall under gravity without support, remain still when gravity is
balanced by an equal external force, and report reaction torque or body impulse
when a contact constrains motion.

The biped fixture must traverse heel, flat, toe, and swing phases with both
legs; keep stance feet fixed; preserve thigh and shin lengths; keep knee angles
inside their declared limits; produce muscle-driven arm motion while walking
and damp it at rest; discard ground contacts while airborne; hand unsupported
motion to a ragdoll; preserve limb lengths through free fall and terrain impact;
make terrain contact with multiple body landmarks; keep post-impact upward
center-of-mass speed below the non-bouncing limit; recover through sustained
brace, kneel, and stand stages; bound every frame-to-frame recovery pose change
across every rendered landmark, including heel, ball, and toe; limit the
visible skin blend change at fall and recovery handoffs;
accelerate rather than teleport to requested speed;
generate a ground reaction near body weight; brake within a bounded distance;
walk uphill on a slope inside the friction budget without its support reaction
pushing it downhill; align the ankle and body frame to the contact normal;
keep its aggregate body root inside the anatomical support-height band; and
enter, traverse, and leave its biomechanical climbing mode without changing
bone lengths, exceeding anatomical arm reach, flipping an IK bend plane,
instantaneously acquiring a contact, or exceeding the whole-pose per-frame
continuity bound. Losing one climb contact must release that limb, lower control
authority, and remain controlled; sustained loss down to one contact must pass
through marginal support before passive physics. A biped whose ragdoll remains active must be refused climb
admission even when its navigation body is grounded. Fixtures must also enter
water without invoking fall control, remove both terrestrial contacts, retain
fixed arm and leg lengths, remain inside a bounded buoyancy height band, and
reacquire contacts on exit. Guard-to-strike and guard-to-swim transitions have
whole-pose continuity bounds, and one strike must expose exactly one consumable
impact window. Local-world fixtures must climb the actual 1.65-unit tower and
fall from all four
edges; after street impact, both center-of-mass and worst-particle upward speed
must remain below the non-bouncing bound, and body contact may not disappear for
more than four consecutive frames before recovery. Release-to-street time must
remain within the authored gravity band for every tested edge, and the generic
ragdoll fixture must reach its plane within a bounded free-fall frame window.
The agent position and velocity must match the authoritative fallen-body center
of mass throughout the fall. Ragdoll fixtures must enforce spine and cone
limits, knee and elbow hinges, and selected self-separation, detect a barrier
between capsule endpoints, and reject false stable support from contacts split
across different elevations. Walking, climbing, weapon sweeps, and falling body
segments must agree on shared geometry. Direct fixtures must scrape a fallen
body down a wall, hit both faces of a corner, land on a shoulder, and recover
from nearby geometry without penetration or a position snap.

### 6. Causal provenance

Every surfaced situation must trace through finite ledger parents to concrete
state changes. No major crisis may exist solely because a random roll requested
content.

### 7. Projection coverage

Every strategic condition included in the vertical slice must activate at least
two local projection channels. Major situation outcomes must change at least
four.

### 8. Painterly art stack

Run the complete visual gate from an active desktop session:

```sh
make art-check
```

This command first runs the C test suite, then checks the V05 animation frames,
hero and NPC paint channels, and NPC asset contracts. It then captures all ten
exterior camera rooms plus street, road, interior, and parley. The report is written to
`out/art-check/report.md`.

Each capture is cropped to the 457 by 285 world target. The gate fails on a
missing or blank subject, strong drift from the shared palette, weak value
separation, or an empty story center. It also records edge density and local
contrast for manual review. Each view gets color, grayscale, silhouette, and
three-value versions. Contact sheets show the ten rooms, the four scene types,
and the street value study.

The same run creates 35, 48, and 60 art-pixel hero comparisons from the checked
V05 source frame. Three repeat captures of one stationary room must stay inside
the small pixel-change budget. Use `python3 tools/art/run_art_check.py
--reuse-captures` to rebuild the report from existing captures without opening
the client.

## Player-experience gates

### Causality comprehension

After a short session, most testers should correctly explain:

- What is wrong
- Why it happened
- Who benefits
- Who suffers
- What actions could change it

### Emotional legibility

Players should recall affected people and places before recalling numeric
modifiers. If they describe only prices and meters, the projection has failed.

### Agency

At least three interventions must create measurably different outcomes over the
following thirty to ninety simulated days.

The player should neither feel irrelevant nor capable of magically repairing a
national economy with one ordinary delivery.

### Travel quality

Across five journeys using different seeds:

- Routine travel does not become repetitive friction.
- Interruptions embody changed world conditions.
- Each interruption introduces a decision beyond combat victory.
- Cleared or stabilized routes become meaningfully faster to revisit.

### Dungeon persistence

Testers should recognize that the dungeon belongs to the surrounding world and
continues changing after the expedition. Its outcome must affect routes,
settlements, factions, monsters, or production.

### Choice comprehension

Before a commitment, players understand two likely consequences. Afterward,
they can distinguish intended results from the delayed surprise.

## Forty-five-minute proof

At the end of a focused prototype session, a tester should answer:

1. Why does each visited settlement exist?
2. Why is one settlement currently suffering?
3. Which named people benefit from that suffering?
4. What did you transport instead of something else?
5. What consequence did your route choice cause?
6. Which person or place changed because of you?
7. What do you now fear will happen next?

## Go decision

Proceed beyond the slice only if:

- All automated integrity gates pass.
- The majority of testers understand the crisis without using the ledger.
- Different interventions create recognizable and persistent outcomes.
- Journey play remains interesting after repetition.
- The city, bandits, monsters, and dungeon feel like expressions of one world.
- Playing the crisis is more interesting than inspecting its debug charts.

## No-go response

Failure does not automatically mean adding content. Responses should be:

1. Remove invisible variables.
2. Shorten causal chains.
3. Strengthen recurring characters and physical projections.
4. Reduce routine interruptions.
5. Bound or merge overlapping systems.
6. Repeat the slice before expanding scope.
