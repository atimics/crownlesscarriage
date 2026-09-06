# Dialog catalog

Every line a character, company, or pony can say, and every player reply, as
of the board-directory build. Sources: `src/story/cc_story.c` (authored story
lines and generated quest speech), `src/story/cc_speech_interactions.c`
(greetings, gossip, road demands, trade), `src/sim/cc_ponies.c` (ponies),
`src/sim/cc_sim.c` (gossip mutations), `src/client/main.c` (player voice),
`src/metagame/cc_metagame.c` (playtest narrator).

## The named cast — quest conversations

`%s` marks a world-derived name, `%d` a number, `%c` crowns.

### Mara Venn — relief delivery sponsor

| When | Line |
| --- | --- |
| Offer | "Another town is running out of food." |
| Heard | "They need food. I have the boxes ready." |
| Promised | "The food boxes are loaded. Take my carriage." |
| Helped | "The food arrived. You kept your word." |
| Pressing | "The flour ledger lost another page this week. The bread line did not." |
| Breaking | "There is seed grain left. If they eat it, spring becomes the next famine." |

With the granary knowledge unlocked, her lines generate instead:
"%s is running out of food." / "%d boxes of food. I will load them." /
"All %d food boxes are aboard. %s will meet you in %s." /
"%s received all %d food boxes." / "The food did not reach %s."

As monster-expedition sponsor she also says: "I believe Bren. Take me to the
wall. We need to find Cera first." and "I will pay you to find Cera. Do not
open the old wall until she is safe."

### Tomas Rill — night-road delivery sponsor

| When | Line |
| --- | --- |
| Offer | "Take eight food boxes to the miners by the old road. No soldiers. No inspections. Better pay." |
| Heard | "Leave after dark. Cover the lamps when you pass the tax patrol." |
| Promised | "No passengers. No stops. My sister will meet you under the fox lanterns." |
| Helped | "The flour arrived. My sister says you drive like a thief. She meant it kindly." |

### Ilyra Senn — route repair sponsor

| When | Line |
| --- | --- |
| Offer | "The bridge works. I was ordered to keep the gate closed." |
| Heard | "Alderwatch is hungry. If I open the gate, I answer for every food box that crosses." |
| Promised | "Bring two crates each of tools, wood, and stone. My crew will call it bridge repair." |
| Helped | "I cut the gate chain myself. My report says the bridge machinery failed." |
| Pressing | "The chain still holds. The wagons behind it have started turning back." |
| Breaking | "Open the bridge now, or the road will belong to whoever feeds it at night." |

### Jory Fen — the mine thread

| When | Line |
| --- | --- |
| Lead | "Bren ran out of the west gallery and left his lamp behind. He will not tell me why." |
| Decision (old friends) | "Tell Mara what Bren heard. She knows the mine, and she will listen." |
| Decision (former partners) | "Tell Mara what Bren heard. Do not tell her it came from me." |
| Decision (rivals) | "Do not tell Mara yet. She will close the mine before we find Cera." |
| Offer | "Help me find Cera. We can deal with the old wall after she is safe." |
| Promised | "Stay close to me. Keep your lamp up and watch the roof." |
| Helped | "Cera came up with the morning shift. She won't work the west gallery again." |
| Pressing | "The breathing is closer to the lifts now. Even the foreman heard it." |
| Breaking | "Nobody whistles below now. Something learned to whistle back." |
| Relief, as affected | "We are running out of food." / "We need the whole delivery." / "Bring the food here. People are waiting." / "The food arrived. Thank you." |

### Bren Alder — the witness

"Witness: I heard someone using a pick behind the old wall. Cera was still down there, so I ran."

## Generated quest speech — any sponsor or affected person

| Beat | Sponsor | Affected |
| --- | --- | --- |
| Relief offer | "Another town needs food. I can pay for the delivery." | "We are running out of food." |
| Repair offer | "The road crew can open the gate. They need tools, wood, and stone, or coin for the guards." | "We used to get two carts a week on that road. We have had none since it closed." |
| Monster offer | "The workers will not pass the bricked-up tunnel. Clear the lower road and they can work again." | "We heard movement behind the bricked-up tunnel. The foreman sent us home and locked the gate." |
| Night-road offer | "The tax patrol is turning away hungry families. I need you to take food past the guards." | "We cannot use the town store. The tax collector waits there with two guards." |
| Courier offer | "This letter is for the king's court. If anyone opens it, the court can refuse it." | "I should have reached the king's court yesterday. My horse cannot cross another mountain." |
| Heard | "That is the whole of it. Will you take the work?" | "That is what happened. I do not know what else to tell you." |
| Promised | "All right. I will put your company name on the job papers." | "Thank you. I will tell the others you are coming." |
| Helped | "I received the report this morning. You did what you promised." | "You came back. Thank you." |
| Withdrew | "I put your name on the job. Now I have to find another carrier." | "We waited until dark. No one came." |
| Resolved elsewhere | "Another crew finished it this morning. The offer is closed." | "They settled it without us. I do not know yet if that is good news." |
| Failed | "The deadline passed. I cannot hold the offer open." | "It is too late for that plan now. We have to manage what is left." |
| Pressing | "The need is growing faster than the answer. There is still time, but less of it." | "It is worse than when we last spoke. People have started making quieter plans." |
| Breaking | "This is the last honest chance to choose the outcome. After this, the pressure chooses." | "We are past waiting calmly. Whatever you do now will be what we remember." |

