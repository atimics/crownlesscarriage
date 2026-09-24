# Scrivendays and zodiac signs

The Company Book now has a Calendar page. A dragon's first change into a Deep
Wyrm starts an age in the simulation. Scribes learn about that change through
journeys, dated field notes, and the books that reach their annual meeting.

## The sky calendar

A solar year lasts 364 days: thirteen signs of 28 days, with four seasons of
91 days. The signs are Lantern, Hare, Hart, Broken Crown, Hammer, Cup, Sheaf,
Quill, Scales, Gate, Bell, Ash Tree, and Wyrm.

The Wanderer completes a circuit in thirteen years. Its sign at the start of
the solar year supplies the year name, such as “Year of the Lantern.” The town
sky draws thirteen fixed star patterns and the Wanderer from the same calendar
used for observations. A known person's birth date supplies their birth sign.

Day 0 is Lantern 1. Existing world time begins on day 1, so a fresh game starts
on Lantern 2. Existing birthdays, dragon age thresholds, and promised deadlines
retain their absolute dates and existing duration rules. The world clock starts
at dawn; its last eight hours are night. Spending a watch advances eight hours.

## Scrivendays

Scrivendays fall on Quill 1–7 each year. A host is chosen 56 days before the
opening. Eligible towns need people, security, food, paper, tools, and a safe
route for at least two scribes. The town that hosted longest ago takes priority;
travel burden breaks ties. Notices take time to reach other towns.

Each delegate is a named living scribe. The town assigns a specific tome and
four wheat. Delegates use actual open roads and respect war borders. They pay
for extra food from their own travel purse. When the route and time permit,
they visit the lair before the meeting. They then travel to the host and home.
The archive keeps its last working keeper at home.

A failed host ends that year's gathering. Closed routes and food shortages can
leave a delegate waiting. A carrier's death leaves the tome at their last town.
Each journey retains the person's ID, book ID, owner, location, food, route,
arrival date, and return destination.

## The books on the table

Books retain their passages from the day they were bound. Each has space for a
Crowned Dragon sighting, a Deep Wyrm sighting, a goblin porter tally, and a sky
observation. Every field note names its author, place, day, and source book.
Copies preserve those source IDs. Rebinding keeps selected saved passages and
selected field notes. Carried books remain reserved during archive work and a
town's destruction of debt records.

The company can commission a tome, borrow a town's tome, read its pages, make a
copy, write field notes, and return a loan. A loan retains the town's ownership
and names the exact book to return. A book occupies one cargo space. Commissioning
and copying each cost two crowns, one paper, one wheat, and a day of work; the
town also needs tools. Loans last one solar year.

At a hearing, scribes compare books physically present. They choose a pair of
independent source accounts: a dated Crowned Dragon sighting and a later dated
Deep Wyrm sighting. A copy carries the original account's provenance. Conflicting
sightings leave the date disputed. Scribes from at least two schools must attend;
the first release uses the three kingdoms as schools.

The agreed date is the midpoint of the supported interval. The almanac preserves
the interval, the proposed day, two literal citations, school IDs, signers, and
the agreement day. An exact one-day interval yields that day. Wider intervals
remain visible beside the adopted date. The private transition date is held in
a separate record; the hearing reads the brought evidence.

Goblin porter counts supply dated local context. The first dating calculation
uses the before-and-after dragon sightings. Goblin genealogies, inscriptions,
and further ways to narrow the interval are later research work.

Each written almanac consumes paper. A delegate's edition travels home in their
tome. Towns adopt the edition they receive. The company can learn an edition by
reading it, carry it in an actual book, and deliver it to another town. Older
published editions remain available for comparison.

## Playing

Open **Book → Calendar**. Browse local tomes or commission a field tome. The
book view provides pages, borrowing, copying, returns, and lair observations.
The dates page provides sky reading, hearing attendance, almanac delivery, and
an eight-hour watch. The signs page lists the thirteen signs.

The text client has `calendar`, `tomes`, and `scribe` actions. These use the same
saved command as the graphical client and shared-world host.

## Storage and bounds

Save schema 113 adds explicit portable storage for this feature. Older saves
receive frozen pages at migration time. Existing schema 112 road-withdrawal
rules remain intact. The save and hash use the same field order.

The first release supports the existing 24 physical treasure slots, six
simultaneous delegates, four field notes per book, 32 age anchors, and 32
published almanacs. Loans retain their due dates and return obligations. Further
loan sanctions, commissioned escort contracts, reserve hosts, and a wider range
of historical evidence are later additions.

Code: [calendar](../../src/sim/cc_calendar.c),
[scribe rules](../../src/sim/cc_scriven.c),
[Company Book](../../src/client/cc_scriven_book.inc),
[save codec](../../src/sim/cc_scriven_codec.c).

The [simulation report](../reviews/scrivendays-2026-09-24/README.md) records the
observed results and the next balance question.
