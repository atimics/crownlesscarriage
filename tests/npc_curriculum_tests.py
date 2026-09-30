"""Keep the NPC curriculum honest: every command, event and structure it names must exist."""
import json
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'dialogue'))
import render_curriculum
import sim_census

CURRICULUM = json.loads((ROOT / 'tools' / 'dialogue' / 'curriculum.json').read_text())
CENSUS = json.loads((ROOT / 'docs' / 'npc-census.json').read_text())
HEADER = (ROOT / 'src' / 'sim' / 'cc_sim.h').read_text()
REQUIRED = ('id', 'name', 'status', 'actors', 'decision', 'observation', 'options', 'outcome', 'interface',
            'commands', 'events', 'structs', 'size_hint', 'dependencies', 'first_experiment', 'gate', 'risks', 'evidence')


class CurriculumTests(unittest.TestCase):
    def test_each_stage_is_complete_and_ids_are_unique(self):
        ids = [s['id'] for s in CURRICULUM['stages']]
        self.assertEqual(len(ids), len(set(ids)))
        for s in CURRICULUM['stages']:
            for key in REQUIRED:
                self.assertIn(key, s, f"{s['id']} lacks {key}")
                self.assertTrue(s[key] or key in ('commands', 'dependencies'), f"{s['id']} has empty {key}")
            self.assertIn(s['interface']['state'], ('ready', 'partial', 'missing'))

    def test_dependencies_point_to_earlier_stages(self):
        seen = set()
        for s in CURRICULUM['stages']:
            self.assertTrue(set(s['dependencies']) <= seen, f"{s['id']} depends on a later stage")
            seen.add(s['id'])

    def test_named_commands_events_and_structures_exist_in_the_simulation(self):
        commands = set(re.findall(r'\bCC_COMMAND_[A-Z0-9_]+\b', HEADER))
        events = {e.removeprefix('CC_EVENT_') for e in sim_census.enum_names('CcEventKind')}
        for s in CURRICULUM['stages']:
            for c in s['commands']:
                self.assertIn(c, commands, f"{s['id']}: {c}")
            for e in s['events']:
                self.assertIn(e, events, f"{s['id']}: {e}")
            for t in s['structs']:
                self.assertRegex(HEADER, r'\}\s*' + t + r'\s*;|typedef struct ' + t + r'\b', f"{s['id']}: {t}")

    def test_every_named_event_is_in_the_census(self):
        known = set(CENSUS['events_per_world']) | set(CENSUS['silent_event_kinds'])
        for s in CURRICULUM['stages']:
            for e in s['events']:
                self.assertIn(e, known, f"{s['id']}: {e} missing from the census")

    def test_the_stage_order_follows_how_often_situations_arise(self):
        # Stages that need injected scenarios (war, dragons) come after the ones that run unattended.
        rate = lambda s: sum(CENSUS['events_per_world'].get(e, 0) for e in s['events'])
        by_id = {s['id']: s for s in CURRICULUM['stages']}
        self.assertGreater(rate(by_id['L3']), rate(by_id['L6']))
        self.assertGreater(rate(by_id['L4']), rate(by_id['L6']))
        self.assertIn('WAR_DECLARED', CENSUS['silent_event_kinds'])

    def test_the_markdown_is_generated_from_the_data(self):
        self.assertEqual((ROOT / 'docs' / 'npc-curriculum.md').read_text(),
                         render_curriculum.render(CURRICULUM, CENSUS))


if __name__ == '__main__':
    unittest.main()
