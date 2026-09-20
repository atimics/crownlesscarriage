# V3 food agreement trial

Fresh 4,945,153 parameter model; 2,500 CPU training steps, 80,000 choice targets.
The local training run took 317.64 seconds. The test split has 692 turns from
held-out world profiles: all choices were valid and 689 matched the authored
preference (99.57%). All 692 exported native choices matched Python. These
checks measure the food policy and its execution.

Export SHA-256: `1c9c42deeadaaaa1f421eff1ba07a3b245a506c1b3c869c313e74d5376593dd5`.

## Generated world

Seed 1202, day 31, Rosespire. The probe found Silva Fairweight hungry and
Reidel Rosethorn present with 23 crowns. These people and resources came from
the native simulation. The Hra'khor column renders the same acts with that
language pack.

| Person | Human | Hra'khor |
| --- | --- | --- |
| Silva Fairweight | I am hungry. Could you help me get something to eat? | Sha am hungry. Could thu help sha get zhek? |
| Reidel Rosethorn | I can buy 3 portions of food for you at Rosespire. It will cost me 9 crowns. | Sha can buy 3 portions of zhek fo thu at Rosespire. It will cost sha 9 crowns. |
| Silva Fairweight | Agreed: 3 portions of food for 9 crowns, now. | Agreed: 3 portions of zhek fo 9 crowns, now. |
| Reidel Rosethorn | We can speak again later. | We can speak again later. |

The accepted command moved 9 crowns from Reidel to the market and consumed
3 food units. Reidel kept 14 crowns. Store stock fell from 95 to 92. Silva's
hungry days fell from 1 to 0. Both people retained the outcome after save/load.
A repeated execute command left the complete world hash unchanged.

## Reunion

**Silva Fairweight:** You bought 3 portions of food for me. That help mattered.

**Reidel Rosethorn:** Thank you. I remember it too.

**Silva Fairweight:** We can speak again later.

## Controlled choices

With a target of 75 food units, changing observed stock from 16 to 74 changed
the trained helper's offer from 1 portion to 3. An empty purse led to a money
refusal. An empty store led to a stock refusal. These four cases are authored
fixtures; the generated-world trial above is a separate check.

Run `verify_meaning_rollouts.py` with the trained artifact and native probes to
recreate the checks. The release workflow saves the full records in its model
and run artifacts. See [MEANING.md](MEANING.md) for commands and current scope.