Mine fallbacks when no authored line fits: affected lead — "%s fled the west
gallery. Help me learn why."; witness — "I heard stone moving behind the old
wall. Tell %s."; affected at decision — "The mine official must hear this. I
trust your choice."; sponsor — "Take tools into the west gallery and bring our
people home."

## Player replies in conversation

| Quest | Ask | Promise |
| --- | --- | --- |
| Relief delivery | "1  What do they need?" | "2  I'll take the job." |
| Route repair | "1  Why is the bridge closed?" | "2  I will open the road." |
| Monster expedition | "1  What did the miners hear?" | "2  I will go below." |
| Night road | "1  Who gets the food?" | "2  I will make the delivery." |
| Courier | "1  Who needs the letter?" | "2  I will carry the letter." |
| Any | "1  Tell Mara." / "2  Keep it between us." (the mine decision) | |
| Leave | "Esc  Not now." | |

## Neighbours — greetings, gossip, and trade

Greetings by speaker and town state:
- Guard: "Welcome to %s. Keep the gate clear for the wagons."
- Traveller: "I am stopping in %s for a meal. How was the road?"
- Trader: "Come into the %s. We can settle a price at the counter."
- Hungry town: "Food is short here in %s. Ask at the %s if you can help."
- Otherwise: "Welcome to %s. The %s has work and supplies."
- Fallback before any speech resolves: "Welcome. I was passing the %s. How was your journey?"

Gossip drawn by the Chat verb:
- The account: "I heard this: %s"
- Who told them: "%s told me. The account concerns %s, on day %d." or, when the
  teller is the speaker: "People were talking about %s. The account is from day %d."

Trade counter speech: "For %d %s, the price is %c crowns." /
"I can pay %c crowns for %d %s." /
"You have brought %d %s for the delivery. Let us settle it."

## Road companies — the blockade demand

- The Unpaid Company: "The crown owes us six months' pay. Until it pays, travellers pay us."
- The Tallow Knives: "Food or coin. We have no use for speeches."
- The Broken Pennants: "We guarded this road until the crown dismissed us. We still collect."
- The Ditch Parliament: "We counted your guards. We counted your wheels. Now we're counting your coin."
- Any other company: "Pay for the road or turn back."
- Demand form: "%s Give us %d %s or %c crowns, and you may pass." or "%s Pay %c crowns to pass."
- After paying: "We have what we asked for. Move along."

## Gossip mutation — retellings

Word swaps as stories pass between towns (hop counts): one→two→three→five,
four→six, five→seven, western/eastern→northern/southern directions,
workers→merchants, raiders→deserters, guards→soldiers. Old campaigns instead
summarise: "Word from %s: %s." with claims like "war is spreading", "the courts
have made peace", "a great dragon has fallen", "a dragon commands the realm",
"dragons are multiplying", "a remarkable treasure has appeared", "raids are
spreading", "raiders have struck", "a familiar face has died". Suffixes:
" Loyal voices credit the crown." (court bias +15), " Some blame the court."
(bias −15), " They fear worse is coming." (alarm ≥ 30).

## The player's own voice

- Ambush: "Raiders! Stay close!"
- Fighting: "Back!"
- Arrival: "We have reached %s."

## Rainbow ponies

Names: Ember, Marmalade, Dandelion, Clover, Puddle, Ink, Velvet. Personalities:
Brave and impulsive; Friendly and always hungry; Cheerfully distracted; Gentle
and stubborn; Playful rain-lover; Quiet and curious; Dramatic racer.

Quest requests (one per pony, by quest kind):
- Ember: "Ember is feeding a stranded patrol." / "Ember wants a brave red banner." / "Ember is fixing a broken warning bell."
- Marmalade: "Marmalade found a hungry roadside baker." / "Marmalade wants a picnic blanket." / "Marmalade broke the baker's oven handle."
- Dandelion: "Dandelion forgot the picnic basket." / "Dandelion is making trail ribbons." / "Dandelion wants to repair a crooked sign."
- Clover: "Clover is feeding tired travellers." / "Clover needs blankets for a shelter." / "Clover is repairing the shelter gate."
- Puddle: "Puddle is sharing lunch with a ferryman." / "Puddle needs towels after a swim." / "Puddle is helping mend a ferry rope."
- Ink: "Ink packed books instead of lunch." / "Ink wants a soft wrap for an old book." / "Ink found a locked roadside chest."
- Velvet: "Velvet is hosting a victory picnic." / "Velvet demands a splendid race ribbon." / "Velvet is repairing the starting gate."

Each appends "Bring %d %s." When helped: "Thank you. I would love to travel
with you. Who will head out on a new adventure?"

## Playtest narrator

"Tell me what happened, from the beginning." — the metagame's fallback when a
speaker has no authored line. The debrief sheet asks: "Tell the story of what
happened. Which choice felt hardest, and why? What surprised you later? Who do
you now trust or distrust? What would you do next?"