import io
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import time
import unittest
from concurrent.futures import ThreadPoolExecutor
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'coop'))
from engine import Engine
from server import ApiError, Application, Worlds, away_days, AWAY_GRACE, AWAY_RAMP

LIBRARY = sys.argv.pop(1)


class CoopTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.engine = Engine(LIBRARY)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.path = Path(self.temp.name) / 'worlds.sqlite3'
        self.worlds = Worlds(self.path, self.engine)
        self.a, self.b, self.id = 'a' * 64, 'b' * 64, '1' * 32
        self.worlds.create(self.a, {'id': self.id, 'name': 'Lantern Road', 'player': 'Mara', 'seed': 0xc0a71a9e})
        self.invite = self.worlds.invite(self.id, self.a)['invite']
        self.worlds.join(self.id, self.b, {'player': 'Bren', 'invite': self.invite})

    def tearDown(self):
        self.worlds.close()
        self.temp.cleanup()

    def command(self, token, action='trade', **values):
        view = self.worlds.view(self.id, token)
        return {'protocol': 1, 'sequence': view['next_sequence'],
                'action_revision': view['action_revision'], 'action': action, **values}

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
        self.worlds.create(self.a, {'id': other, 'name': 'Other Road', 'player': 'Mara'})
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

    def request(self, path, body=None, token=None, origin='http://localhost:8787'):
        raw = json.dumps(body).encode() if body is not None else b''
        env = {'REQUEST_METHOD': 'POST' if body is not None else 'GET', 'PATH_INFO': path,
               'CONTENT_TYPE': 'application/json', 'CONTENT_LENGTH': str(len(raw)),
               'wsgi.input': io.BytesIO(raw), 'wsgi.url_scheme': 'http',
               'HTTP_HOST': 'localhost:8787', 'HTTP_ORIGIN': origin,
               'HTTP_AUTHORIZATION': 'Bearer ' + (token or self.a)}
        statuses = []
        data = b''.join(Application(self.worlds)(env, lambda status, headers: statuses.append(status)))
        return int(statuses[0].split()[0]), json.loads(data)

    def test_owner_deletes_only_their_world(self):
        self.worlds.command(self.id, self.a, self.command(self.a, amount=1, good=0))
        view = self.worlds.appearance(self.id, self.a,
            {'appearance': dict(skin=1, hair=2, style=3, face=1, coat=4)})
        self.worlds.save_session(self.id, self.a, dict(sequence=1,
            context=view['session_context'], session='CROWNLESS_SESSION 7\nlaunch test\n'))
        other = '2' * 32
        self.worlds.create(self.a, {'id': other, 'name': 'Other Road', 'player': 'Mara'})
        path = f'/api/worlds/{self.id}/host'
        self.assertEqual(self.request(path, {'action': 'delete'}, token=self.b)[0], 403)
        self.assertEqual(self.request(path, {'action': 'delete'}, token='c' * 64)[0], 403)
        self.assertEqual(self.worlds.view(self.id, self.a)['id'], self.id)
        status, result = self.request(path, {'action': 'delete'})
        self.assertEqual(status, 200)
        self.assertEqual(result, {'deleted': True})
        for table, column in [('worlds', 'id'), ('members', 'world'), ('receipts', 'world'),
                              ('sessions', 'world'), ('appearances', 'world'),
                              ('scene_contexts', 'world'), ('away_clocks', 'world')]:
            self.assertEqual(self.worlds.db.execute(f'SELECT count(*) FROM {table} WHERE {column}=?', (self.id,)).fetchone()[0], 0)
        self.assertFalse(any(key[0] == self.id for key in self.worlds.seen))
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
