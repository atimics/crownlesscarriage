"""Run the unattended-simulation census and write it with names instead of numbers."""
import argparse
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / 'src' / 'sim' / 'cc_sim.h'


def enum_names(name, header=HEADER):
    """Member names of `typedef enum ... { ... } name;` in declaration order."""
    text = header.read_text()
    body = re.search(r'\{([^{}]*)\}\s*' + name + r'\s*;', text)
    if body is None:
        raise ValueError('enum not found: ' + name)
    members = []
    for part in re.sub(r'/\*.*?\*/', '', body.group(1), flags=re.S).split(','):
        part = re.sub(r'//.*', '', part).strip().split('=')[0].strip()
        if part and not part.endswith('_COUNT'):
            members.append(part)
    return members


def census(binary, seeds, days):
    raw = json.loads(subprocess.run([str(binary), str(seeds), str(days)], capture_output=True, text=True,
                                    check=True).stdout)
    kinds = enum_names('CcEventKind')
    out = {'worlds': raw['worlds'], 'days': raw['days'], 'per_world': raw['per_world'],
           'events_per_world': {kinds[int(k)].removeprefix('CC_EVENT_'): v for k, v in raw['events'].items()},
           'silent_event_kinds': [k.removeprefix('CC_EVENT_') for i, k in enumerate(kinds)
                                  if str(i) not in raw['events'] and k != 'CC_EVENT_KIND_COUNT']}
    for key, enum, prefix in (('role', 'CcCharacterRole', 'CC_CHARACTER_'), ('occupation', 'CcCharacterOccupation', 'CC_OCCUPATION_'),
                              ('goal', 'CcCharacterGoal', 'CC_CHARACTER_GOAL_'), ('activity', 'CcCharacterActivity', 'CC_CHARACTER_ACTIVITY_')):
        names = enum_names(enum)
        out[key + '_per_world'] = {names[i].removeprefix(prefix): v for i, v in enumerate(raw[key]) if i < len(names)}
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True, help='crownless_sim_census')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seeds', type=int, default=60)
    parser.add_argument('--days', type=int, default=730)
    args = parser.parse_args()
    result = census(args.binary, args.seeds, args.days)
    args.output.write_text(json.dumps(result, indent=1, sort_keys=False) + '\n')
    print(f"{result['worlds']} worlds x {result['days']} days: {sum(result['events_per_world'].values()):.0f} events per world, "
          f"{len(result['silent_event_kinds'])} event kinds never fired")


if __name__ == '__main__':
    main()
