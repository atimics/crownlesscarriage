# 04 - NPC decision-making and learning in a world

Method note: "full" = page or PDF body read. "abstract" = only the abstract/summary was read. "search" = only a search-result snippet; treat as weaker. Claims from "search" are marked UNVERIFIED where used.

## Sources read
1. https://arxiv.org/pdf/2110.07594 (full, via pdftotext) - Neural MMO v1.5. 128x128 map / cap 256 agents ("SmallMaps"), 1024x1024 / cap 1024 ("LargeMaps"); reward is only -1 for dying; skills appear in order forage, explore, fight, hunt NPCs.
2. https://arxiv.org/abs/2304.03442 (abstract) - Generative Agents: memory, reflection, planning; 25 agents; ablation says all three matter.
3. https://arxiv.org/abs/2411.00114 (abstract) - Project Sid / PIANO: 10-1000+ LLM agents in Minecraft, roles and rules emerge; "preliminary".
4. https://arxiv.org/abs/2305.16291 (abstract) - Voyager: skill library of code, auto curriculum, 3.3x items, 15.3x faster tech tree.
5. https://arxiv.org/abs/1909.07528 (abstract) - Hide and seek: six emergent phases from pure team competition, large scale RL.
6. https://arxiv.org/abs/1901.01753 (abstract) - POET: paired environment + agent generation, transfer between envs is key.
7. https://arxiv.org/abs/1504.04909 (abstract) - MAP-Elites: a map of best solution per behavior cell.
8. https://arxiv.org/abs/2107.06857 (abstract) - Melting Pot: 80+ scenarios; generalisation to unseen co-players exposes weaknesses hidden by training metrics.
9. https://arxiv.org/abs/2312.03664 (abstract) - Concordia: Game Master turns free-text agent intents into world effects.
10. https://arxiv.org/abs/2004.13332 (abstract) - AI Economist: two-level RL (agents + planner), 16% better equality/productivity trade-off, robust to agents gaming the policy.
11. https://arxiv.org/abs/1703.03864 (abstract) - Evolution Strategies: scalar-only communication via shared random seeds, 1000+ workers; invariant to reward delay, no discounting.
12. https://arxiv.org/html/2302.09334 (full) - Eco-evolutionary non-episodic neuroevolution: 200x400 grid, 330 start agents, cap 1000, 1,000,000 steps with no resets; boom/bust cycles like Lotka-Volterra, then plateau at carrying capacity.
13. https://www.gamedeveloper.com/design/building-the-ai-of-f-e-a-r-with-goal-oriented-action-planning (full) - GOAP: ~70 goals, 120 actions, A* plans of 1-2 actions (sometimes 3-4), 3-state FSM, constant plan revalidation.
14. https://en.wikipedia.org/wiki/Utility_system (full) - utility AI: score every action, pick best or weighted random; Sims uses needs x object advertisement.
15. https://www.gdcvault.com/play/1012410/Improving-AI-Decision-Modeling-Through (full, description only) - Dill and Mark GDC 2010: response curves, population distributions, weighted random. The 2015 IAUS talk is known only via Wikipedia, slides not opened.
16. https://arxiv.org/abs/1709.00084 (abstract) - Behavior trees: modular and reactive, generalise FSMs.
17. https://www.guerrilla-games.com/read/killzone-2-multiplayer-bots (page only; slides not opened) - HTN planner with commander, squad and bot layers. "Replans 5 times per second" is UNVERIFIED (search snippet).
18. https://dwarffortresswiki.org/index.php/DF2014:Need (full) - needs set focus, not mood; need value refreshed to 400 when met; weights 1-10 by personality; need jobs have low or high priority.
19. https://faculty.cc.gatech.edu/~turk/bio_sim/articles/tierra_thomas_ray.pdf (full, pdftotext) - Tierra: reaper starts at 80% memory, then memory stays ~80% full; errors move a creature up the kill queue; parasites, immunity, hyper-parasites evolve.
20. https://www.cse.msu.edu/~ofria/pubs/2009AvidaIntro.pdf (full, pdftotext) - Avida: birth replaces parent, a neighbour, or anyone; "replace oldest" evolves faster than random because under random kill about half die before one offspring.
21. https://arxiv.org/pdf/1112.3574 (full, pdftotext) - Polyworld: every action costs energy incl. neural activity; population cap 300; "replace least fit" at cap; re-seed from an N-best list when population falls below a minimum.
22. https://en.wikipedia.org/wiki/Lotka%E2%80%93Volterra_equations (full) - cycles, unstable extinction point; helping prey raises predator density not prey density; carrying-capacity extensions (Rosenzweig-MacArthur).
23. https://arxiv.org/abs/2005.03742 (abstract) - Lenia: rich self-organising patterns from one continuous rule; found by genetic search.
24. https://hugocisneros.com/notes/lenia/ (thin) - Lenia definition only.
25. https://hugocisneros.com/notes/brantdiversitypreservationminimal2020/ (thin) and search results on Minimal Criterion Coevolution (Brant and Stanley, https://dl.acm.org/doi/10.1145/3071178.3071186): reproduce only if you meet a minimal criterion; resource limitation (queue capacity) preserves diversity. Details UNVERIFIED.
26. https://github.com/a16z-infra/ai-town (full README) - Convex backend, "determinism guardrails", vector-search memory, engine pauses when idle.
Search-only, not opened: Quality Diversity frontier paper (Pugh, Soros, Stanley 2016), CMA-ME, Colledanchise BT book. The Sims (2000) utility detail comes from source 14 only.

