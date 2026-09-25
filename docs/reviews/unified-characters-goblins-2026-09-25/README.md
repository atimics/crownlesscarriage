# Goblins as one skinned GLB each (2026-09-25)

Design: `docs/design/unified-characters.md`.

"Before" frames come from main before this change (hand-drawn C goblin rig).
"After" frames come from this branch (one skinned GLB per goblin, posed by
the humanoid skin solver from the goblin creature rig).

| Frame | What it shows |
| --- | --- |
| `cave-before.jpg`, `cave-after.jpg` | `--capture-creatures goblins`, game size. Raider in front of the Cinder Tithe, scavenger behind the banner. |
| `reel-before.png`, `reel-after.png` | `--capture-creature-reel goblins`, frames 0, 4, ... 28, crop at 1:1. The raider walks in place; the arms now swing against the legs and the gait is continuous instead of 8 stepped poses. |
| `mine-before.jpg`, `mine-after.jpg` | `--capture-mine-contest`, game size. Two half-size goblin haulers in combat. |
| `review-before.png`, `review-after.png` | `renderer_regression_tests --creature-captures`: three goblins, front, side and face. |
| `stride-after.png` | New sheet from the same run: each goblin in side view at contact A, passing A, contact B, passing B. |

What to look for:

- Shapes, sizes and palette slots match the old rig: ears, tusks, brows,
  eyes with dark pupils, court-colored hair, helmet and spear, pack and hook,
  offering box.
- The skin draws through the pack-controlled skinned shader, so it reads a
  little darker and flatter than the old world-shader draw; the goblin paint
  sits one value step up to keep faces readable at game size.
- Held props turn with the hand: the raider's spear tilts with the arm swing
  instead of staying upright.
- No `clamped to bind pose` or `not drawn; rig pose failed` lines in any of
  these runs.
