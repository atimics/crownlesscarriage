# Participant policies

The policy has nine goal families and 38 speech acts. Each decision receives
one person's current view and the public exchange. It returns one typed act.

| Goal | Acts |
|---|---|
| Help | Request help, explain need, offer a crown toward food, exchange time |
| Trade | Offer paid work, counteroffer, explain a price, accept or decline |
| Safety | Warn, seek shelter, offer an escort, ask for daylight |
| Care | Ask about feelings, comfort, offer company |
| Grief | Express grief, recall a loss account, ask for space |
| Trust | Recall a memory, thank, express regret, offer time to rebuild trust |
| Conflict | State a grievance, explain a motive, request repair, forgive |
| Clan or faction | Request a contribution, discuss duty, bargain, refuse, threaten resistance |
| Learn | Ask, report a held account, explain a personal concern, express uncertainty |

Hunger, stress, courage, coins, trust, affinity, obligation, faction membership,
owned memories, and public held accounts affect the choices. The same question
can receive help, an offer, a refusal, or a safety concern. Hunger and fear can
interrupt another topic. Twelve public turns trigger a closing turn.

## Meaning before language

An act records version, goal, intent, actor, recipient, subject, reply index,
and any proposal, claim, or memory. Proposals name the payer, resource, cost,
and condition. Accepting an offer retains its exact terms and checks the
current speaker's means. Claims retain their owner, source, day, certainty,
and held-account reference. Private accounts stay in the private view.

These acts represent speech and provisional agreements. Execution of trades,
movement, and resource transfers belongs to the simulation command layer.
The participant view currently supplies coins and time as possible offers.
Its faction field supports faction discussions; clan-specific membership and
item inventories can extend that view. Loss speech refers to owned loss
accounts. Named bereavement needs an explicit relationship to the victim.

The policy uses the existing four memory kinds: meeting the player, a player
promise, help from the player, and the player withdrawing. A gratitude or
regret turn describes that record. Rendering quotes held accounts with their
source and uncertainty. Goblin rendering changes the authored surrounding
speech and preserves the quoted account and its names.

The English event parser remains an adapter for existing simulation messages.
The new model receives only numeric state features and act IDs. Event meaning,
participant choice, and language realization remain separate layers.

## Wire and native inference

Input starts with 1280, contains typed state features, topic, public last act,
and turn count, then marker 1580, valid candidate action IDs, and 1281. Each
candidate is checked against the current view. The model chooses one ID in
1024..1087 from that set. The wire grammar closes the record after that choice.
The native decoder implements the same candidate mask and fixed boundary.

The Python layer binds the selected ID to current proposal and evidence fields.
`pack_act` uses 18 bytes: version, action, and a 16-byte digest of the full act.
Reading the record requires the current participant context and checks that
the reconstructed act matches the digest. Public acts contain their explicit
meaning; the context-bound record is useful for compact local storage.

`policy_language.py` supplies English clauses. The native Hra'khor renderer
supplies the goblin voice. Changing those clauses leaves policy training IDs
unchanged. Ash stays shared, and vault is grak'rakh.

## Training and use

`train_policy.py` creates a fresh 4,945,153-parameter model with the native
architecture. The existing checkpoint supplies architecture and export metadata;
its weights are replaced before training. Training uses procedural single-turn
targets from branching exchanges, with balanced sampling over all 38 acts.

Own-state feature profiles stay in one train, development, or test split.
Exact observable inputs are deduplicated. These are synthetic policy-distillation
checks; human review and saved-world examples provide separate evidence.
The release gate requires every act to be valid and every native output to match
Python. Reference preference agreement must reach 99%; a valid choice can differ
from the procedural preference. Nine goal conversations and three saved-world
conversations are also replayed before publishing the model artifact.

```sh
python tools/dialogue/train_policy.py --zero /path/to/pinned-zero \
  --reference models/dialogue-syntax/model.ccv2 \
  --tokenizer assets/language/tokenizer.json --output /tmp/policy-run --steps 4000
python tools/dialogue/build_syntax_probe.py --model /tmp/policy-run/last.ccv2 \
  --build /path/to/native-libraries --output /tmp/policy-probe
python tools/dialogue/verify_semantic_training.py --run /tmp/policy-run \
  --probe /tmp/policy-probe/probe
python tools/dialogue/run_policy.py --snapshot /path/to/two-people.json \
  --model /tmp/policy-run/last.ccv2 --probe /tmp/policy-probe/probe \
  --goal trade --output /tmp/conversation.json
```

Use `--language goblin --language-probe /path/to/core_account_probe` for Hra'khor.
Each model call receives one private view. Public acts pass to the other person.
The runner validates the chosen act before speaking. The existing v1 model and
codec stay available beside the v2 runner.
