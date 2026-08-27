# Robotics Algorithms for Character Movement

Research reviewed on 2026-08-27. The goal is not to copy a robotics stack into
the game. The goal is to take small ideas that improve deterministic movement
without adding a trained model or a second physics world.

## Applied now

### Overlapping spheres for articulated links

[SPARROWS (RSS 2024)](https://www.roboticsproceedings.org/rss20/p035.html)
represents the reachable volume of an articulated robot with overlapping
spheres. [Collision-Affording Point Trees (RSS 2024)](https://www.roboticsproceedings.org/rss20/p038.html)
shows how useful point-space collision queries are for fast planning.

Crownless now samples every healthy link in the generalized limb rig with
overlapping spheres. Quadrupeds, hexapods, and octopods therefore cannot move a
leg through solid level geometry merely because their root capsule is clear.
The humanoid keeps its tuned root capsule, ragdoll particles, and weapon
contacts because those already cover its physical movement rules.

### Predict motion before characters overlap

[Collision Avoidance in Model Predictive Control using Velocity Damper
(ICRA 2025)](https://gepettoweb.laas.fr/articles/haffemayer2025.html) uses
velocity constraints to react before a collision. The game's crowd controller
now finds each pair's closest approach over a short horizon. It gives both
characters equal and opposite corrections before they enter personal space.
The rule is deterministic and still uses role-specific combat, ally, and
bystander clearances.

### Prefer traversable terrain, not only legal terrain

[TOP-Nav (CoRL 2025)](https://proceedings.mlr.press/v270/ren25a.html) combines
terrain, obstacle, and movement feedback instead of treating path planning as a
flat map problem. Crownless A* edges now cost more when they climb sharply or
cross a tilted surface. The existing movement-stall feedback can still request
a fresh path when the physical gait cannot follow a corridor.

## Reviewed but not added

- [Agile But Safe (RSS 2024)](https://www.roboticsproceedings.org/rss20/p059.html)
  and [SATA (RSS 2025)](https://www.roboticsproceedings.org/rss21/p124.html)
  use learned policies and safety or adaptation layers. Their separation of
  normal movement from recovery is useful, but adding neural inference would
  weaken replay and is not needed for the current authored world.
- [Avoid Everything (CoRL 2025)](https://proceedings.mlr.press/v270/fishman25a.html)
  learns collision-free motion under partial observation. Crownless has exact
  level geometry, so a learned perception and planning stack would solve a
  problem the game does not have.
- A point-tree or other spatial index is not needed yet. The current collision
  world contains a small fixed list of boxes. Add an index only if profiling
  shows that dynamic point obstacles become a real cost.

## Validation contract

- Link samples must overlap and keep a stable radius.
- Disabled limbs must leave the collision proxy.
- Approaching actors must receive reciprocal corrections; separating actors
  must receive none.
- Terrain cost must never be less than geometric distance, so the existing
  straight-line A* heuristic remains valid.
- All rules must remain deterministic across frame rates and platforms.
