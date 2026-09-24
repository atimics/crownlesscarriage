import io
import ctypes as c
import hashlib
import json
from pathlib import Path
import re
import sqlite3
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from concurrent.futures import ThreadPoolExecutor
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'coop'))
from engine import Campaign, Engine
from server import ACTIONS, ApiError, Application, Worlds, away_days, AWAY_GRACE, AWAY_RAMP, issue_world_pass

LIBRARY = sys.argv.pop(1)


class CoopTests(unittest.TestCase):
    def test_server_accepts_every_shared_command_name(self):
        header = (Path(__file__).resolve().parents[1] /
                  'src/multiplayer/cc_coop_commands.h').read_text()
        table = re.search(r'const names\[\] = \{(.*?)\};', header, re.S)
        self.assertIsNotNone(table)
        names = set(re.findall(r'"([a-z_]+)"', table.group(1)))
        local_only = {'party_wipe', 'mine_contest', 'mine_resolve_contest',
                      'food_relief_propose', 'food_relief_accept', 'food_relief_execute'}
        server_only = {'stop_travel', 'resume_travel', 'skip_watch'}
        self.assertEqual(ACTIONS, (names - local_only) | server_only)

    @classmethod
    def setUpClass(cls):
        cls.engine = Engine(LIBRARY)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / 'worlds.sqlite3'
        self.worlds = Worlds(self.path, self.engine)
        self.a, self.b, self.id = 'a' * 64, 'b' * 64, '1' * 32
        self.create_world(self.id, seed=0xc0a71a9e)
        self.invite = self.worlds.invite(self.id, self.a)['invite']
        self.worlds.join(self.id, self.b, {'player': 'Bren', 'invite': self.invite})

    def tearDown(self):
        self.worlds.close()
        self.temp.cleanup()

    def create_world(self, world, seed=42):
        return self.worlds.create(self.a, {'id': world, 'name': 'Lantern Road',
            'player': 'Mara', 'seed': seed, 'world_pass': issue_world_pass(self.path)})

    def test_deep_wyrm_shared_start_join_and_restart(self):
        world = 'd' * 32
        body = dict(id=world, name='Deep Wyrm', player='Mara', campaign='deep-wyrm',
                    world_pass=issue_world_pass(self.path))
        first = self.worlds.create(self.a, body)
        self.assertEqual(first['state']['day'], 73366)
        self.assertTrue(first['state']['prophecy']['carried'])
        self.assertTrue(first['state']['prophecy']['can_deliver'])
        self.assertEqual(first['state']['company']['cargo_used'], 1)
        with self.engine.open(campaign='deep-wyrm') as sim:
            self.assertEqual(first['state'], sim.snapshot())
        invitation = self.worlds.invite(world, self.a)['invite']
        joined = self.worlds.join(world, self.b, dict(player='Bren', invite=invitation))
        self.assertEqual(joined['state'], first['state'])
        self.assertEqual(self.worlds.create(self.a, body)['state'], first['state'])
        with self.assertRaises(ApiError):
            self.worlds.create(self.a, dict(body, campaign='new-world'))
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.view(world, self.b)['state'], first['state'])
        view = self.worlds.view(world, self.a)
        command = dict(protocol=1, sequence=view['next_sequence'],
                       action_revision=view['action_revision'], action='deliver_prophecy',
                       target=first['state']['prophecy']['id'])
        result = self.worlds.command(world, self.a, command)
        self.assertTrue(result['accepted'])
        delivered = self.worlds.view(world, self.b)
        self.assertTrue(delivered['state']['prophecy']['delivered'])
        self.assertEqual(delivered['state']['company']['cargo_used'], 0)
        retry = self.worlds.command(world, self.a, command)
        self.assertTrue(retry['duplicate'])
        self.assertEqual(retry['world']['state'], result['world']['state'])
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        view = self.worlds.view(world, self.a)
        self.assertTrue(view['state']['prophecy']['delivered'])
        again = dict(command, sequence=view['next_sequence'], action_revision=view['action_revision'])
        self.assertFalse(self.worlds.command(world, self.a, again)['accepted'])
        self.worlds.owner_action(world, self.a, 'delete')
        self.assertIsNone(self.worlds.db.execute(
            'SELECT campaign FROM world_starts WHERE world=?', (world,)).fetchone())

    def test_starting_campaign_failure_preserves_world_pass(self):
        body = dict(id='d' * 32, name='Deep Wyrm', player='Mara', campaign='unknown',
                    world_pass=issue_world_pass(self.path))
        with self.assertRaises(ApiError):
            self.worlds.create(self.a, body)
        body['campaign'] = 'deep-wyrm'
        with patch.object(self.engine, 'open', side_effect=RuntimeError('Opening unavailable')):
            with self.assertRaises(RuntimeError):
                self.worlds.create(self.a, body)
        self.assertIsNone(self.worlds.db.execute(
            'SELECT id FROM worlds WHERE id=?', (body['id'],)).fetchone())
        self.assertEqual(self.worlds.create(self.a, body)['state']['day'], 73366)

    def command(self, token, action='trade', **values):
        view = self.worlds.view(self.id, token)
        return {'protocol': 1, 'sequence': view['next_sequence'],
                'action_revision': view['action_revision'], 'action': action, **values}

    def stage_shared_mine(self, mode):
        fixture = Path(self.temp.name) / f'mine-{mode}.bin'
        executable = Path(LIBRARY).resolve().parent / 'mine_tests'
        made = subprocess.run([executable, '--write-shared-mine-fixture', mode, fixture],
                              check=True, capture_output=True, text=True)
        revision = int(made.stdout.strip())
        with self.engine.open(saved=fixture.read_bytes()) as sim:
            state, view = sim.save(), sim.snapshot()
        self.worlds.db.execute(
            'UPDATE worlds SET state=?,view=?,revision=revision+1,action_revision=action_revision+1 WHERE id=?',
            (state, json.dumps(view), self.id))
        self.worlds.db.execute('DELETE FROM scene_contexts WHERE world=?', (self.id,))
        return revision

    def shared_mine_goods(self):
        pointer = c.c_void_p
        for name in ('CcMinePackGood', 'CcMineSourceGood'):
            function = getattr(self.engine.lib, name)
            function.argtypes, function.restype = [pointer, c.c_int32], c.c_int32
        saved = self.worlds.db.execute(
            'SELECT state FROM worlds WHERE id=?', (self.id,)).fetchone()['state']
        with self.engine.open(saved=saved) as sim:
            pack = self.engine.lib.CcMinePackGood
            source = self.engine.lib.CcMineSourceGood
            return (pack(sim.handle, 0), pack(sim.handle, 4),
                    source(sim.handle, 0), source(sim.handle, 4))

    def check_cached_view_upgrade(self, travelling=False, paused=False):
        self.worlds.command(self.id, self.a, self.command(self.a, good=0, amount=1))
        if travelling:
            target = self.worlds.view(self.id, self.a)['state']['travel'][0]['id']
            result = self.worlds.command(self.id, self.a, self.command(self.a, 'travel', target=target))
            self.assertTrue(result['accepted'])
        if paused:
            self.worlds.owner_action(self.id, self.a, 'pause')
        before = self.worlds.view(self.id, self.a)
        receipts = [tuple(row) for row in self.worlds.db.execute('SELECT * FROM receipts')]
        stale = dict(before['state'], hash='older-engine-view', travel=[])
        self.worlds.db.execute('UPDATE worlds SET view=? WHERE id=?', (json.dumps(stale), self.id))
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        after = self.worlds.view(self.id, self.a)
        self.assertEqual(after['state'], before['state'])
        self.assertEqual(after['revision'], before['revision'] + 1)
        self.assertEqual(after['action_revision'], before['action_revision'] + 1)
        self.assertEqual(after['next_sequence'], before['next_sequence'])
        self.assertEqual(after['paused'], paused)
        self.assertEqual([tuple(row) for row in self.worlds.db.execute('SELECT * FROM receipts')], receipts)
        saved = self.worlds.db.execute('SELECT state FROM worlds WHERE id=?', (self.id,)).fetchone()[0]
        with self.engine.open(saved=saved) as sim:
            self.assertEqual(sim.snapshot(), after['state'])
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.view(self.id, self.a)['revision'], after['revision'])

    def test_idle_cached_view_refreshes_on_restart(self):
        self.check_cached_view_upgrade()

    def test_paused_cached_view_refreshes_on_restart(self):
        self.check_cached_view_upgrade(paused=True)

    def test_travelling_cached_view_refreshes_on_restart(self):
        self.check_cached_view_upgrade(travelling=True)

    def test_historical_cast_recovery_preserves_backup_players_and_retry(self):
        fixture = Path(__file__).parent / 'fixtures/shipped/schema-73-retired-cast.ccsave'
        historical = fixture.read_bytes()
        fixture_hash = hashlib.sha256(historical).hexdigest()
        with self.engine.open(saved=historical) as sim:
            expected = sim.snapshot()
        self.assertEqual(expected['day'], 12411)
        self.worlds.command(self.id, self.a, self.command(self.a, good=0, amount=1))
        view = self.worlds.appearance(self.id, self.a,
            {'appearance': dict(skin=1, hair=2, style=3, face=1, coat=4)})
        self.worlds.save_session(self.id, self.a, dict(sequence=1,
            context=view['session_context'], session='CROWNLESS_SESSION 7\nrecovery test\n'))
        self.worlds.db.execute('UPDATE worlds SET state=?,view=? WHERE id=?',
                              (historical, json.dumps({'historical': True}), self.id))
        before = dict(self.worlds.db.execute('SELECT * FROM worlds WHERE id=?', (self.id,)).fetchone())
        preserved_tables = ('members', 'receipts', 'appearances', 'sessions',
                            'party_lives', 'party_wipes', 'world_starts')
        def records(db):
            return {table: [tuple(row) for row in db.execute(f'SELECT * FROM {table} ORDER BY rowid')]
                    for table in preserved_tables}
        players = records(self.worlds.db)
        backup = Path(self.temp.name) / 'before-recovery.sqlite3'
        subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] / 'tools/coop/server.py'),
                        '--database', str(self.path), '--backup-to', str(backup)], check=True)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.failed, set())
        self.assertEqual(records(self.worlds.db), players)
        restored = self.worlds.view(self.id, self.b)
        self.assertEqual(restored['state'], expected)
        self.assertEqual(restored['revision'], before['revision'] + 1)
        self.assertEqual(restored['action_revision'], before['action_revision'] + 1)
        self.assertEqual(restored['next_sequence'], 1)
        target = next(route['id'] for route in expected['travel'] if route['available'])
        command = self.command(self.b, 'travel', target=target)
        result = self.worlds.command(self.id, self.b, command)
        self.assertTrue(result['accepted'])
        self.assertTrue(result['world']['state']['journey']['active'])
        self.assertEqual(result['world']['state']['company'], expected['company'])
        saved = self.worlds.db.execute('SELECT state FROM worlds WHERE id=?', (self.id,)).fetchone()[0]
        with self.engine.open(saved=saved) as sim:
            self.assertEqual(sim.snapshot(), result['world']['state'])
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.failed, set())
        self.assertEqual(self.worlds.view(self.id, self.a)['state'], result['world']['state'])
        retry = self.worlds.command(self.id, self.b, command)
        self.assertTrue(retry['duplicate'])
        self.assertEqual(retry['world']['state'], result['world']['state'])
        self.assertEqual(retry['world']['revision'], result['world']['revision'])
        self.assertEqual(self.worlds.db.execute(
            'SELECT count(*) FROM receipts WHERE world=? AND member=? AND sequence=?',
            (self.id, restored['member'], command['sequence'])).fetchone()[0], 1)
        with sqlite3.connect(backup) as db:
            db.row_factory = sqlite3.Row
            self.assertEqual(dict(db.execute('SELECT * FROM worlds WHERE id=?', (self.id,)).fetchone()), before)
            self.assertEqual(records(db), players)
            self.assertEqual(db.execute('PRAGMA integrity_check').fetchone()[0], 'ok')
        self.assertEqual(hashlib.sha256(fixture.read_bytes()).hexdigest(), fixture_hash)

    def enter(self, token):
        state = self.worlds.view(self.id, token, campaign=True, enter=True)
        return dict(visit=state['visit'], context=state['session_context'], scene=0, pose=[0.0] * 83)

    def test_party_wipe_waits_for_last_player_and_advances_once(self):
        a, b = self.enter(self.a), self.enter(self.b)
        before = self.worlds.view(self.id, self.a)
        self.worlds.pose(self.id, self.a, dict(a, dead=True))
        self.assertEqual(self.worlds.view(self.id, self.a)['state']['day'], before['state']['day'])
        # A living player in a different scene is still part of the party.
        self.worlds.pose(self.id, self.b, dict(b, scene=1, dead=False))
        self.worlds.pose(self.id, self.a, dict(a, dead=False))
        self.assertTrue(self.worlds.view(self.id, self.a)['dead'])
        with self.assertRaises(ApiError):
            self.worlds.command(self.id, self.a, self.command(self.a))
        result = self.worlds.pose(self.id, self.b, dict(b, scene=1, dead=True))['world']
        self.assertEqual(result['state']['day'], before['state']['day'] + 20 * 365)
        self.assertEqual(result['party_wipes'], 1)
        self.assertFalse(result['dead'])
        self.assertNotEqual(result['session_context'], before['session_context'])
        for token, pose in ((self.a, a), (self.b, b)):
            with self.assertRaises(ApiError):
                self.worlds.pose(self.id, token, dict(pose, dead=True))
        self.assertEqual(self.worlds.view(self.id, self.a)['party_wipes'], 1)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        restored = self.worlds.view(self.id, self.a)
        self.assertEqual(restored['state'], result['state'])
        self.assertEqual(restored['party_wipes'], 1)
        self.assertEqual(restored['session_context'], result['session_context'])

    def test_simultaneous_deaths_share_one_world_jump(self):
        a, b = self.enter(self.a), self.enter(self.b)
        before = self.worlds.view(self.id, self.a)['state']['day']
        with ThreadPoolExecutor(max_workers=2) as workers:
            first = workers.submit(self.worlds.pose, self.id, self.a, dict(a, dead=True))
            second = workers.submit(self.worlds.pose, self.id, self.b, dict(b, dead=True))
            first.result()
            second.result()
        result = self.worlds.view(self.id, self.a)
        self.assertEqual(result['state']['day'], before + 7300)
        self.assertEqual(result['party_wipes'], 1)

    def test_solo_death_ignores_members_in_the_lobby(self):
        a = self.enter(self.a)
        before = self.worlds.view(self.id, self.a)['state']['day']
        result = self.worlds.pose(self.id, self.a, dict(a, dead=True))['world']
        self.assertEqual(result['state']['day'], before + 7300)
        a['context'] = result['session_context']
        self.worlds.pose(self.id, self.a, dict(a, dead=False))
        self.assertEqual(self.worlds.view(self.id, self.a)['party_wipes'], 1)
        result = self.worlds.pose(self.id, self.a, dict(a, dead=True))['world']
        self.assertEqual(result['state']['day'], before + 14600)
        self.assertEqual(result['party_wipes'], 2)

    def test_party_death_survives_reconnect_and_host_restart(self):
        a, b = self.enter(self.a), self.enter(self.b)
        self.worlds.pose(self.id, self.a, dict(a, dead=True))
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        a, b = self.enter(self.a), self.enter(self.b)
        self.assertTrue(self.worlds.view(self.id, self.a)['dead'])
        self.worlds.pose(self.id, self.a, dict(a, dead=False))
        self.assertEqual(self.worlds.view(self.id, self.a)['party_wipes'], 0)
        result = self.worlds.pose(self.id, self.b, dict(b, dead=True))['world']
        self.assertEqual(result['party_wipes'], 1)

    def test_knockdowns_and_paused_world_wait_for_death_resolution(self):
        a = self.enter(self.a)
        before = self.worlds.view(self.id, self.a)['state']['day']
        self.worlds.pose(self.id, self.a, dict(a, dead=False))
        self.assertEqual(self.worlds.view(self.id, self.a)['state']['day'], before)
        self.worlds.owner_action(self.id, self.a, 'pause')
        self.worlds.pose(self.id, self.a, dict(a, dead=True))
        self.assertEqual(self.worlds.view(self.id, self.a)['state']['day'], before)
        self.worlds.owner_action(self.id, self.a, 'resume')
        result = self.worlds.pose(self.id, self.a, dict(a, dead=True))['world']
        self.assertEqual(result['state']['day'], before + 7300)

    def test_visible_crew_pose_appearance_and_isolation(self):
        a, b = self.enter(self.a), self.enter(self.b)
        before = self.worlds.view(self.id, self.a)['state']
        appearance = dict(skin=4, hair=2, style=3, face=1, coat=5)
        self.worlds.appearance(self.id, self.b, {'appearance': appearance})
        self.assertEqual(self.worlds.pose(self.id, self.a, a)['peers'], [])
        peers = self.worlds.pose(self.id, self.b, b)['peers']
        self.assertEqual([peer['name'] for peer in peers], ['Mara'])
        peers = self.worlds.pose(self.id, self.a, a)['peers']
        self.assertEqual([peer['name'] for peer in peers], ['Bren'])
        self.assertEqual(peers[0]['appearance'], appearance)
        self.assertEqual(set(peers[0]), {'id', 'name', 'appearance', 'pose', 'sequence'})
        b['pose'][0] = 2.5
        self.worlds.pose(self.id, self.b, b)
        self.assertEqual(self.worlds.pose(self.id, self.a, a)['peers'][0]['pose'][0], 2.5)
        self.assertEqual(self.worlds.pose(self.id, self.b, dict(b, scene=1))['peers'], [])
        self.assertEqual(self.worlds.pose(self.id, self.a, a)['peers'], [])
        self.assertEqual(self.worlds.view(self.id, self.a)['state'], before)
        other = '2' * 32
        self.create_world(other)
        other_state = self.worlds.view(other, self.a, campaign=True, enter=True)
        self.assertEqual(self.worlds.pose(other, self.a, dict(a, visit=other_state['visit'], context=other_state['session_context']))['peers'], [])
        with self.assertRaises(ApiError):
            self.worlds.pose(other, self.b, b)

    def test_pose_expiry_reload_leave_and_restart(self):
        a, b = self.enter(self.a), self.enter(self.b)
        with patch('server.time.monotonic', return_value=100):
            self.worlds.pose(self.id, self.b, b)
        with patch('server.time.monotonic', return_value=102):
            self.assertEqual(len(self.worlds.pose(self.id, self.a, a)['peers']), 1)
        with patch('server.time.monotonic', return_value=104):
            self.assertEqual(self.worlds.pose(self.id, self.a, a)['peers'], [])
        fresh = self.enter(self.b)
        with self.assertRaises(ApiError) as old:
            self.worlds.pose(self.id, self.b, dict(b, pose=None))
        self.assertEqual(old.exception.status, 409)
        # A normal campaign read leaves the active game visit intact.
        self.worlds.view(self.id, self.b, campaign=True)
        self.worlds.pose(self.id, self.b, fresh)
        self.assertEqual(len(self.worlds.pose(self.id, self.a, a)['peers']), 1)
        self.worlds.pose(self.id, self.b, dict(fresh, pose=None))
        self.assertEqual(self.worlds.pose(self.id, self.a, a)['peers'], [])
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        with self.assertRaises(ApiError) as missing:
            self.worlds.pose(self.id, self.a, a)
        self.assertEqual(missing.exception.status, 428)
        self.worlds.pose(self.id, self.a, self.enter(self.a))

    def test_pose_validation_context_and_revocation(self):
        a, b = self.enter(self.a), self.enter(self.b)
        for bad in ([0] * 82, [True] * 83, [float('nan')] * 83,
                    [float('inf')] * 83, [2000000] * 83,
                    [0] * 4 + [100] + [0] * 78):
            with self.assertRaises(ApiError):
                self.worlds.pose(self.id, self.a, dict(a, pose=bad))
        for changed in (dict(scene=8), dict(scene=True), dict(context='stale'), dict(name='Imposter')):
            with self.assertRaises(ApiError):
                self.worlds.pose(self.id, self.a, dict(a, **changed))
        self.worlds.pose(self.id, self.b, b)
        member = self.worlds.view(self.id, self.b)['member']
        self.worlds.owner_action(self.id, self.a, 'remove', member)
        self.assertEqual(self.worlds.pose(self.id, self.a, a)['peers'], [])
        with self.assertRaises(ApiError):
            self.worlds.pose(self.id, self.b, b)

    def test_one_company_and_save_round_trip(self):
        a, b = [self.worlds.view(self.id, token) for token in (self.a, self.b)]
        self.assertEqual(a['state'], b['state'])
        self.assertNotEqual(a['member'], b['member'])
        self.assertEqual(a['state']['company']['coins'], 42)
        row = self.worlds.db.execute('SELECT state FROM worlds').fetchone()
        with self.engine.open(saved=row['state']) as sim:
            self.assertEqual(a['state'], sim.snapshot())
        with self.assertRaises(RuntimeError):
            self.engine.open(saved=b'broken save')

    def test_appearance_is_personal_and_survives_restart(self):
        before = self.worlds.view(self.id, self.a)
        appearance = dict(skin=4, hair=2, style=3, face=1, coat=5)
        saved = self.worlds.appearance(self.id, self.a, {'appearance': appearance})
        self.assertEqual(saved['appearance'], appearance)
        self.assertEqual(saved['state'], before['state'])
        self.assertEqual(saved['action_revision'], before['action_revision'])
        self.assertEqual(saved['next_sequence'], before['next_sequence'])
        self.assertEqual(self.worlds.view(self.id, self.b)['appearance'], dict(skin=0, hair=0, style=0, face=0, coat=0))
        for invalid in (dict(appearance, skin=6), dict(appearance, coat=True), {'skin': 1}):
            with self.assertRaises(ApiError):
                self.worlds.appearance(self.id, self.a, {'appearance': invalid})
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.view(self.id, self.a)['appearance'], appearance)

    def test_player_session_order_isolation_and_scene_changes(self):
        before = self.worlds.view(self.id, self.a)
        saved = dict(sequence=3, context=before['session_context'], session='CROWNLESS_SESSION 7\nlaunch test\n')
        self.worlds.save_session(self.id, self.a, saved)
        self.worlds.save_session(self.id, self.a, saved)
        self.assertIsNone(self.worlds.view(self.id, self.b, campaign=True)['session'])
        self.assertEqual(self.worlds.view(self.id, self.a)['state'], before['state'])
        for invalid in (dict(saved, sequence=2), dict(saved, session=saved['session']+'changed'), dict(saved, sequence=True)):
            with self.assertRaises(ApiError):
                self.worlds.save_session(self.id, self.a, invalid)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.view(self.id, self.a, campaign=True)['session'], saved)
        self.worlds.command(self.id, self.a, self.command(self.a, amount=1, good=0))
        self.assertEqual(self.worlds.view(self.id, self.a)['session_context'], saved['context'])
        target = before['state']['travel'][0]['id']
        self.worlds.command(self.id, self.a, self.command(self.a, 'travel', target=target))
        self.assertNotEqual(self.worlds.view(self.id, self.a)['session_context'], saved['context'])
        with self.assertRaises(ApiError):
            self.worlds.save_session(self.id, self.a, dict(saved, sequence=4))
        with self.assertRaises(ApiError):
            self.worlds.save_session(self.id, 'c'*64, dict(saved, sequence=4))

    def test_player_session_versions(self):
        context = self.worlds.view(self.id, self.a)['session_context']
        for version in (7, 8, 9):
            saved = dict(sequence=version, context=context,
                         session=f'CROWNLESS_SESSION {version}\nlaunch test\n')
            self.worlds.save_session(self.id, self.a, saved)
            self.assertEqual(self.worlds.view(self.id, self.a, campaign=True)['session'], saved)
        for version in (6, 10):
            with self.assertRaises(ApiError):
                self.worlds.save_session(self.id, self.a, dict(sequence=10, context=context,
                    session=f'CROWNLESS_SESSION {version}\nlaunch test\n'))

    def test_retry_survives_server_restart(self):
        body = self.command(self.a, amount=1, good=0)
        first = self.worlds.command(self.id, self.a, body)
        self.assertTrue(first['accepted'])
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        repeat = self.worlds.command(self.id, self.a, body)
        self.assertTrue(repeat['duplicate'])
        self.assertEqual(first['world']['state'], repeat['world']['state'])
        self.assertEqual(self.worlds.view(self.id, self.b)['state']['company']['cargo'][0], 1)
        with self.assertRaises(ApiError):
            self.worlds.command(self.id, self.a, dict(body, amount=2))

    def test_simultaneous_shared_purchase_has_one_winner(self):
        bodies = [(token, self.command(token, amount=1, good=0)) for token in (self.a, self.b)]
        def apply(item):
            try:
                return self.worlds.command(self.id, *item)['accepted']
            except ApiError as error:
                self.assertEqual(error.status, 409)
                return False
        with ThreadPoolExecutor(max_workers=2) as pool:
            self.assertEqual(sum(pool.map(apply, bodies)), 1)
        self.assertEqual(self.worlds.view(self.id, self.a)['state']['company']['cargo'][0], 1)

    def test_shared_road_request_return_and_town_facts_survive_restart(self):
        bought = self.worlds.command(self.id, self.a,
            self.command(self.a, 'trade', good=2, amount=1))
        self.assertTrue(bought['accepted'])
        before = self.worlds.view(self.id, self.a)
        origin = before['state']['company']['location']
        route = next(option for option in before['state']['travel']
                     if option['available'])
        bodies = [(token, self.command(token, 'travel', target=route['id']))
                  for token in (self.a, self.b)]

        def depart(item):
            try:
                return item[0], self.worlds.command(self.id, *item)
            except ApiError as error:
                self.assertEqual(error.status, 409)
                return item[0], None

        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(depart, bodies))
        winners = [(token, result) for token, result in results if result is not None]
        self.assertEqual(len(winners), 1)
        winner, first = winners[0]
        self.assertTrue(first['accepted'])
        self.assertTrue(first['world']['state']['journey']['active'])
        self.assertEqual(self.worlds.view(self.id, self.a)['state'],
                         self.worlds.view(self.id, self.b)['state'])
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        retry = self.worlds.command(self.id, winner,
            next(body for token, body in bodies if token == winner))
        self.assertTrue(retry['duplicate'])
        self.assertEqual(retry['world']['state'], first['world']['state'])

        for step in range(50):
            state = self.worlds.view(self.id, self.a)['state']
            journey = state['journey']
            if not journey['active']:
                break
            site = journey['road_site']
            if site:
                action, target = 'pass_road_site', site['id']
            elif journey['phase'] == 4:
                position = state['road_position']
                forward = [leg for leg in position['next_legs']
                           if leg['direction'] == position['direction']]
                self.assertTrue(forward)
                leg = next((leg for leg in forward if leg['kind'] == 1),
                           forward[0])
                action, target = 'road_leg', leg['token']
            elif journey['phase'] == 3:
                action, target = ('break' if journey['stop'] == 1 else 'camp'), 0
            else:
                self.assertEqual(journey['phase'], 1)
                action, target = 'skip_watch', 0
            member = (self.a, self.b)[step % 2]
            result = self.worlds.command(self.id, member,
                self.command(member, action, target=str(target)))
            self.assertTrue(result['accepted'], result['message'])
        arrived = self.worlds.view(self.id, self.b)
        self.assertFalse(arrived['state']['journey']['active'])
        self.assertEqual(arrived['state']['company']['location'], route['id'])
        self.assertEqual(arrived['state']['company']['cargo'][2], 1)
        self.assertEqual(arrived['state']['team']['carriage_location'], route['id'])
        self.assertGreater(arrived['state']['market']['residents'], 0)
        self.assertTrue(arrived['state']['market']['services'])
        self.assertTrue(any(option['id'] == origin
                            for option in arrived['state']['travel']))

        base = self.worlds.db.execute(
            'SELECT last_human FROM away_clocks WHERE world=?',
            (self.id,)).fetchone()[0]
        self.worlds.seen.clear()
        self.worlds.tick(wall_now=base + AWAY_GRACE - 1)
        short = self.worlds.view(self.id, self.a, present=False)
        self.assertEqual(short['state']['day'], arrived['state']['day'])
        self.worlds.tick(wall_now=base + AWAY_GRACE + 3600)
        long = self.worlds.view(self.id, self.a, present=False)
        self.assertGreater(long['state']['day'], short['state']['day'] + 100)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        returned = self.worlds.view(self.id, self.b, campaign=True)
        self.assertEqual(returned['state'], long['state'])
        self.assertEqual(returned['state']['company']['location'], route['id'])
        self.assertEqual(returned['state']['company']['cargo'][2], 1)
        self.assertEqual(returned['state']['team']['carriage_location'], route['id'])
        self.assertEqual(returned['state']['market']['residents'],
                         self.worlds.view(self.id, self.a)['state']['market']['residents'])

    def test_road_and_pony_actions_reach_the_simulation(self):
        for action in ('repair_road_site', 'transfer_road_site', 'clear_road_site', 'camp_road_site', 'pass_road_site', 'meet_pony',
                       'help_pony', 'swap_pony', 'leave_pony'):
            with self.subTest(action=action):
                before = self.worlds.view(self.id, self.a)['state']
                body = self.command(self.a, action, target='1')
                result = self.worlds.command(self.id, self.a, body)
                self.assertFalse(result['accepted'])
                self.assertEqual(result['world']['state'], before)
                self.assertTrue(self.worlds.command(self.id, self.a, body)['duplicate'])

    def test_mine_take_cache_payloads_and_stale_revision_reach_the_host(self):
        before = self.worlds.view(self.id, self.a)
        for action in ('mine_take', 'mine_cache'):
            body = self.command(self.a, action, target='0', good=1, amount=1)
            result = self.worlds.command(self.id, self.a, body)
            self.assertFalse(result['accepted'])
            self.assertIn('mine position', result['message'])
            self.assertEqual(result['world']['state'], before['state'])
        stale = self.command(self.a, 'mine_take', target='0', good=1, amount=1)
        self.assertTrue(self.worlds.command(
            self.id, self.a, self.command(self.a, 'trade', good=0, amount=1))['accepted'])
        stale['sequence'] += 1
        with self.assertRaises(ApiError) as rejected:
            self.worlds.command(self.id, self.a, stale)
        self.assertEqual(rejected.exception.status, 409)
        self.assertIn('company has changed', rejected.exception.message)

    def test_shared_mine_lead_and_report_authority(self):
        for mode, action, amount in (
                ('lead', 'mine_learn_lead', 1),
                ('report', 'mine_report_return', 0)):
            with self.subTest(action=action):
                revision = self.stage_shared_mine(mode)
                body = self.command(self.a, action, target=str(revision), amount=amount)
                result = self.worlds.command(self.id, self.a, body)
                self.assertTrue(result['accepted'])
                repeat = self.worlds.command(self.id, self.a, body)
                self.assertTrue(repeat['duplicate'])
                self.assertEqual(repeat['world']['state'], result['world']['state'])

                stale = self.command(self.a, action, target=str(revision), amount=amount)
                purchase = self.command(self.a, 'trade', good=0, amount=1)
                self.assertTrue(self.worlds.command(self.id, self.a, purchase)['accepted'])
                stale['sequence'] += 1
                with self.assertRaises(ApiError) as rejected:
                    self.worlds.command(self.id, self.a, stale)
                self.assertEqual(rejected.exception.status, 409)
                self.assertIn('company has changed', rejected.exception.message)

    def test_shared_mine_bargain_and_break_contact_reach_the_host(self):
        revision = self.stage_shared_mine('bargain')
        before = self.shared_mine_goods()
        self.assertEqual(before, (2, 0, 0, 3))
        stale = self.command(self.a, 'mine_bargain', target=str(revision))
        moved = self.worlds.command(
            self.id, self.a,
            self.command(self.a, 'mine_step', target=str(revision), amount=3))
        self.assertTrue(moved['accepted'], moved['message'])
        stale['sequence'] += 1
        with self.assertRaises(ApiError) as rejected:
            self.worlds.command(self.id, self.a, stale)
        self.assertEqual(rejected.exception.status, 409)
        self.assertIn('company has changed', rejected.exception.message)
        self.assertEqual(self.shared_mine_goods(), before)
        returned = self.worlds.command(
            self.id, self.a,
            self.command(self.a, 'mine_step', target=str(revision + 1), amount=1))
        self.assertTrue(returned['accepted'], returned['message'])
        bargained = self.worlds.command(
            self.id, self.a, self.command(self.a, 'mine_bargain', target=str(revision + 2)))
        self.assertTrue(bargained['accepted'], bargained['message'])
        self.assertEqual(self.shared_mine_goods(), (0, 1, 2, 2))

        revision = self.stage_shared_mine('contest')
        with self.assertRaises(ApiError):
            self.worlds.command(
                self.id, self.a, self.command(self.a, 'mine_contest', target=str(revision)))
        broken = self.worlds.command(
            self.id, self.a,
            self.command(self.a, 'mine_break_contact', target=str(revision), amount=27))
        self.assertTrue(broken['accepted'], broken['message'])
        repeated = self.worlds.command(
            self.id, self.a,
            self.command(self.a, 'mine_break_contact', target=str(revision + 1), amount=27))
        self.assertFalse(repeated['accepted'])
        self.assertIn('fight', repeated['message'].lower())

    def test_two_players_clear_and_reload_a_road_site(self):
        def apply(token, action, **values):
            body = self.command(token, action, **values)
            result = self.worlds.command(self.id, token, body)
            self.assertTrue(result['accepted'], result['message'])
            return body, result['world']['state']
        apply(self.a, 'trade', good=2, amount=2)
        apply(self.a, 'trade', good=6, amount=2)
        destination = next(t['id'] for t in self.worlds.view(self.id, self.a)['state']['travel'] if t['available'])
        apply(self.a, 'travel', target=destination)
        _, stopped = apply(self.b, 'skip_watch')
        site = stopped['journey']['road_site']
        self.assertFalse(site['accessible'])
        body, opened = apply(self.b, 'clear_road_site', target=site['id'])
        self.assertTrue(opened['journey']['road_site']['accessible'])
        self.assertGreater(opened['journey']['road_site']['condition'], site['condition'])
        condition = opened['journey']['road_site']['condition']
        body, opened = apply(self.b, 'repair_road_site', target=site['id'])
        self.assertEqual(opened['journey']['road_site']['condition'], min(100, condition + 10))
        self.assertEqual(self.worlds.command(self.id, self.b, body)['world']['state'], opened)
        body, opened = apply(self.b, 'transfer_road_site', target=site['id'], good=2, amount=1)
        self.assertEqual(opened['journey']['road_site']['stock'][2], 1)
        self.assertEqual(self.worlds.command(self.id, self.b, body)['world']['state'], opened)
        self.assertEqual(self.worlds.view(self.id, self.a)['state'], opened)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.view(self.id, self.b)['state'], opened)

    def test_two_players_camp_continue_and_resume_the_same_road(self):
        def apply(token, action, target='0'):
            result = self.worlds.command(self.id, token, self.command(token, action, target=target))
            self.assertTrue(result['accepted'],
                            f"{action}: {result['message']} "
                            f"journey={result['world']['state']['journey']} "
                            f"road={result['world']['state']['road_position']}")
            return result['world']['state']
        def choose_onward(token, state):
            site = state['journey']['road_site']
            road = state['road_position']
            if site is not None and len(road['next_legs']) == 1:
                return apply(token, 'road_leg', road['next_legs'][0]['token'])
            if site is not None:
                return apply(token, 'pass_road_site', site['id'])
            choices = [item for item in road['next_legs']
                       if item['direction'] == road['direction'] and
                       item['kind'] != 3]
            leg = choices[0] if choices else road['next_legs'][0]
            return apply(token, 'road_leg', leg['token'])
        destination = next(t['id'] for t in self.worlds.view(self.id, self.a)['state']['travel'] if t['available'])
        apply(self.a, 'travel', destination)
        stopped = apply(self.b, 'skip_watch')
        stale_skip = self.worlds.command(
            self.id, self.a, self.command(self.a, 'skip_watch'))
        self.assertFalse(stale_skip['accepted'])
        self.assertEqual(stale_skip['world']['state'], stopped)
        mill_visits = 0
        for _ in range(200):
            if (not stopped['journey']['active'] or
                    stopped['journey']['stop'] == 2):
                break
            if stopped['journey']['stop'] == 1:
                stopped = apply(self.b, 'press_on')
            elif (stopped['journey']['road_site'] is not None or
                    stopped['journey']['phase'] == 4):
                mill = next((item for item in
                             stopped['road_position']['next_legs']
                             if item['kind'] == 3), None)
                if mill is not None and mill_visits < 8:
                    stopped = apply(self.b, 'road_leg', mill['token'])
                    mill_visits += 1
                else:
                    stopped = choose_onward(self.b, stopped)
            else:
                stopped = apply(self.b, 'skip_watch')
        self.assertTrue(stopped['journey']['active'])
        self.assertGreater(mill_visits, 0)
        self.assertEqual(stopped['journey']['stop'], 2)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.view(self.id, self.b)['state'], stopped)
        apply(self.a, 'stop_travel', stopped['journey']['route'])
        self.assertTrue(self.worlds.view(self.id, self.b)['travel_stopped'])
        body = self.command(self.b, 'camp')
        camp = self.worlds.command(self.id, self.b, body)
        self.assertTrue(camp['accepted'],
                        f"{camp['message']} {camp['world']['state']['journey']}")
        camped = camp['world']['state']
        elapsed = (camped['day'] - stopped['day']) * 1440 + camped['minute'] - stopped['minute']
        self.assertEqual(elapsed, 480)
        self.assertEqual(camped['journey']['progress'], stopped['journey']['progress'])
        self.assertIsNone(camped['journey']['road_site'])
        self.assertEqual(self.worlds.command(self.id, self.b, body)['world']['state'], camped)
        self.assertEqual(self.worlds.view(self.id, self.a)['state'], camped)
        self.assertTrue(self.worlds.view(self.id, self.a)['travel_stopped'])
        apply(self.b, 'resume_travel', camped['journey']['route'])
        self.assertFalse(self.worlds.view(self.id, self.a)['travel_stopped'])
        next_stop = apply(self.a, 'skip_watch')
        self.assertNotEqual(
            (next_stop['road_position']['coordinate'],
             next_stop['road_position']['travelled'],
             next_stop['road_position']['remaining']),
            (camped['road_position']['coordinate'],
             camped['road_position']['travelled'],
             camped['road_position']['remaining']))
        while (next_stop['journey']['phase'] != 4 and
               next_stop['journey']['road_site'] is None):
            if next_stop['journey']['stop'] != 0:
                next_stop = apply(self.a, 'camp')
            else:
                next_stop = apply(self.a, 'skip_watch')
        continued = choose_onward(self.b, next_stop)
        self.assertIsNone(continued['journey']['road_site'])
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.view(self.id, self.a)['state'], continued)
        self.assertEqual(self.worlds.view(self.id, self.b)['state'], continued)
        moving = apply(self.a, 'skip_watch')
        self.assertEqual(moving['road_position']['journey'],
                         continued['road_position']['journey'])
        self.assertNotEqual(
            (moving['road_position']['coordinate'],
             moving['road_position']['travelled'],
             moving['road_position']['remaining']),
            (continued['road_position']['coordinate'],
             continued['road_position']['travelled'],
             continued['road_position']['remaining']))

    def test_failed_command_is_atomic_and_has_a_receipt(self):
        before = self.worlds.view(self.id, self.a)['state']
        body = self.command(self.a, amount=1000000, good=0)
        result = self.worlds.command(self.id, self.a, body)
        self.assertFalse(result['accepted'])
        self.assertEqual(before, result['world']['state'])
        self.assertTrue(self.worlds.command(self.id, self.a, body)['duplicate'])

    def test_database_failure_keeps_company_and_sequence(self):
        before = self.worlds.view(self.id, self.a)
        self.worlds.db.execute("CREATE TRIGGER failed_write BEFORE INSERT ON receipts BEGIN SELECT RAISE(ABORT,'disk failure'); END")
        with self.assertRaises(sqlite3.DatabaseError):
            self.worlds.command(self.id, self.a, self.command(self.a, amount=1, good=0))
        after = self.worlds.view(self.id, self.a)
        self.assertEqual(before['state'], after['state'])
        self.assertEqual(before['next_sequence'], after['next_sequence'])

    def test_shared_clock_pause_and_empty_carriage(self):
        view = self.worlds.view(self.id, self.a)
        destination = view['state']['travel'][0]['id']
        result = self.worlds.command(self.id, self.a, self.command(self.a, 'travel', target=destination))
        self.assertTrue(result['accepted'])
        now = time.monotonic()
        self.worlds.last_tick[self.id] = now - 0.5
        self.worlds.tick(now)
        a = self.worlds.view(self.id, self.a)
        b = self.worlds.view(self.id, self.b)
        self.assertEqual(a['state']['tick'], 30)
        self.assertEqual(a['state'], b['state'])
        self.worlds.owner_action(self.id, self.a, 'pause')
        with self.assertRaises(ApiError):
            self.worlds.command(self.id, self.a, self.command(self.a, 'skip_watch'))
        self.worlds.tick(now + 1)
        self.assertEqual(self.worlds.view(self.id, self.a)['state']['tick'], 30)
        self.worlds.owner_action(self.id, self.a, 'resume')
        self.worlds.seen.clear()
        self.worlds.tick(now + 100)
        self.assertEqual(self.worlds.view(self.id, self.a)['state']['tick'], 30)

    def test_tick_yields_to_two_players_between_three_worlds(self):
        worlds = [self.id, '2' * 32, '3' * 32]
        routes = {}
        for world in worlds[1:]:
            self.create_world(world)
        for world in worlds:
            view = self.worlds.view(world, self.a)
            route = next(option['id'] for option in view['state']['travel']
                         if option['available'])
            result = self.worlds.command(world, self.a, dict(
                protocol=1, sequence=view['next_sequence'],
                action_revision=view['action_revision'], action='travel',
                target=route))
            self.assertTrue(result['accepted'])
            routes[world] = result['world']['state']['journey']['route']

        def ready_tick():
            now = time.monotonic()
            for world in worlds:
                self.worlds.last_tick[world] = now - .5
                member = self.worlds.view(world, self.a)['member']
                self.worlds.seen[(world, member)] = now
            return now

        started = threading.Event()
        advance = Campaign.advance
        def slower_advance(campaign, ticks, scale=1):
            started.set()
            time.sleep(.08)
            return advance(campaign, ticks, scale)

        with patch.object(Campaign, 'advance', slower_advance):
            now = ready_tick()
            with ThreadPoolExecutor(max_workers=2) as workers:
                begun = time.perf_counter()
                ticking = workers.submit(self.worlds.tick, now)
                self.assertTrue(started.wait(3))
                waiting = time.perf_counter()
                other_player = workers.submit(self.worlds.view, self.id, self.b)
                seen = other_player.result(timeout=3)
                wait = time.perf_counter() - waiting
                ticking.result(timeout=3)
                total = time.perf_counter() - begun
            self.assertEqual(seen['id'], self.id)
            self.assertLess(wait, total * .8,
                            'A player request should finish between world ticks')

            started.clear()
            now = ready_tick()
            stop = self.command(self.a, 'stop_travel', target=routes[self.id])
            with ThreadPoolExecutor(max_workers=2) as workers:
                ticking = workers.submit(self.worlds.tick, now)
                self.assertTrue(started.wait(3))
                command = workers.submit(self.worlds.command, self.id, self.a, stop)
                accepted = command.result(timeout=3)
                ticking.result(timeout=3)
            self.assertTrue(accepted['accepted'],
                            (accepted['message'], accepted['world']['state']['journey']['phase']))
            self.assertTrue(self.worlds.command(self.id, self.a, stop)['duplicate'])
            saved = self.worlds.db.execute('SELECT state FROM worlds WHERE id=?',
                                           (self.id,)).fetchone()[0]
            with self.engine.open(saved=saved) as campaign:
                self.assertEqual(campaign.snapshot(),
                                 self.worlds.view(self.id, self.b)['state'])

    def test_away_clock_ramp_and_century_rate(self):
        self.assertEqual(away_days(-1), 0)
        self.assertGreater(away_days(7200)-away_days(3600), away_days(3600))
        self.assertAlmostEqual(away_days(AWAY_RAMP+86400)-away_days(AWAY_RAMP), 36500)

    def test_paused_time_survives_restart_and_resume(self):
        before = self.worlds.owner_action(self.id, self.a, 'pause')
        base = self.worlds.db.execute('SELECT accounted_at FROM away_clocks WHERE world=?', (self.id,)).fetchone()[0]
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        with patch('server.time.time', return_value=base+86400):
            resumed = self.worlds.owner_action(self.id, self.a, 'resume')
            self.worlds.tick(wall_now=base+86400)
            self.assertEqual(resumed['away_clock']['days_pending'], 0)
            self.assertEqual(self.worlds.view(self.id, self.a)['state']['day'], before['state']['day'])

    def test_invitation_and_avatar_panel_preserve_away_speed(self):
        base = self.worlds.db.execute('SELECT last_human FROM away_clocks WHERE world=?', (self.id,)).fetchone()[0]
        with patch('server.time.time', return_value=base+AWAY_GRACE+AWAY_RAMP):
            joined = self.worlds.join(self.id, self.b, {'player':'Bren', 'invite':self.invite})
            self.assertEqual(joined['away_clock']['absent_seconds'], AWAY_RAMP)
            self.assertAlmostEqual(joined['away_clock']['years_per_real_day'], 100)
            self.assertTrue(all(not member['online'] for member in joined['crew']))
            edited = self.worlds.appearance(self.id, self.b, {'appearance':joined['appearance']})
            self.assertEqual(edited['away_clock'], joined['away_clock'])

    def test_away_clock_survives_restart_and_return_stops_acceleration(self):
        before = self.worlds.view(self.id, self.a)
        base = self.worlds.db.execute('SELECT last_human FROM away_clocks WHERE world=?', (self.id,)).fetchone()[0]
        self.worlds.seen.clear()
        future = base + AWAY_GRACE + AWAY_RAMP + 86400
        expected = int(away_days(AWAY_RAMP + 86400))
        with patch('server.time.time', return_value=future):
            self.worlds.tick(wall_now=future)
            partial = self.worlds.view(self.id, self.a, present=False)
            self.assertEqual(partial['state']['day'], before['state']['day'] + 8*365)
            self.assertTrue(partial['catching_up'])
            self.worlds.close()
            self.worlds = Worlds(self.path, self.engine)
            returned = self.worlds.view(self.id, self.a, campaign=True)
            self.assertEqual(returned['away_clock']['days_pending'], expected - 8*365)
            for _ in range(30):
                self.worlds.tick(wall_now=future)
            settled = self.worlds.view(self.id, self.a, campaign=True)
            self.assertFalse(settled['catching_up'])
            self.assertEqual(settled['state']['day'], before['state']['day'] + expected)
            self.assertEqual(settled['state']['company']['location'], before['state']['company']['location'])
            self.assertEqual(settled['session_context'], before['session_context'])
            self.worlds.tick(wall_now=future+1)
            self.assertEqual(self.worlds.view(self.id, self.a)['state']['day'], settled['state']['day'])

    def test_away_clock_batch_failure_keeps_elapsed_time_for_retry(self):
        self.worlds.view(self.id, self.a)
        base = self.worlds.db.execute('SELECT last_human FROM away_clocks WHERE world=?', (self.id,)).fetchone()[0]
        self.worlds.seen.clear()
        self.worlds.db.execute("CREATE TRIGGER failed_world_write BEFORE UPDATE OF state ON worlds BEGIN SELECT RAISE(ABORT,'disk failure'); END")
        with self.assertLogs(level='ERROR'):
            self.worlds.tick(wall_now=base+3600)
        self.assertIn(self.id, self.worlds.failed)
        self.assertEqual(self.worlds.db.execute('SELECT accounted_at FROM away_clocks WHERE world=?', (self.id,)).fetchone()[0], base)
        self.worlds.db.execute('DROP TRIGGER failed_world_write')
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.worlds.tick(wall_now=base+3600)
        self.assertGreater(self.worlds.view(self.id, self.a, present=False)['state']['day'], 1)

    def test_one_stranded_world_keeps_the_host_and_its_neighbours_serving(self):
        other = '2' * 32
        self.create_world(other)
        self.worlds.view(self.id, self.a)
        base = self.worlds.db.execute('SELECT last_human FROM away_clocks WHERE world=?', (self.id,)).fetchone()[0]
        self.worlds.seen.clear()
        self.worlds.db.execute("CREATE TRIGGER failed_world_write BEFORE UPDATE OF state ON worlds BEGIN SELECT RAISE(ABORT,'disk failure'); END")
        with self.assertLogs(level='ERROR'):
            self.worlds.tick(wall_now=base+3600)
        self.worlds.db.execute('DROP TRIGGER failed_world_write')
        self.assertIn(self.id, self.worlds.failed)
        status, health = self.request('/healthz')
        self.assertEqual((status, health['status']), (200, 'ready'))
        self.assertEqual(health['worlds_needing_recovery'], len(self.worlds.failed))
        self.assertEqual(self.request('/api/worlds/' + other + '/state')[0], 200)
        self.assertTrue(self.worlds.view(self.id, self.a)['recovery_required'])

    def test_road_stop_is_authoritative_durable_and_idempotent(self):
        target = self.worlds.view(self.id, self.a)['state']['travel'][0]['id']
        started = self.worlds.command(self.id, self.a, self.command(self.a, 'travel', target=target))
        self.assertTrue(started['accepted'])
        route = started['world']['state']['journey']['route']
        saved = self.worlds.db.execute('SELECT state FROM worlds WHERE id=?', (self.id,)).fetchone()[0]
        stale_resume = self.command(self.b, 'resume_travel', target=route)
        stop = self.command(self.a, 'stop_travel', target=route)
        stopped = self.worlds.command(self.id, self.a, stop)
        self.assertTrue(stopped['accepted'])
        self.assertTrue(stopped['world']['travel_stopped'])
        self.assertEqual(saved, self.worlds.db.execute('SELECT state FROM worlds WHERE id=?', (self.id,)).fetchone()[0])
        with self.assertRaises(ApiError):
            self.worlds.command(self.id, self.b, stale_resume)
        retry = self.worlds.command(self.id, self.a, stop)
        self.assertTrue(retry['duplicate'])
        self.assertEqual(retry['world']['revision'], stopped['world']['revision'])
        now = time.monotonic()
        self.worlds.last_tick[self.id] = now - 10
        self.worlds.tick(now=now)
        still = self.worlds.view(self.id, self.b)
        self.assertTrue(still['travel_stopped'])
        self.assertEqual(still['state'], stopped['world']['state'])
        self.assertEqual(self.worlds.last_tick[self.id], now)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertTrue(self.worlds.view(self.id, self.b)['travel_stopped'])
        wrong = self.worlds.command(self.id, self.b, self.command(self.b, 'resume_travel', target='0'))
        self.assertFalse(wrong['accepted'])
        self.assertTrue(wrong['world']['travel_stopped'])
        resumed = self.worlds.command(self.id, self.b, self.command(self.b, 'resume_travel', target=route))
        self.assertTrue(resumed['accepted'])
        self.assertFalse(resumed['world']['travel_stopped'])
        self.assertEqual(saved, self.worlds.db.execute('SELECT state FROM worlds WHERE id=?', (self.id,)).fetchone()[0])
        now = time.monotonic()
        self.worlds.last_tick[self.id] = now - .1
        self.worlds.tick(now=now)
        self.assertGreater(self.worlds.view(self.id, self.a)['state']['tick'], stopped['world']['state']['tick'])
        # A retry of the earlier Stop acknowledges its receipt, not a second stop.
        self.assertFalse(self.worlds.command(self.id, self.a, stop)['world']['travel_stopped'])
        self.worlds.command(self.id, self.a, self.command(self.a, 'stop_travel', target=route))
        self.worlds.owner_action(self.id, self.a, 'delete')
        self.assertIsNone(self.worlds.db.execute('SELECT * FROM road_holds WHERE world=?', (self.id,)).fetchone())

    def test_road_stop_rejects_unrelated_states(self):
        result = self.worlds.command(self.id, self.a, self.command(self.a, 'stop_travel', target='0'))
        self.assertFalse(result['accepted'])
        self.assertFalse(result['world']['travel_stopped'])
        self.assertEqual(self.worlds.db.execute('SELECT count(*) FROM road_holds').fetchone()[0], 0)

    def test_travel_hold_expires_and_releases(self):
        target = self.worlds.view(self.id, self.a)['state']['travel'][0]['id']
        self.worlds.command(self.id, self.a, self.command(self.a, 'travel', target=target))
        pose = self.enter(self.a)
        from engine import Campaign
        scales = []
        with patch.object(Campaign, 'advance', lambda sim, ticks, scale=1: scales.append(scale)):
            now = time.monotonic()
            with patch('server.time.monotonic', return_value=now):
                self.worlds.pose(self.id, self.a, dict(pose, travel_scale=8))
            self.worlds.last_tick[self.id] = now - 0.1
            self.worlds.tick(now=now)
            self.assertEqual(scales[-1], 8)
            self.worlds.tick(now=now + 0.5)
            self.assertEqual(scales[-1], 1)
            with patch('server.time.monotonic', return_value=now + 0.6):
                self.worlds.pose(self.id, self.a, dict(pose, travel_scale=1))
            self.worlds.tick(now=now + 0.6)
            self.assertEqual(scales[-1], 1)
        for scale in (0, 9, True, 1.5):
            with self.assertRaises(ApiError):
                self.worlds.pose(self.id, self.a, dict(pose, travel_scale=scale))

    def test_travel_tick_rebuilds_scene_context(self):
        target = self.worlds.view(self.id, self.a)['state']['travel'][0]['id']
        result = self.worlds.command(self.id, self.a, self.command(self.a, 'travel', target=target))
        self.assertTrue(result['accepted'])
        pose = self.enter(self.a)
        now = time.monotonic()
        with patch('server.time.monotonic', return_value=now):
            self.worlds.pose(self.id, self.a, dict(pose, travel_scale=8))
        # Rebuild the cache on a tick, as happens when a new watch changes the scene.
        self.worlds.db.execute('DELETE FROM scene_contexts WHERE world=?', (self.id,))
        before = self.worlds.db.execute('SELECT revision FROM worlds WHERE id=?', (self.id,)).fetchone()[0]
        self.worlds.last_tick[self.id] = now - 0.1
        self.worlds.tick(now=now)
        self.assertNotIn(self.id, self.worlds.failed)
        after = self.worlds.view(self.id, self.a)
        self.assertGreater(after['state']['tick'], result['world']['state']['tick'])
        self.assertGreater(after['revision'], before)
        self.assertIsNotNone(self.worlds.db.execute('SELECT context FROM scene_contexts WHERE world=?', (self.id,)).fetchone())

    def test_travel_resume_and_tick_batch_equivalence(self):
        with self.engine.open(0xc0a71a9e) as a:
            target = a.snapshot()['travel'][0]['id']
            self.assertTrue(a.apply('travel', target)[0])
            saved = a.save()
            a.advance(60)
            with self.engine.open(saved=saved) as b:
                b.advance(30)
                b.advance(30)
                self.assertEqual(a.snapshot(), b.snapshot())
                with self.engine.open(saved=b.save()) as c:
                    self.assertEqual(c.snapshot(), a.snapshot())

    def test_world_isolation_revocation_and_invite_rotation(self):
        other = '2' * 32
        self.create_world(other)
        with self.assertRaises(ApiError):
            self.worlds.view(other, self.b)
        self.worlds.invite(self.id, self.a, rotate=True)
        with self.assertRaises(ApiError):
            self.worlds.join(self.id, 'c' * 64, {'player': 'Tomas', 'invite': self.invite})
        self.worlds.owner_action(self.id, self.a, 'remove', self.worlds.view(self.id, self.b)['member'])
        with self.assertRaises(ApiError):
            self.worlds.view(self.id, self.b)

    def test_second_host_is_rejected(self):
        with self.assertRaises(RuntimeError):
            Worlds(self.path, self.engine)

    def test_snapshot_ids_and_small_buffer(self):
        view = self.worlds.view(self.id, self.a)
        self.assertIsInstance(view['state']['company']['id'], str)
        self.assertGreater(int(view['state']['company']['id']), 2**53)
        import ctypes
        with self.engine.open() as sim:
            tiny = ctypes.create_string_buffer(5)
            self.assertFalse(sim.lib.CcCoopSnapshot(sim.handle, tiny, len(tiny)))

    def request(self, path, body=None, token=None, origin='http://localhost:8787', application=None):
        raw = json.dumps(body).encode() if body is not None else b''
        env = {'REQUEST_METHOD': 'POST' if body is not None else 'GET', 'PATH_INFO': path,
               'CONTENT_TYPE': 'application/json', 'CONTENT_LENGTH': str(len(raw)),
               'wsgi.input': io.BytesIO(raw), 'wsgi.url_scheme': 'http',
               'HTTP_HOST': 'localhost:8787', 'HTTP_ORIGIN': origin,
               'HTTP_AUTHORIZATION': 'Bearer ' + (token or self.a)}
        statuses = []
        data = b''.join((application or Application(self.worlds))(env, lambda status, headers: statuses.append(status)))
        return int(statuses[0].split()[0]), json.loads(data)

    def test_creation_permission_survives_session_changes_and_restart(self):
        app = Application(self.worlds)
        body = {'id': '2' * 32, 'name': 'New Road', 'player': 'Jory'}
        for index in range(32):
            attempt = dict(body, id=f'{index + 32:032x}', world_pass=f'{index + 32:064x}')
            self.assertEqual(self.request('/api/worlds', attempt,
                token=f'{index + 64:064x}', application=app)[0], 403)
        self.assertEqual(self.request('/api/worlds', body)[0], 403)
        self.assertEqual(self.worlds.db.execute('SELECT count(*) FROM worlds').fetchone()[0], 1)
        permission = issue_world_pass(self.path)
        body['world_pass'] = permission
        status, created = self.request('/api/worlds', body)
        self.assertEqual(status, 200)
        self.assertEqual(created['id'], body['id'])
        self.assertNotIn(permission, json.dumps(created))
        stored = self.worlds.db.execute('SELECT pass_hash FROM world_passes WHERE claimed_world=?',
                                       (body['id'],)).fetchone()[0]
        self.assertNotEqual(stored, permission)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.request('/api/worlds', body)[0], 200)
        self.assertEqual(self.request('/api/worlds', {k: v for k, v in body.items() if k != 'world_pass'})[0], 200)
        self.assertEqual(self.request('/api/worlds', body, token='c' * 64)[0], 409)
        self.assertEqual(self.request('/api/worlds', dict(body, id='3' * 32), token='c' * 64)[0], 403)
        self.worlds.owner_action(body['id'], self.a, 'delete')
        self.assertEqual(self.request('/api/worlds', body)[0], 403)

    def test_one_world_pass_has_one_concurrent_winner(self):
        permission = issue_world_pass(self.path)
        app = Application(self.worlds)
        def create(index):
            return self.request('/api/worlds', {'id': str(index) * 32,
                'name': 'Contested Road', 'player': 'Jory', 'world_pass': permission},
                token=str(index) * 64, application=app)[0]
        with ThreadPoolExecutor(max_workers=2) as pool:
            self.assertEqual(sorted(pool.map(create, (2, 3))), [200, 403])
        self.assertEqual(self.worlds.db.execute('SELECT count(*) FROM worlds').fetchone()[0], 2)
        self.assertEqual(len(list(self.worlds.db.execute('PRAGMA foreign_key_check'))), 0)

    def test_full_host_preserves_pass_until_slot_is_recovered(self):
        self.create_world('2' * 32)
        body = {'id': '3' * 32, 'name': 'Fresh Road', 'player': 'Jory',
                'world_pass': issue_world_pass(self.path)}
        with patch('server.MAX_WORLDS', 2):
            self.assertEqual(self.request('/api/worlds', body)[0], 409)
            self.worlds.owner_action('2' * 32, self.a, 'delete')
            self.worlds.close()
            self.worlds = Worlds(self.path, self.engine)
            self.assertEqual(self.request('/api/worlds', body)[0], 200)

    def test_failed_creation_preserves_pass_and_has_one_retry_result(self):
        body = {'id': '2' * 32, 'name': 'Retry Road', 'player': 'Jory',
                'world_pass': issue_world_pass(self.path)}
        with patch.object(self.engine, 'open', side_effect=RuntimeError('test save failure')):
            with self.assertLogs(level='ERROR'):
                self.assertEqual(self.request('/api/worlds', body)[0], 503)
        self.assertEqual(self.request('/api/worlds', body)[0], 200)
        self.assertEqual(self.request('/api/worlds', body)[0], 200)
        self.assertEqual(self.worlds.db.execute('SELECT count(*) FROM worlds').fetchone()[0], 2)

    def test_operator_can_issue_pass_live_and_recover_world_with_exclusive_access(self):
        command = [sys.executable, str(Path(__file__).resolve().parents[1] / 'tools/coop/server.py'),
                   '--database', str(self.path)]
        permission = subprocess.check_output(command + ['--issue-world-pass'], text=True).strip()
        self.assertEqual(len(permission), 64)
        self.assertEqual(self.request('/api/worlds', {'id': '2' * 32, 'name': 'Operator Road',
            'player': 'Jory', 'world_pass': permission})[0], 200)
        listing = json.loads(subprocess.check_output(command + ['--list-worlds'], text=True))
        self.assertEqual({world['id'] for world in listing}, {self.id, '2' * 32})
        remove = command + ['--library', LIBRARY, '--delete-world', '2' * 32]
        busy = subprocess.run(remove, capture_output=True, text=True)
        self.assertNotEqual(busy.returncode, 0)
        self.worlds.close()
        try:
            self.assertEqual(json.loads(subprocess.check_output(remove, text=True)), {'deleted': True})
        finally:
            self.worlds = Worlds(self.path, self.engine)
        self.assertEqual(self.worlds.db.execute('SELECT count(*) FROM worlds').fetchone()[0], 1)

    def test_owner_deletes_only_their_world(self):
        self.worlds.pose(self.id, self.a, self.enter(self.a))
        self.worlds.command(self.id, self.a, self.command(self.a, amount=1, good=0))
        view = self.worlds.appearance(self.id, self.a,
            {'appearance': dict(skin=1, hair=2, style=3, face=1, coat=4)})
        self.worlds.save_session(self.id, self.a, dict(sequence=1,
            context=view['session_context'], session='CROWNLESS_SESSION 7\nlaunch test\n'))
        other = '2' * 32
        self.create_world(other)
        path = f'/api/worlds/{self.id}/host'
        self.assertEqual(self.request(path, {'action': 'delete'}, token=self.b)[0], 403)
        self.assertEqual(self.request(path, {'action': 'delete'}, token='c' * 64)[0], 403)
        self.assertEqual(self.worlds.view(self.id, self.a)['id'], self.id)
        status, result = self.request(path, {'action': 'delete'})
        self.assertEqual(status, 200)
        self.assertEqual(result, {'deleted': True})
        for table, column in [('worlds', 'id'), ('members', 'world'), ('receipts', 'world'),
                              ('sessions', 'world'), ('appearances', 'world'),
                              ('scene_contexts', 'world'), ('away_clocks', 'world'),
                              ('world_starts', 'world')]:
            self.assertEqual(self.worlds.db.execute(f'SELECT count(*) FROM {table} WHERE {column}=?', (self.id,)).fetchone()[0], 0)
        self.assertFalse(any(key[0] == self.id for key in self.worlds.seen))
        self.assertFalse(any(key[0] == self.id for key in self.worlds.visits))
        self.assertFalse(any(key[0] == self.id for key in self.worlds.poses))
        self.assertNotIn(self.id, self.worlds.last_tick)
        self.assertEqual(self.worlds.view(other, self.a)['id'], other)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        with self.assertRaises(ApiError):
            self.worlds.view(self.id, self.a)

    def test_delete_failure_rolls_back_world_and_crew(self):
        self.worlds.command(self.id, self.a, self.command(self.a, amount=1, good=0))
        self.worlds.db.execute("CREATE TRIGGER block_delete BEFORE DELETE ON worlds BEGIN SELECT RAISE(ABORT, 'test failure'); END")
        with self.assertRaises(sqlite3.IntegrityError):
            self.worlds.owner_action(self.id, self.a, 'delete')
        self.assertEqual(len(self.worlds.view(self.id, self.a)['crew']), 2)
        self.assertEqual(self.worlds.db.execute('SELECT count(*) FROM receipts WHERE world=?', (self.id,)).fetchone()[0], 1)

    def test_protocol_auth_and_clock_boundary(self):
        path = f'/api/worlds/{self.id}/command'
        self.assertEqual(self.request(path, self.command(self.a, 'advance'))[0], 400)
        self.assertEqual(self.request(path, dict(self.command(self.a), actor=self.b))[0], 400)
        self.assertEqual(self.request(path, dict(self.command(self.a), protocol=2))[0], 409)
        self.assertEqual(self.request(path, dict(self.command(self.a), protocol=True))[0], 409)
        self.assertEqual(self.request(path, self.command(self.a), origin='https://elsewhere.test')[0], 403)
        self.assertEqual(self.request(f'/api/worlds/{self.id}/state', token='c' * 64)[0], 403)
        self.assertEqual(self.request(path, dict(self.command(self.a), amount=True))[0], 400)
        self.assertEqual(self.request(path, dict(self.command(self.a), target='18446744073709551616'))[0], 400)

    def test_shared_stable_care_applies_once_and_survives_long_return(self):
        departure = self.worlds.view(self.id, self.a)
        route = next(option for option in departure['state']['travel']
                     if option['available'])
        started = self.worlds.command(self.id, self.a,
            self.command(self.a, 'travel', target=route['id']))
        self.assertTrue(started['accepted'])
        for step in range(50):
            state = self.worlds.view(self.id, self.a)['state']
            journey = state['journey']
            if not journey['active']:
                break
            site = journey['road_site']
            if site:
                action, target = 'pass_road_site', site['id']
            elif journey['phase'] == 4:
                position = state['road_position']
                forward = [leg for leg in position['next_legs']
                           if leg['direction'] == position['direction']]
                self.assertTrue(forward)
                leg = next((leg for leg in forward if leg['kind'] == 1),
                           forward[0])
                action, target = 'road_leg', leg['token']
            elif journey['phase'] == 3:
                action, target = ('break' if journey['stop'] == 1 else 'camp'), '0'
            else:
                self.assertEqual(journey['phase'], 1)
                action, target = 'skip_watch', '0'
            member = (self.a, self.b)[step % 2]
            result = self.worlds.command(self.id, member,
                self.command(member, action, target=target))
            self.assertTrue(result['accepted'], result['message'])
        arrived = self.worlds.view(self.id, self.b)
        state = arrived['state']
        self.assertFalse(state['journey']['active'])
        self.assertEqual(state['company']['location'], route['id'])
        offer = state['horse_care']
        self.assertTrue(offer['available'])
        self.assertEqual((offer['source'], offer['care_wheat'], offer['days']),
                         ('stable market', 1, 1))
        self.assertGreaterEqual(state['market']['stock'][7], 1)
        bodies = [(token, self.command(token, 'care_horses'))
                  for token in (self.a, self.b)]

        def apply(item):
            try:
                return item[0], self.worlds.command(self.id, *item)
            except ApiError as error:
                self.assertEqual(error.status, 409)
                return item[0], None

        with ThreadPoolExecutor(max_workers=2) as pool:
            results = list(pool.map(apply, bodies))
        winners = [(token, result) for token, result in results if result is not None]
        self.assertEqual(len(winners), 1)
        winner, receipt = winners[0]
        self.assertTrue(receipt['accepted'])
        cared = receipt['world']['state']
        self.assertEqual(cared['day'], state['day'] + 1)
        self.assertEqual(cared['company']['coins'], state['company']['coins'] - offer['cost'])
        self.assertTrue(any('Care at ' in event['text'] and
                            '1 Wheat from the stable market' in event['text'] and
                            '1 day.' in event['text']
                            for event in cared['events']))
        self.assertEqual(self.worlds.view(self.id, self.a)['state'],
                         self.worlds.view(self.id, self.b)['state'])
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        repeat = self.worlds.command(self.id, winner,
            next(body for token, body in bodies if token == winner))
        self.assertTrue(repeat['duplicate'])
        self.assertEqual(repeat['world']['state'], cared)

        base = self.worlds.db.execute(
            'SELECT last_human FROM away_clocks WHERE world=?',
            (self.id,)).fetchone()[0]
        self.worlds.seen.clear()
        self.worlds.tick(wall_now=base + AWAY_GRACE - 1)
        self.assertEqual(self.worlds.view(self.id, self.a, present=False)['state']['day'],
                         cared['day'])
        self.worlds.tick(wall_now=base + AWAY_GRACE + 3600)
        long = self.worlds.view(self.id, self.a, present=False)
        self.assertGreater(long['state']['day'], cared['day'] + 100)
        self.worlds.close()
        self.worlds = Worlds(self.path, self.engine)
        returned = self.worlds.view(self.id, self.b, campaign=True)
        self.assertEqual(returned['state'], long['state'])
        self.assertEqual(returned['state']['company']['location'], route['id'])
        self.assertEqual(returned['state']['horse_care'],
                         self.worlds.view(self.id, self.a)['state']['horse_care'])

    def test_campaign_poll_and_shared_skip(self):
        view = self.worlds.view(self.id, self.a, campaign=True)
        self.assertIn('campaign', view)
        self.assertNotIn('campaign', self.worlds.view(self.id, self.b, True, str(view['revision'])))
        target = view['state']['travel'][0]['id']
        self.assertTrue(self.worlds.command(self.id, self.a, self.command(self.a, 'travel', target=target))['accepted'])
        skipped = self.worlds.command(self.id, self.b, self.command(self.b, 'skip_watch'))
        self.assertTrue(skipped['accepted'])
        self.assertEqual(skipped['world']['state'], self.worlds.view(self.id, self.a)['state'])
        self.assertNotEqual(skipped['world']['state']['hash'], view['state']['hash'])


if __name__ == '__main__':
    unittest.main()
