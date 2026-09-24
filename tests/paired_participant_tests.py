#!/usr/bin/env python3
"""Participant isolation, actor turns, saved memory, and actual sim snapshots."""
import json
from pathlib import Path
import subprocess
import sqlite3
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/dialogue'))
from paired_participants import Memory, run, validate_turn

PROBE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None


def snapshot():
    people = []
    for own, other, secret in [('1', '2', 'private apple'), ('2', '1', 'private plum')]:
        people.append({'self': {'id': own, 'name': own, 'in_transit': False},
                       'listener': {'id': other, 'name': other}, 'day': 5,
                       'place': {'id': '7'}, 'knowledge': [secret],
                       'available_actions': ['end_conversation']})
    return {'version': 1, 'world_seed': 12, 'day': 5, 'state_hash': 'abc', 'participants': people}


class ProtocolTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.worker = self.root / 'worker.py'
        # This is a transport fixture. Its outputs are confined to the test directory.
        self.worker.write_text('''import json,sys
for line in sys.stdin:
 r=json.loads(line)
 me=r['participant']['self']['id']
 secret='private plum' if me=='1' else 'private apple'
 assert secret not in json.dumps(r)
 assert r['participant']['listener']['id'] != me
 if sys.argv[-1]=='end': out={'kind':'action','action':'end_conversation'}
 elif sys.argv[-1]=='broken': out={'kind':'dialogue','turns':['A','B']}
 else: out={'kind':'speech','text':'I am '+me+'. Heard '+str(len(r['observed_turns']))+' turns.'}
 print(json.dumps(out),flush=True)
''')
        self.command = [sys.executable, str(self.worker)]

    def tearDown(self):
        self.tmp.cleanup()

    def test_pair_keeps_private_views_and_own_targets(self):
        dest = self.root / 'pair'
        result = run(snapshot(), [self.command, self.command], dest, turns=4)
        self.assertEqual(result['completed_turns'], 4)
        rows = [json.loads(l) for l in (dest / 'training-candidates.jsonl').read_text().splitlines()]
        self.assertEqual([r['speaker_id'] for r in rows], ['1', '2', '1', '2'])
        self.assertEqual(rows[1]['input']['observed_turns'][0]['text'], rows[0]['output']['text'])
        self.assertEqual(rows[2]['output']['text'], 'I am 1. Heard 2 turns.')
        self.assertEqual(rows[2]['input']['participant']['self']['id'], '1')

    def test_action_ends_episode(self):
        dest = self.root / 'end'
        result = run(snapshot(), [self.command + ['end'], self.command], dest)
        self.assertEqual(result['status'], 'ended_by_participant')
        self.assertEqual(result['completed_turns'], 1)

    def test_failed_turn_and_prior_success_are_retained(self):
        dest = self.root / 'failed'
        with self.assertRaises(ValueError):
            run(snapshot(), [self.command, self.command + ['broken']], dest)
        rows = [json.loads(l) for l in (dest / 'turns.jsonl').read_text().splitlines()]
        self.assertEqual([r['status'] for r in rows], ['accepted', 'failed'])
        self.assertEqual(rows[1]['output']['kind'], 'dialogue')
        self.assertEqual(json.loads((dest / 'receipt.json').read_text())['status'], 'failed')

    def test_memory_survives_worker_and_episode_restart(self):
        db = self.root / 'memory.sqlite'
        run(snapshot(), [self.command, self.command], self.root / 'first', turns=2,
            memory_path=db, world='world-A', episode='one')
        run(snapshot(), [self.command, self.command], self.root / 'next', turns=1,
            memory_path=db, world='world-A', episode='two')
        turn = json.loads((self.root / 'next/turn-000.json').read_text())
        self.assertEqual(len(turn['input']['remembered_observations']), 2)
        memory = Memory(db)
        try:
            self.assertEqual(memory.read('world-B', '1', 5), [])
            self.assertEqual(memory.read('world-A', '3', 5), [])
            with self.assertRaises(ValueError):
                memory.read('world-A', '1', 4)
        finally:
            memory.close()

    def test_invalid_actions_and_multi_turn_shapes(self):
        p = snapshot()['participants'][0]
        for output in ({'kind': 'action', 'action': 'give_all_coins'},
                       {'kind': 'speech', 'text': 'one\ntwo'},
                       {'kind': 'speech', 'text': '', 'speaker': '2'},
                       [{'kind': 'speech', 'text': 'hi'}]):
            with self.assertRaises(ValueError):
                validate_turn(output, p)

    def test_reject_separated_people_before_starting_worker(self):
        s = snapshot()
        s['participants'][1]['place']['id'] = '8'
        with self.assertRaises(ValueError):
            run(s, [self.command, self.command], self.root / 'separate')
        self.assertFalse((self.root / 'separate').exists())

    def test_timeout_preserves_partial_output(self):
        slow = self.root / 'slow.py'
        slow.write_text("""import sys,time
sys.stdin.readline()
print('{"kind":',end="",flush=True)
time.sleep(10)
""")
        dest = self.root / 'timeout'
        with self.assertRaises(TimeoutError):
            run(snapshot(), [[sys.executable, str(slow)], self.command], dest, timeout=0.15)
        turn = json.loads((dest / 'turn-000.json').read_text())
        self.assertEqual(turn['status'], 'failed')
        self.assertEqual(turn['raw_output'], '{"kind":')

    def test_duplicate_episode_preserves_original_memory(self):
        db = self.root / 'memory.sqlite'
        run(snapshot(), [self.command, self.command], self.root / 'original', turns=1,
            memory_path=db, world='world-A', episode='same')
        with self.assertRaises(sqlite3.IntegrityError):
            run(snapshot(), [self.command, self.command], self.root / 'duplicate', turns=1,
                memory_path=db, world='world-A', episode='same')
        memory = Memory(db)
        try:
            self.assertEqual(len(memory.read('world-A', '1', 5)), 1)
            self.assertEqual(len(memory.read('world-A', '2', 5)), 1)
        finally:
            memory.close()

    def test_worker_start_failure_has_receipt(self):
        dest = self.root / 'start-failed'
        with self.assertRaises(FileNotFoundError):
            run(snapshot(), [['/missing/worker'], self.command], dest)
        self.assertEqual(json.loads((dest / 'receipt.json').read_text())['status'], 'failed')


@unittest.skipIf(PROBE is None, 'native probe supplied by CTest')
class NativeTests(unittest.TestCase):
    def test_actual_people_and_owned_views(self):
        result = subprocess.check_output([str(PROBE), '--seed', '1202', '--days', '367',
                                          '--first', '1369094286720630904',
                                          '--second', '1369094286720630838'], text=True)
        s = json.loads(result)
        a, b = s['participants']
        self.assertEqual(a['self']['name'], 'Harthild Underwick')
        self.assertEqual(b['self']['name'], 'Jory Fen')
        self.assertEqual(a['day'], 368)
        self.assertNotIn('death_day', result)
        self.assertNotIn('stress', a['listener'])
        self.assertTrue(any(r['kind'] == 118 for r in a['held_accounts']))
        self.assertEqual(subprocess.check_output([str(PROBE), '--seed', '1202', '--days', '367',
                         '--first', a['self']['id'], '--second', b['self']['id']], text=True), result)

    def test_invalid_pair(self):
        result = subprocess.run([str(PROBE), '--first', '1', '--second', '1'], capture_output=True)
        self.assertEqual(result.returncode, 2)


if __name__ == '__main__':
    unittest.main()
