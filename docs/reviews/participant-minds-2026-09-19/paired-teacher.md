# Two participant teachers

The user selected two independent subagents of the parent model. Each started
with an empty conversation and received one person's private state. The same
session handled that person's later turns. The collector passed speech between
them and stopped after six turns. World 1202 remained at day 368.

Harthild is a hungry quarryman hiding in Gloamgate. He has 117 coins.
Hartha is a working smith with zero coins. Each holds the dragon attack account.
Each learns the other person's private needs through the exchange.

**Harthild:** Hartha, I have gone hungry for three days, and hiding here wears me down. I have heard that Varkesh burns Gloamgate over seventeen missing crowns. I want to find food and a safe place where my quarry work can earn a living. Have you heard of a way out?

**Hartha:** I have heard the same account about Varkesh and the seventeen crowns. My home is Silverwick. We could ask which road is safe and where you can get food today. I have zero coins, and I need paid smith work myself. Perhaps we can seek work near each other.

**Harthild:** Silverwick is my home too. I have 117 coins. I could pay for a meal for us both if we find food for sale. Let us ask about the road to Silverwick and paid work there. I would feel easier travelling with someone who also needs a steady living.

**Hartha:** Thank you, Harthild. I would gladly share a meal with you. Your hunger makes finding food our first need. Then we can ask about the road home and work for a quarry worker and a smith. I would feel easier travelling together too.

**Harthild:** Then let us look for someone selling food. I will keep enough coins for the road. Once we have eaten, we can ask who knows the way to Silverwick and whether anyone there needs a quarryman or a smith. Having a plan with you eases my mind.

**Hartha:** That plan suits me. Let us ask the price before we buy, so you can keep enough for the journey. A food seller may also know who has come from Silverwick lately. We could ask that traveller about the road and work.

## Agent review

All six outputs pass the existing speech protocol validator. The first three
turns establish needs, propose cooperation and offer payment. The fourth and
fifth repeat the agreed plan; their training eligibility is held for contribution
review. The sixth adds price checking and a possible source of road information.
Hartha's exact phrase "zero coins" also deserves a naturalness review.

The factual claims follow the private packets or received speech. Food, travel
and work remain proposals. The next scene should let a person choose an allowed
action once they have agreed on a plan. That requires actor commands and a new
simulator observation before further speech.

This is one reviewed-by-agent teacher example. Human review remains pending.
The shipped 5M checkpoint is unchanged. Broader collection should sample
different real needs and relationships, including disagreement with a concrete
reason, affection, bargaining, danger and grief supported by personal events.

`paired-teacher.json.gz` preserves all six output objects and normalized
single-person candidate inputs. It records the packet selection, session names,
world hash, instruction summary and collection boundary. These are manually
transcribed subagent outputs. Exact provider model revision and raw provider
request receipts are unavailable. SHA-256:
`1a00386099b38eafb7f9b6b720c62242dc4e37bc2930283c8c95e16af0ea1366`.
