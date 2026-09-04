# Crownless interaction and UX redesign

Status: design for review. Date: 4 September 2026. Parent: [#216](https://github.com/atimics/crownlesscarriage/issues/216).

## The experience

Crownless should feel like walking through an illustrated adventure. A person turns toward you. A shop has a clear doorway. A carriage waits beside the road. You point at what interests you, approach it, and act. The world shows the result. The book helps you remember it.

The user's King's Quest reference sets the direction: readable scenes, curious exploration, people with names, useful objects, and short exchanges. The storybook carriage carries that experience between places. These are Crownless design choices.

This pass provides a source audit, a working interface sketch, and an ordered work plan. The sketch uses sample dialogue, prices, and journey timing. Game behavior will be delivered through the linked implementation issues. The current game remains the baseline for comparison.

## Try the interaction sketch

Open [the prototype](interaction-ux-prototype.html) in a browser. Click Ilyra, listen, review the offer, and accept. Enter the shop, approach the keeper, and buy eight boxes. Leave through the door, choose the carriage, and depart. Try Let time pass, read the road stop, and continue. At Silverwick, enter the shop and deliver. The Company Book recalls these sample actions. The Menu provides two visual alternatives: focus assistance and reduced motion.

The sketch demonstrates those choices with simplified scene shapes and timed movement. Path planning, save files, combat, all six settlement services, and the full settings page are covered by the implementation plan below. Reload the sketch to restart its sample journey.

## Evidence and limits

Source reviewed: [`c1799c6`](https://github.com/atimics/crownlesscarriage/tree/c1799c6c4452a2468e6032f362da77453c0141cb). Native play observed at `9ab3a56`, the merged storybook travel build, at a 1200 by 700 game area. The relevant interaction paths were checked again at the reviewed source revision.

The live review covered the bazaar, movement, the top command bar, and the quest offer. The preceding investigation reproduced the talk failure through the production input handler. Other findings below come from source inspection. The camera and readability findings are design assessments. Fresh-player observation and full journey testing are acceptance work.

| Finding | Evidence | Player effect | Owner |
| --- | --- | --- | --- |
| People and doors share the ground-click fallback. | `CcLocalWorldTargetKind` has only None and Carriage; `CcLocalAgentPickWorldTarget` tests the carriage. A person click moved the hero in the preceding live review. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/local3d/actor_simulation.inc#L3058) | A click on a person reads as a request to walk on their ground. | #220 |
| Talk availability and execution use different rules. | The tray checks witness range; the handler also checks the course alarm. Controlled input test: alarm off opens Ilyra; alarm on leaves the same enabled action idle with an empty message. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L8937) | A visible Talk action can silently stall. The live alarm flag itself was not inspected. | #282, #221 |
| Town pressure can change during ordinary exploration. | The course starts with a 24-second alarm timer; its update raises the alarm. Nearby combat is a separate test. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/local3d/actor_simulation.inc#L2010) | The world can change action rules before a player understands the situation. | #292 |
| Entrances use several separate points and ranges. | Town entry uses `LOCAL_MARKET` at 1.30 units; exit uses `INTERIOR_EXIT` at 1.25. Ordinary street actions omit market entry; the open-world branch adds it near a town. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L8968) | The building, visible door, and place that accepts input can feel unrelated. | #217 |
| Action layout changes with the nearby action count. | `ContextActionBounds` splits more than six actions across two rows and changes column width with count. The set is built independently for draw, hit test, and pointer blocking. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L4426) | A moving witness or changing stock can move the control under the pointer. | #219, #221 |
| Conversation depends on a selected situation. | `VIEW_CHARACTER` resolves both a character and situation; reply rules and reply keys are separately encoded. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L4066) | The named cast needs a broader way to respond to ordinary conversation. | #275–#279 |
| Speech and replies occupy separate layers. | The conversation panel adds speaker metadata and speech above a separate tray. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L5614) | Attention moves between the person, their panel, and the reply buttons. | #218, #222 |
| Buying and selling share one control. | Left click buys one unit; right click sells one. Number keys buy; Shift plus a number sells. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L4588) | The transaction direction and total deserve a visible choice. | #290 |
| An offer can read like an accepted promise. | The live quest page showed “Buy 8 Bread” while Accept quest required a visit to the board. `SituationNextAction` derives delivery instructions before checking accepted state. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L629) | The player can spend cargo money before understanding the commitment. | #258, #291 |
| Results have a short, narrow display path. | Ordinary messages show for 2.2 seconds, use a 48-character display limit, and appear only in the local view outside travel. Message age resets on changed text. Repeating the same result can inherit its previous age. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L10835) | A reply or repeated action can produce feedback the player misses. | #291 |
| The book has limited recall and inconsistent names. | Ledger shows four recent events. Quest, promise, cargo delivery, and ordinary sale have overlapping labels; every negative trade is described as “Cargo delivered.” [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L5742) | A player needs a clear record of purchases, commitments, and completed deliveries. | #291, #184 |
| Controls and text need a shared layout policy. | Main calls `CcOverlayBegin(1.0f)`. Small text calls are clamped to nine logical pixels. Several screens use fixed 1280-style coordinates; the carriage panel extends to x=1256 while normal startup width is 1200. Preferences contain reduced motion, toggled by F4. [Source](https://github.com/atimics/crownlesscarriage/blob/c1799c6c4452a2468e6032f362da77453c0141cb/src/client/main.c#L5447) | Reading, resizing, and learning controls require a dedicated pass. Actual glyph size needs capture measurement. | #258, #293 |

## One interaction contract

Every useful world target follows the same path:

1. **Notice.** Its shape, placement, lighting, and behavior make its purpose readable.
2. **Focus.** Hover or keyboard selection shows its name and main verb in the action area. A small shape cue marks the object itself. Text inside the scene belongs to speech or physical signs.
3. **Approach.** A click, tap, or F chooses the main verb. The hero walks to a clear place beside the target. The action area says “Walking to Ilyra” and offers Cancel.
4. **Act.** Reaching the target runs the chosen ordinary action once. Talk, enter, read, and open the carriage can complete here. A purchase, promise, departure, theft, or attack waits for its visible choice.
5. **Show the result.** The person speaks, the door opens, or the transaction changes the displayed totals. A short receipt records a lasting change.
6. **Return.** Closing the exchange restores the scene, camera, and useful target.

A click carries a stable target ID and verb. A moving person keeps that identity. A small pointer movement can preview another object while the committed approach continues. Clicking a new object replaces the pending approach. Clicking clear ground or pressing a movement key cancels it. Escape cancels the approach first.

Use one frame's target plan for rendering and input. Recheck the action against current world state before commit. A changed rule produces a visible reason and leaves the choice with the player.

### Target and input rules

| Situation | Pointer or touch | Keyboard | Visible feedback |
| --- | --- | --- | --- |
| Clear ground | Click or tap to walk | Movement keys | Destination mark and clear path |
| Useful object | Click or tap for its main verb | Cycle nearby targets; F acts | Name, verb, target cue |
| Several overlapping objects | Select the visible front target; alternate action opens a short target list | Cycle that same list | Each candidate has a name and cue |
| Moving person | Follow a nearby reachable approach point | Same behavior | Named pending action |
| Blocked path | Keep focus and show the obstruction | Same behavior | “The crates block this path. Try the lane on the left.” when that lane is known |
| Door | Approach threshold, enter, keep a clear return point | Same behavior | Service name, entry transition, visible exit |
| Conversation | Select a reply | Arrows select; Enter confirms; displayed number selects that reply | Speaker line and reply result |
| Buy, sell, deliver | Separate named action with quantity and total | Same controls in a stable focus order | Preview, then receipt |
| Dangerous choice | Choose its named verb in the action area | Same choice | Target, known cost, and known consequence |
| Reading or settings | Back closes the top layer | Escape closes the top layer | Restored focus and shown clock state |

The final key map should resolve the existing Tab-to-ledger binding before target cycling ships. Suggested default: Tab cycles world targets; J opens the Company Book; F acts; Escape steps back. Keep input remapping and visible key hints in the same contract. Touch uses generous hit areas and the visible alternate-action button.

### Hard cases

- Use the last presented camera and its viewport for picking. During a room blend, the hit shape follows the rendered object.
- Choose visible targets before ground. Use scene depth and visible portions for partially covered people. A wall covering the full target removes it from ordinary pointer selection. Nearby keyboard candidates apply the same reach and visibility rules.
- Use a stable tie order for crowded targets. A focused target gets a short hold margin near range and screen boundaries; source identity still decides when it disappears.
- Replan moving targets at a bounded rate. End an approach with a reason if the target leaves the scene, the path stays blocked, or the scene changes. Set concrete distance, timeout, and hold values through movement tests and playtests.
- A held input and a double click each produce one intended action. Starting a panel consumes the opening input before its first reply can be selected.
- Ordinary exploration, nearby danger, combat, and reading each have one declared input owner. A threat cancels the pending social action and shows who or what caused the change.
- Scene exits keep source place, door ID, approach pose, and return direction. Road sites also keep route ID and progress.

## Screen and story direction

The world is the main surface. Use a quiet top strip for place, time, purse, and access to the Company Book and menu. Keep the current objective and the focused action outside the scene. Reserve one compact region for replies, trading, or road choices. Each mode has one clear primary action.

The Company Book has four sections: **Promises**, **People**, **Cargo**, and **Journal**. Maps and learned road notes are available from the book. Preparing a departure belongs at the carriage. Place-dependent actions explain the needed person or place and offer an approach when the target is known and reachable.

### Storyboard A: the first town

The arrival shot shows the carriage, a clear lane, and a shop sign. The first human need comes from a visible person or board. Focus says “Ilyra Senn · Talk.” One click brings the hero beside her. Both people face each other. Her first line explains who needs help. The reply shows the promise and its terms. Accepting updates Promises and a single next step.

The fresh campaign and resumed campaign get separate checks. The live review used a resumed town state; the opening handoff from Mara needs its own test.

### Storyboard B: a shop

The entrance belongs to the authored building. Its sign, open threshold, keeper, and stock suggest its service. Click the door to enter. A matching exit remains visible inside. Click the keeper or counter to trade. The trade area shows Buy, Sell, and any valid Deliver action. Choose an item and quantity. Show unit price, total cost, purse after purchase, and cargo after purchase. Commit once and show the actual result beside the totals.

Each settlement profile supplies its own building, door, service, and return pose. The first slice covers the bazaar; the completion pass covers all six service profiles.

### Storyboard C: a conversation

The active speaker stays visible. Their current line is the scene's speech caption. Name, reply choices, and Leave sit together outside the scene. Reading waits for the player. Listen leads to another line. A promise remains visible long enough to read its response.

Every named character has a stable conversation identity under #275. Local conversation and distant letters share that person's thread. The six reply kinds in #279 use plain, specific text for the current situation. Relevant choices appear first; unavailable choices carry a useful reason where they help explain an option. Message delivery, source, and confidence remain visible in People when mail work lands.

### Storyboard D: the carriage and road

The carriage opens cargo, company, maps, and departure choices. A departure card names the destination, known travel time, supplies, and reported danger. The hero boards; the wheels begin to move; the view joins the storybook road. Normal travel keeps the caravan readable as it crests a hill. **Let time pass** eases the camera out and advances time. **Normal time** returns to the close view. A warning, rest stop, encounter, site choice, or arrival brings attention back to a clear choice.

Reading road facts is separate from committing departure. Fast-forward stops at the first event that needs a decision. Source and learning day distinguish a report from a witnessed fact. The map shows learned knowledge and a useful route; physical road geometry remains derived from the saved journey.

### Storyboard E: trouble

A visible threat and a short cue explain a change to danger. The focused person responds to the immediate situation. The action area offers a fitting response such as parley, withdraw, or combat. Combat gets an explicit target cue and its own controls. Ending danger returns the player to the interrupted place with a clear next action. Opening help and reading a choice have a declared clock rule.

### Storyboard F: completion and return

At Silverwick, Deliver names the accepted promise and its remaining quantity. A receipt shows goods handed over, reward, and promise progress. Jory's response describes the result. The journal links the receipt to the person and place. A later return visit shows the changed town through #269. Promises keeps completed and failed work readable alongside the current commitment.

## Time, menus, and save flow

Proposed solo rule: opening the book, a conversation, a trade choice, or the menu pauses local action and campaign advancement. Cosmetic motion can continue if it respects reduced motion. Closing a panel resumes the previous pace; a decision stop resumes at normal time after an explicit Continue. Save and loading feedback use their own persistent status.

Escape follows a fixed order: cancel pending approach; close the top panel; open the pause menu. The pause menu contains Resume, Save, Load, Settings, and New campaign. New campaign shows the existing campaign and an explicit archive choice. Raw time-jump keys move to a visible rest or wait choice; testing shortcuts belong in a declared developer mode.

The menu exposes text size, caption size, reduced motion, input hints, input bindings, and optional focus assistance. Show settings before starting a campaign. The user can return to them during play.

PR #273 is active work on a shared carriage. In shared play the host owns the company clock and accepted actions. A local book or conversation shows the shared clock state; host pause uses the host command. Keep this distinction in the mode contract and integration tests before combining the changes.

## Technical shape

Preserve `CcSim` and the journal as the source of lasting game state. Reuse authored places, the current path planner, camera framing, and travel transitions. Extract the interaction plan from `main.c` in stages through #221.

| Part | Owns | Output |
| --- | --- | --- |
| Scene target provider | Stable IDs, hit shapes, reachable approach points, service and return anchors | Bounded candidates for the active scene |
| Interaction planner | Hover, selected target, pending verb, approach phase, availability, reason, priority | One immutable plan for the frame |
| Mode controller | Explore, approach, conversation, trade, book, road choice, travel, combat, pause | Input owner, return context, clock policy |
| Presenter | Target cue, cursor, focused action, replies, costs, receipts, layout | Visible controls from the plan |
| Command adapter | Target and verb validation, authoritative command, accepted result | Receipt or reason tied to one request |
| Session layer | Scene, camera, place, road progress, safe resume point | Valid restored state |

Use a target key composed of scene/place identity, object kind, and stable object ID. Use a request serial to consume each local input once. Keep reply keys stable when their order or words change. Shared play integrates the host's command receipt and revision from PR #273.

Save safe local position and committed campaign actions. Resume a pending approach as idle focus after checking the target again. Resume an unfinished purchase as a fresh quote. Reopening a conversation reads the saved thread and preserves completed replies. Tests prove that loading a screen never repeats a purchase, promise, or delivery.

Feedback has a structured result ID, action, target, outcome, quantities, costs, and reason. Display timing follows that ID. The journal records lasting outcomes through existing event and command records. A local blocked path remains a local message. Keep ordinary sales and promise deliveries distinct in both wording and result data.

## Readability and scene work

- Keep the current painted world palette and pixel art scene. Draw text and controls at display resolution.
- Use sentence case for instructions and replies. Keep the decorative face for short titles; test a clear body face with the actual smallest supported window.
- Set an initial project target of at least 16 visible pixels of body height for essential desktop text at the current minimum window. Provide larger settings and measure real glyphs in saved captures. This target is a design proposal to test.
- Reflow the action area and book with the window. Measure text before laying out controls. Long names and costs wrap within their regions.
- Check 1040×620, 1200×700, 1280×720, and 1920×1080; check normal and larger text; include browser zoom and display scaling.
- Show focus through shape and text as well as color. Give touch targets enough space to select individually. Keep every main action reachable through keyboard navigation.
- Frame the hero, target, and approach lane together. Test pillars, foreground roofs, moving people, and room transitions. Apply foreground fading or camera adjustment through explicit scene rules that match picking.
- Keep physical signs and spoken captions visually distinct. Optional focus assistance can reveal useful targets through temporary shape cues.

Microsoft's [text display guidance](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/101) supports readable defaults, player text options, and measuring rendered glyphs. Its [UI navigation guidance](https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/112) supports clear focus and consistent navigation. These inform the acceptance work; the pixel targets and screen structure above are Crownless proposals.

## Delivery order and ownership

Extend #216 as the parent for this full UX pass. Keep the existing issues as work owners and add the missing flows to that parent. New work: [#290 trade](https://github.com/atimics/crownlesscarriage/issues/290), [#291 results and promises](https://github.com/atimics/crownlesscarriage/issues/291), [#292 modes and time](https://github.com/atimics/crownlesscarriage/issues/292), and [#293 settings and layout](https://github.com/atimics/crownlesscarriage/issues/293).

| Order | Deliverable | Existing owners | Exit check |
| --- | --- | --- | --- |
| 1 | Shared availability and one frame plan | #282, #221 | Enabled Talk reaches its handler for click and F in the same conditions |
| 2 | One complete town interaction slice | #220, #219, #217 | Click person, board, door, keeper, exit, carriage; approach and act with cancellation |
| 3 | Clear scene, dialogue, and first objective | #218, #222, #258 | Both people stay visible; replies update in place; offer and accepted promise read clearly |
| 4 | Explicit trade and remembered results | #290; #291 | Buy, sell, and deliver show correct totals and distinct receipts |
| 5 | Book, pause, clock, and settings | #292; #293; #184 | Back restores focus; clock follows declared mode; large text fits |
| 6 | Road and site continuity | #132, #259, #268 | Normal travel, fast-forward stop, site entry, return, and arrival work in one play session |
| 7 | Cast and full scene coverage | #275–#281, #183, #216 | Named cast, six services, roads, sites, and shared-play integration meet the same contract |

Build each slice through a small PR with captures and a real input check. A broad rewrite of `main.c` should follow proven extracted boundaries. Coordinate the adapter and save behavior with open PRs #273 and #253. Scene readability should consume the accepted art from #272 when it lands.

## Acceptance matrix

| Test | Required result |
| --- | --- |
| Enabled Talk, alarm off/on, nearby threat off/on | One shared decision; a reply opens or a visible reason explains the restriction |
| Person, board, door, keeper, exit, carriage | Pointer and keyboard select the same object and verb; opening input is consumed |
| Person walks away; door closes; scene changes mid-approach | Stable identity, bounded replanning, visible cancellation reason, zero stale actions |
| Ground click, movement key, Escape, second target | Pending action cancels or changes exactly as the input table states |
| Two targets overlap; person behind pillar; camera blends | Visible target wins; covered targets and hit shapes follow the scene contract |
| Listen, pledge, leave, return | Correct saved character, reply, promise state, camera, and local pose |
| Buy eight; sell one; partial delivery; final delivery | Exact purse, stock, cargo, and promise changes; distinct readable receipts |
| Quote changes; cargo fills; repeated click; save write fails | Fresh validated result; at most one commit; useful reason; retry preserves state |
| Offer, accepted promise, partial load, completion, failure | Correct state label and next action, linked person/place, readable history |
| Book from conversation, trade, road choice; Escape back | Correct return focus and declared clock state for each layer |
| Normal travel; fast-forward; first warning; site return | Saved route and progress match presentation; decision stop remains available |
| Save/resume during each safe state | Committed results persist; pending screens revalidate; actions execute once |
| All supported window sizes; larger text; keyboard; touch | Readable text, complete controls, stable focus, correct hit regions |

Existing coverage includes movement and camera tests, viewport tests, policy range tests, travel/session tests, persistence tests, and browser save checks. Extend those where they own the behavior. Add planner tests in #221 and a real native and browser input journey for the complete slice. Capture reels help review presentation; acceptance must also drive the actual input path.

Observe at least three fresh players after the first full slice. Ask them to find the speaker, accept the job, buy the right amount, leave town, handle a road choice, deliver, and explain the result. Record first-click success, help needed, repeated inputs, missed feedback, remembered names, and recovery after returning to play. Treat these as observations; tune numeric targets after that baseline. #268 owns the longer mill adventure playtest.


## Validation of this design PR

Browser checks exercised the prototype's talk, board acceptance, shop entry, purchase, exit, departure, fast-forward stop, arrival, delivery, and journal recall. The sample delivery finished with 42 crowns and zero cargo after buying eight boxes for 16 crowns and earning the 18-crown reward. A double click on Buy committed one trade. A separate sale returned two crowns for one box. F opened Talk, Enter chose a reply or trade, and Escape returned to the scene. Book return and focus assistance were also checked.

The prototype was inspected at 1024, 736, 360, and 320 pixels wide. Narrow layouts fit the document width. Browser inspection reported zero script errors. These are prototype checks; the acceptance matrix above defines the game implementation checks.
