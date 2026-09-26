# The Return: the combined moment (M1 + M2 + M3)

These frames show all three milestones together: the last-seen record and
the digest (#938), the staged arrival scene (#940), and the gate voice
(#939). They also show the follow-up that makes the voice happen at the
gate. Seed 4. The company leaves, waits the given days, and rides back by
the real roads.

Frames are saved as 256-colour PNGs to keep the folder small.

## Gloamgate, seed 4

| Frame | What it shows |
| --- | --- |
| `gloamgate-before-0-days.png` | Back after 0 days: a calm evening gate, a stocked stall. |
| `gloamgate-after-380-days.png` | Back after 380 days: two smoke columns over burned roofs, a hungry crowd on the left of the road, a boarded stall on the right. |
| `gloamgate-gate-voice-380-days.png` | The carriage stops by the resident waiting inside the gate, beside the stalls. "Varkesh the Unappeased burned most of the town. It was over missing hoard money, I hear." |
| `gloamgate-gate-voice-380-days-parked-before-fix.png` | The same voice on main before the follow-up: it opened after the carriage parked in the coach court, far from the staged gate, with both people turned away from the camera. |

## Thornford (385 days) and Silverwick (375 days)

| Frame | What it shows |
| --- | --- |
| `thornford-after-385-days.png` | The river-crossing gate with bare stalls. |
| `thornford-gate-voice-385-days.png` | On the bridge by the stall: "There's no bread or wool in the market." |
| `silverwick-after-375-days.png` | A hungry crowd by the gate and bare stalls. |
| `silverwick-gate-voice-375-days.png` | By the first stall: "The streets are not safe now." |

## How to reproduce

```
crownless_carriage --capture-return 4 380 Gloamgate after.png
crownless_carriage --capture-return 4 0 Gloamgate before.png
crownless_carriage --capture-gate-voice 4 380 1 voice.png
crownless_carriage --capture-return 4 385 Thornford thornford.png
crownless_carriage --capture-gate-voice 4 385 0 thornford-voice.png
crownless_carriage --capture-return 4 375 Silverwick silverwick.png
crownless_carriage --capture-gate-voice 4 375 3 silverwick-voice.png
```

`--capture-gate-voice` takes the same trip as `--capture-return` and shows the
moment the game opens the voice.

## Notes

- The conversation shot is close, so the smoke and crowd are in the arrival
  shot just before it, not in the voice frame itself.
- In Silverwick the scene stages hunger while the voice leads with
  lawlessness. The digest ranks the security change first; hunger is the
  "Tell me more" line.
