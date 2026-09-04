import io
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import time
import unittest
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools' / 'coop'))
from engine import Engine
from server import ApiError, Application, Worlds

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
        other = '2' * 32
        self.worlds.create(self.a, {'id': other, 'name': 'Other Road', 'player': 'Mara'})
        path = f'/api/worlds/{self.id}/host'
        self.assertEqual(self.request(path, {'action': 'delete'}, token=self.b)[0], 403)
        self.assertEqual(self.request(path, {'action': 'delete'}, token='c' * 64)[0], 403)
        self.assertEqual(self.worlds.view(self.id, self.a)['id'], self.id)
        status, result = self.request(path, {'action': 'delete'})
        self.assertEqual(status, 200)
        self.assertEqual(result, {'deleted': True})
        for table, column in [('worlds', 'id'), ('members', 'world'), ('receipts', 'world')]:
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