## What to steal for sim2's decide() interface and role design
- Make the option list the contract. GOAP, utility AI and HTN all reduce to "pick one of N candidate things". Pass decide(role, obs, options) a fixed-size option list with masks (legal or not), and let the answer be an index. A rule brain can be utility scoring; a net outputs logits over the same slots. (Design inference from 13, 14, 16.)
- Keep rules as the floor and as the mask. F.E.A.R. kept FSM states tiny (GoTo, Animate) and put intelligence in goal selection; plans are only 1-2 actions. So sim2 decisions should be coarse and sparse (what to do next, where to go), not per-tick motor control. Event-driven decisions fit: call decide only when a goal finishes, is invalidated, or a need crosses a threshold. GOAP's continuous plan revalidation = an "interrupt" event type.
- Needs as the observation. Dwarf Fortress and Sims give a ready observation vector: per-need satisfaction with a fixed max and personality weights (1-10). Feed needs plus weights to the net; rules use them as utility curves. Personality weights per actor give behaviour diversity for free.
- Utility curves are good rule baselines: response curves and weighted-random pick (Dill and Mark) avoid the deterministic "always same best" look. Use the seeded RNG for the weighted pick so replays stay exact.
- Hierarchy: a commander layer (HTN in Killzone 2: commander, squad, bot) maps to goblin faction and caravan-level decisions vs individual ones. Give each role its own decide() head; do not share one net across roles unless the obs is role-tagged.
- Train whole roles on survival/reproduction, as Neural MMO did: reward was only -1 for dying and a skill ladder (forage, explore, fight, hunt NPCs) emerged. This supports the new plan over per-decision scoring.
- ES fits: only scalar fitness and seeds need to move between workers, and it is invariant to reward delay and horizon, so sparse survival-at-end signals are fine (source 11).
- Evaluate on unseen co-players (Melting Pot idea): hold out opponent brains and worlds; training curves hide weakness.
- Use QD for the population, not just one champion: MAP-Elites archive over role-behavior descriptors (e.g. trade vs raid rate for goblins, hunger threshold at which a townsperson turns bandit). Keeps diverse brains alive and gives fallback brains.
- Log-derived fitness is sound: tick-deterministic logs mean fitness = lifespan + offspring computed after the fact.

