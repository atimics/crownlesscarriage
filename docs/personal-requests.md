# Personal requests

People ask for items based on their lives and the town around them:

- A hungry person asks for bread.
- A baker or miller asks for wheat when local stock is low.
- A smith asks for iron; a cartwright asks for wood.
- A worker asks for a named belonging left in another town's stores.
- A worker with a worn tool names a smith. The smith asks for iron to repair it.

The generator checks living people, jobs, item condition, local stocks, services,
and open roads. The world seed gives workers different belongings and conditions.
Each named tool comes from an existing town tool stock. Each reward comes from
the person's purse and stays reserved until the request ends.

## Playing

Speak to a person and choose "What do you need?" to begin. Later requests follow
the changing world. Choose a person's item request to enter it in the Company Book's
Requests page. A repair also records the smith's request for iron.

Collect an item from town stores through Requests. Borrow a worn tool from its
owner, bring it and one iron to the smith, then return it. The four-item satchel
holds named belongings. Requests also lets you leave a carried item in town stores.
Food and work supplies use the carriage's existing cargo.

The book shows the recorded source, reward, and the next action. The action's
hint explains what to bring or where to meet. Giving the item transfers it,
pays the reserved reward, and changes the person's trust and memory.

In the text client, `wants` shows requests and local actions. `want NUMBER`
chooses an action from that list.

## World changes and saves

Requests settle when the town supplies the need. They close when the recipient
dies. A repair closes if its smith dies before repairing the item; a later
request can name another smith. Completed requests stay in the book for at least
28 days. A person waits at least 14 days before making another request.

Schema 121 saves request IDs, dates, causes, links, rewards, and named belongings.
Custody keeps each item's owner, holder, condition, and transfer revision. The
portable save encoding and state hash cover every request and item field.
Commands carry a revision so an old action can be refreshed from the current
world. The same command and offer rules serve conversations, the book, the text
client, replay, and shared play.

World setup keeps its historical identity. The first direct question creates
personal requests from the loaded town stocks and living people. This choice
is saved. The fixed Deep Wyrm campaign keeps its recorded starting identity.

The current bounds are 32 request records, 16 named belongings, and four carried
belongings. Supply requests can keep appearing as people and stocks change.
