"""Render docs/npc-curriculum.md from tools/dialogue/curriculum.json and the census.

The JSON is the source of truth; the markdown is generated so the two cannot drift
(tests/npc_curriculum_tests.py compares them).
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CURRICULUM = Path(__file__).with_name('curriculum.json')
CENSUS = ROOT / 'docs' / 'npc-census.json'
OUTPUT = ROOT / 'docs' / 'npc-curriculum.md'


def rate(census, event):
    value = census['events_per_world'].get(event)
    return f'{event} {value:.1f}' if value is not None else f'{event} (never fires unattended)'


def render(curriculum, census):
    out = ['# NPC training curriculum', '',
           'Generated from `tools/dialogue/curriculum.json` and `docs/npc-census.json` by '
           '`tools/dialogue/render_curriculum.py`. Edit the JSON, not this file.', '',
           f"Census: {census['worlds']} unattended worlds of {census['days']} days each "
           f"(`tools/sim_census.c`, `tools/dialogue/sim_census.py`). Per world: "
           f"{census['per_world']['people']:.0f} people, {census['per_world']['settlements']:.0f} settlements, "
           f"{census['per_world']['kingdoms']:.0f} kingdoms, {sum(census['events_per_world'].values()):.0f} events; "
           f"{len(census['silent_event_kinds'])} of the event kinds never fire without a player.", '',
           '## Principles', '']
    out += [f'- {p}' for p in curriculum['principles']]
    if curriculum.get('findings'):
        out += ['', '## What the runs found', ''] + [f'- {x}' for x in curriculum['findings']]
    out += ['', '## Stages', '', '| Stage | Name | Status | Verdict | Interface | Depends on | Model |', '| --- | --- | --- | --- | --- | --- | --- |']
    for s in curriculum['stages']:
        out.append(f"| {s['id']} | {s['name']} | {s['status']} | {s.get('verdict', '-')} | {s['interface']['state']} | "
                   f"{', '.join(s['dependencies']) or '-'} | {s['size_hint'].split(';')[0]} |")
    for s in curriculum['stages']:
        out += ['', f"## {s['id']}. {s['name']}", '',
                f"**Status:** {s['status']}{'; verdict: ' + s['verdict'] if s.get('verdict') else ''}. **Actors:** {', '.join(s['actors'])}.", '',
                f"**Decision.** {s['decision']}", '', '**Sees:**']
        out += [f'- {x}' for x in s['observation']]
        out += ['', '**Chooses among:**'] + [f'- {x}' for x in s['options']]
        out += ['', '**Judged by:**'] + [f'- {x}' for x in s['outcome']]
        out += ['', f"**Interface ({s['interface']['state']}).** {s['interface']['note']}", '']
        if s['events']:
            out += ['**Unattended, per world:** ' + '; '.join(rate(census, e) for e in s['events']) + '.', '']
        if s['commands']:
            out += ['**Commands:** ' + ', '.join(f'`{c}`' for c in s['commands']) + '.', '']
        out += [f"**Structures:** {', '.join(f'`{x}`' for x in s['structs'])}.", '',
                f"**Model size.** {s['size_hint']}.", '',
                f"**First experiment.** {s['first_experiment']}", '',
                f"**Gate to advance.** {s['gate']}", '', '**Risks:**']
        out += [f'- {x}' for x in s['risks']]
        out += ['', '**Evidence:** ' + '; '.join(s['evidence']) + '.']
    out += ['', '## Shared tracks', '', '| Track | Status | Goal | Have | Need |', '| --- | --- | --- | --- | --- |']
    for t in curriculum['tracks']:
        out.append(f"| {t['id']} {t['name']} | {t['status']} | {t['goal']} | {'; '.join(t['have'])} | {'; '.join(t['need'])} |")
    return '\n'.join(out) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='fail if the checked-in markdown is stale')
    args = parser.parse_args()
    text = render(json.loads(CURRICULUM.read_text()), json.loads(CENSUS.read_text()))
    if args.check:
        if OUTPUT.read_text() != text:
            raise SystemExit('docs/npc-curriculum.md is stale; run render_curriculum.py')
        return
    OUTPUT.write_text(text)
    print(f'wrote {OUTPUT.relative_to(ROOT)} ({len(text.splitlines())} lines)')


if __name__ == '__main__':
    main()
