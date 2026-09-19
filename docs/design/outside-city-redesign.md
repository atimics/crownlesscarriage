# One continuous world: town, road, and dungeon

Town, road, and dungeon use the town's visual system with camera angles suited
to each place. The player follows the same company, carriage, people, and goods
through all three. The road is a place where the carriage can be seen, inspected,
stopped, approached, and boarded.

This document is the shared contract for the existing issues below. Its acceptance
list describes the result to prove. The baseline records what source review
established at `52d2e879` on 19 September 2026.

## Existing foundation

- Authored towns provide streets, interiors, services, people, and a carriage panel.
- Normal road travel uses the world route geometry and a side-on camera. It draws
  the shared carriage asset, cargo boxes, and named pony models.
- The mine yard uses a composed exterior camera. Underground uses a first-person
  tile camera. Both use shared lighting and model helpers.
- Low Silver Pit keeps the route and return anchor while the company visits the
  yard and six-room mine. Both doorway transitions use ordinary movement.
- Road stopping, stepping down, reboarding, camping, and continuing past sites
  exist. The mine turn-off remains available throughout its stop window.
- Campaign commands, saves, journal replay, and shared revisions own outcomes.
  The renderer derives the scene from state.

## One location and one distance

The route model must provide a saved segment, direction, physical distance,
destination goal, and any side-road return anchor. The road picture, next choice,
carriage position, wheel rotation, and pony gait read that same position.
Camera framing can compress the view while physical distance stays consistent.

A destination is a goal. The player reaches a junction before choosing a connected
branch. A large advance stops at the first reachable junction, site, barrier, or
other required decision. Turning back begins at the current position and consumes
the time needed to travel home. Visiting a side place preserves the main-road
anchor and direction.

The pilot district uses existing towns and site identities. Broader road coverage
follows the same rules. [#323](https://github.com/atimics/crownlesscarriage/issues/323)
owns saved road movement; [#432](https://github.com/atimics/crownlesscarriage/issues/432)
owns agreement between route distance and the travel views.

## One carriage and one interaction contract

Inspecting the carriage shows the actual load, capacity, team, and condition in
town, on the road, and at the mine yard. Closing that view returns to the same
place and journey. Read-only inspection preserves goods, time, and route state.
Packing and unloading require the appropriate reachable holder.

A visible object supplies an identity, visible hit shape, approach point, reach,
and available action. Ground clicks move. Object clicks preserve the selected
object while approaching it. Boarding requires reaching the carriage at its
current position. Manual input, a changed target, or a changed scene cancels or
rechecks the pending action. Pointer, touch, keyboard, and shared commands follow
the same rules.

[#220](https://github.com/atimics/crownlesscarriage/issues/220) owns the common
interaction contract; [#797](https://github.com/atimics/crownlesscarriage/issues/797)
owns mine sightlines and deliberate use. An opening, wall, or bar has matching
drawn geometry, collision, and hit testing. Automatic mine routes use observed
space. First-person facing stays local; submitted movement names its cardinal
direction and expected revision. Reload restores the saved tile and the documented
entry-facing convention.

## Shared art and readable choices

Keep the town's materials, lighting, people, carriage, and regional motifs across
views. Road scenery reads route and place identity, condition, and saved site
state. Keep the carriage and useful signs clear at bends and stops. The mine uses
this visual language for floor, walls, doors, objects, and occupants.

A doorway may use a camera cut or a short transition. Either keeps the entrance,
exit, company position, and return anchor understandable. Reduced motion uses
cuts. The road, yard, and underground are views of one journey.

[#760](https://github.com/atimics/crownlesscarriage/issues/760) owns exterior
continuity and the manifest-backed cargo cue;
[#148](https://github.com/atimics/crownlesscarriage/issues/148) owns regional road
identity. [#293](https://github.com/atimics/crownlesscarriage/issues/293) owns readable
controls and settings. Reading and action controls use the existing published
action model, with effective text and target sizes checked after scaling.

## Costs, knowledge, and time

Keep current campaign accounting while connecting the views. Mine entry consumes
one packed ration and resets light to 18. Mine steps and use actions keep their
existing costs. The source baseline displays a light counter. Connecting it to the rendered
light radius needs its own visual acceptance check.

Camping is an explicit road decision with current costs. A required choice holds
travel while it is read. Claims from signs, surveys, and people retain their
source and date. The player can return with goods, with information, or after a
retreat. A sale transfers a chosen quantity to a willing, funded buyer.

[#484](https://github.com/atimics/crownlesscarriage/issues/484) owns pack/source/cache
custody; [#763](https://github.com/atimics/crownlesscarriage/issues/763) owns the
haulers and bargain; [#764](https://github.com/atimics/crownlesscarriage/issues/764)
owns the attributed lead and return. Underroad construction in
[#799](https://github.com/atimics/crownlesscarriage/issues/799) can produce later
places that obey the same geometry and interaction rules.

## Connected acceptance

[#762](https://github.com/atimics/crownlesscarriage/issues/762) collects the evidence.
For every check, record the build revision and distinguish fixtures, ordinary
input, browser emulation, and physical-device observation.

- Inspect the carriage in town, leave through its gate, inspect it on the road,
  close the panel, and continue from the same saved position with the same load.
- Stop, walk away, approach the visible carriage, and board it within reach.
- Reach a junction, choose a connected branch, visit a site, and return to its
  saved road anchor. Turn back halfway and cover the actual return distance.
- Enter the mine yard, pack a chosen quantity, walk through the doorway, inspect
  a visible target, carry or cache a partial load, and return to the same carriage.
- Bargain, bypass, and retreat produce distinct custody and knowledge. Report or
  sell the actual return in the familiar town.
- Save/reload at each transition. Compare route, direction, position, identities,
  quantities, learned information, and the next available action. Exercise stale
  and duplicate shared commands and accelerated travel.
- Inspect captures of town departure/return, road in both directions, a bend,
  a junction/stop, the parked carriage, and a mine wall/bar/doorway interaction.
- Check readable text and controls at desktop and phone sizes. Record a physical
  phone walkthrough separately from browser touch emulation.
- Record first actionable screen, input response, transition, and save timing,
  with build, device, browser, renderer, and sample count. Keep deterministic
  graphics-work budgets and calibrate timing limits for each test environment.

## Delivery and continuing checks

The road carriage now has an inspection action and reachable boarding. Its wheel
and gait motion use distance along the sampled route. Mine input selects visible
targets and walks through observed floor. Quantity controls connect the pack,
finite source, carriage, and cache. The haulers offer a saved bargain and a local
combat encounter. Portrait controls put readable actions beside the scene.

The [connected journey record](../reviews/silverwick-first-haul-2026-09-19.md)
lists the ordinary-input observations and the exact builds used. Saved junction
and reversal travel is tracked in #323; attributed information and the town
return are tracked in #764. Each delivery needs its own final merged receipt and
the connected checks above.

Local timing diagnostics record the first actionable screen, input, scene
transitions, and saves. Ctrl/Cmd+Shift+D exports the bounded local record as JSON.
The record includes the build and renderer so timings can be compared within the
same environment. Browser emulation and a physical-phone walkthrough have
separate evidence requirements.
