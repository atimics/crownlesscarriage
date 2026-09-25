# The Return, milestone 3: the gate voice

Seed 4. The company leaves Thornford, rides to Silverwick, waits 365 days, and
rides back. At Gloamgate (index 1) a resident at the gate says what changed.

| Frame | Line | How the resident knows |
| --- | --- | --- |
| ![Fire](gate-fire.png) | "Missing hoard money was blamed for Varkesh the Unappeased burning Gloamgate, I hear." | Their own telling of the DRAGON FIRE story (93% sure, one retelling). |
| ![Hunger](gate-hunger.png) | "Hunger was reported in Gloamgate, I hear." | Their telling of the SHORTAGE story, after "Tell me more". |
| ![Last](gate-last.png) | "Meat costs more than it did." | Seen: no story, so only what is plain to see. The last change offers only "Thank you". |

The fire and hunger stories are marked as told when they are spoken, so the
digest ranks them lower. The lost market is not spoken, because the same
DRAGON FIRE story explains it and the company has now been told.

## Capture recipe

```sh
crownless_carriage --capture-gate-voice 4 365 1 gate-fire.png
crownless_carriage --capture-gate-voice 4 365 1 gate-hunger.png 1
crownless_carriage --capture-gate-voice 4 365 1 gate-last.png 15
```

The arguments are SEED DAYS TOWN FRAME [TURNS]. TURNS is how many times
"Tell me more" is chosen. The text tool prints every line with the evidence of
each clause:

```sh
crownless_return_digest --seed 4 --days 365 --voice
```