## How to keep the ecology alive while training (numbers where sources give them)
- Do not reset the world per episode if you want real ecology: source 12 ran 1,000,000 steps, start 330, cap 1000; it had booms and busts, near tragedy-of-commons, then a plateau at carrying capacity. Expect the first crash.
- Population cap plus replace-the-worst. Polyworld: cap 300, replace least fit at cap, minimum-population re-seeding from an N-best list. Tierra: reaper at 80% memory keeps it ~80% full. Avida: replace oldest beat random replacement. Sim2 analog: cap per role, spawn from best-so-far brains when a role falls below a floor, and cull by age or failure not at random.
- Scale matters: Neural MMO agents trained with population cap 4 only forage near spawn and are unstable; cap 32 and 256 learn map-wide foraging; small-pop agents starve when test cap is raised. Train at the population density you deploy.
- Bigger and longer is less stable: Neural MMO LargeMaps (1024 agents, 8192 steps) showed unstable learning and policy degradation with continued training; 128 agents / 128x128 / 1024 steps was the practical sweet spot. Keep training worlds small; checkpoint and stop early.
- Regrowth design: source 12 used neighborhood-dependent growth, niche-dependent rates, and sparse spontaneous spawn so resources never hit exact zero. Do the same for food, ore, carriage traffic.
- Predator-prey reality (Lotka-Volterra): oscillation is default and extinction is an absorbing state in small populations. Bandits and dragons are predators; give them a prey-dependent floor (bandits only recruit from hunger, which already couples them to prey) and keep refuges. Improving prey conditions feeds predators, not prey, so raising food will boost bandits/guards first.
- Seed a minimum viable founder set for every role so the learner never sees a world where its role is extinct. Polyworld's continual re-seeding is the precedent.
- Scripted backstops: keep rule-based brains for some fraction of each role (Neural MMO compared against scripted baselines). Fraction is a design choice; sources give no number. UNVERIFIED best value.
- Energy costs on everything, including "thinking" if desired (Polyworld charges neural activity) - discourages idle loops.
- Competition as curriculum (hide and seek, Neural MMO): do not hand-shape rewards; pressure from other learners creates the ladder.

## What to avoid
- Single-decision hand objectives (already found brittle). The AI Economist result that agents game a fixed policy is a warning: any proxy reward gets exploited.
- Unbounded evolution: parasite, cheater and hyper-parasite chains appeared in Tierra. Expect goblins to find smuggling exploits; put the exploit check in the event log audits.
- Random culling: in Avida roughly half of organisms die before one offspring under random replacement; it wastes evolution.
- Huge worlds/populations before the small one is stable (Neural MMO degradation).
- LLM agents (Generative Agents, Project Sid, Voyager, Concordia, AI Town) as runtime brains: they are non-deterministic and costly; AI Town only offers "determinism guardrails" in its backend. Steal the ideas (memory summaries, skill library, Game Master validating intents) not the models. Sid results are "preliminary" (abstract).
- Cross-role single net, or one shared reward across roles.
- Reading success off training curves only (Melting Pot).
- Fine-grained per-tick decisions; plans that last 1-2 actions already looked intelligent in F.E.A.R.

## Open questions
- What fraction of each role should be scripted vs learned during training? No source gives a number.
- Does survival fitness for bandits conflict with the town's ecology (bandits that win too well starve the town)? Needs a co-evolution schedule (source 12 suggests cycles, then plateau, but with a different setup).
- Reproduction for learned brains: sexual mating (Polyworld) vs copy-with-mutation into ES population? Sources cover both, sim2 must choose.
- How many ticks per generation for a 5M parameter net under ES; sources give only that ES scales to 1000+ workers, nothing about nets this size in this setting.
- Does QD archive descriptors need to be hand-chosen per role? MAP-Elites says the user picks them.
- Extinction handling inside an ES generation: if a role goes extinct mid-rollout, is fitness censored or zero? Unanswered in sources read.
- Not verified: IAUS GDC 2015 details, Killzone replanning rate, MCC resource limitation mechanism, Sims needs constants.
