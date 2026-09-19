# Concrete private knowledge and Luna teachers

A participant's knowledge previously exported IDs, dates and certainty. The
snapshot now also resolves the directly referenced event's text and date.
The event must exist and be dated at or before the recorded learning date. The learning date
must be at or before the current day. Missing details remain explicit nulls.
The person's original source, certainty and privacy flag stay attached.

The compact v2 input gives resolved personal knowledge priority over general
held accounts. Its fixed array keeps the kind, learned date, certainty, privacy,
source role or ID, recorded source name, event date and complete account text.
Opaque subject and event IDs remain in the source snapshot. Whole records that
exceed the budget appear in the omission list.

Two fresh independent `gpt-5.6-luna` sessions played Jory and Bren, as requested.
Each session received only its own selected packet and the speech it heard.
Their source is world 1202, day 1. Jory holds a told warning that Bren fled the
west gallery. Bren holds a witnessed account of a sealed tunnel opening and
disturbing the Pale Ore-Eaters. Jory learns those details through Bren's reply.

Four original turns, full and selected snapshots, compact inputs, reviews and
hashes are in `luna-mine-teachers.json.gz`. All four pass root-agent source and
compact review. Each compact input retains the person's complete event account.
The dialogue develops toward warning workers and looking for safer work. Those
are proposals; the collector stops after four speech turns with the world frozen.
The provider's exact Luna revision is unavailable.

Validation covers source and certainty retention, private knowledge separation,
read-only simulation access, missing references and time boundaries. Existing
native participant and corpus review tests also pass. This changes participant
input; the shipped model checkpoint remains the current one.
