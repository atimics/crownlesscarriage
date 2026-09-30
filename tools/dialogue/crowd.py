"""A helper meeting each hungry person in turn: resumable, cloneable, scoreable.

The world is either in-process (fastworld, the default) or driven through the
probe executables. Both give the same results; the probes are the reference.
"""
import copy
from pathlib import Path
import random
import shutil
import tempfile

from meaning import candidates
from world_dialogue import World

HORIZON = 7   # days advanced after the meetings before hunger is read again
CACHE_LIMIT = 200   # loaded worlds kept per process (about 1.7 MB each)
_cache = {}


def _base_world(path):
    """A loaded world for this save; callers clone it. Loading costs 8 ms, cloning 0.4 ms."""
    from fastworld import FastWorld
    if path not in _cache:
        if len(_cache) >= CACHE_LIMIT:
            _cache.pop(next(iter(_cache))).close()
        # Processes loading one save at once lock its database; each loads a private copy.
        with tempfile.TemporaryDirectory(prefix='crowd-load-') as folder:
            copy = Path(folder) / 'world.ccsave'
            for suffix in ('', '-wal', '-shm'):
                if Path(path + suffix).exists():
                    shutil.copyfile(path + suffix, str(copy) + suffix)
            _cache[path] = FastWorld.load(copy)
    return _cache[path]


def fast_available():
    try:
        from fastworld import library
        library()
        return True
    except (FileNotFoundError, OSError):
        return False


class Crowd:
    def __init__(self, scenario, food_probe=None, participant_probe=None, seed=0, limit=12, fast=None, _blank=False):
        self.food_probe = None if food_probe is None else str(food_probe)
        self.participant_probe = None if participant_probe is None else str(participant_probe)
        self.limit = limit
        self.fast = fast_available() if fast is None else fast
        self.dir = None if self.fast else Path(tempfile.mkdtemp(prefix='crowd-'))
        if _blank:
            return
        if self.fast:
            self.world = _base_world(scenario['path']).clone()
        else:
            src = Path(scenario['path'])
            for suffix in ('', '-wal', '-shm'):
                if Path(str(src) + suffix).exists():
                    shutil.copyfile(str(src) + suffix, str(self.dir / 'world.ccsave') + suffix)
            self.world = World(self.dir / 'world.ccsave', self.food_probe, self.participant_probe)
        self.helper = scenario['helper']
        self.order = list(scenario['hungry'])
        random.Random(seed).shuffle(self.order)
        self.observed = self.world.call('--observe', self.helper, self.order[0])
        self.start = {p['id']: p for p in self.world.call('--list')['people']}
        self.idx, self.history, self.snap, self.turn, self.bought, self.turns = 0, [], None, 0, 0, 0

    def clone(self):
        other = Crowd(None, self.food_probe, self.participant_probe, 0, self.limit, self.fast, _blank=True)
        if self.fast:
            other.world = self.world.clone()
        else:
            for suffix in ('', '-wal', '-shm'):
                path = Path(str(self.dir / 'world.ccsave') + suffix)
                if path.exists():
                    shutil.copyfile(path, str(other.dir / 'world.ccsave') + suffix)
            other.world = World(other.dir / 'world.ccsave', self.food_probe, self.participant_probe)
        for key in ('helper', 'order', 'observed', 'start', 'idx', 'history', 'snap', 'turn', 'bought', 'turns'):
            setattr(other, key, copy.deepcopy(getattr(self, key)))
        return other

    def close(self):
        if self.fast:
            self.world.close()
        else:
            shutil.rmtree(self.dir, ignore_errors=True)

    def decision(self):
        """The speaker's view and legal options, or None when every conversation is over."""
        if self.idx >= len(self.order):
            return None
        if self.snap is None:
            self.snap = self.world.snapshot(self.order[self.idx], self.helper)
            self.history, self.turn = [], 0
        person = self.snap['participants'][self.turn % 2]
        person['waiting'] = len(self.order) - self.idx - 1   # others still to meet
        return person, candidates(person, self.history)

    def act(self, index):
        person, options = self.decision()
        act = options[index]
        self.turns += 1
        self.turn += 1
        self.history.append({'speaker_id': act['actor'], 'act': copy.deepcopy(act)})
        over = act['intent'] == 'end' or self.turn >= self.limit
        if act['intent'] == 'accept':
            try:
                self.world.execute(act['proposal'])
                self.bought += act['proposal']['quantity']
                self.snap = self.world.snapshot(self.order[self.idx], self.helper)
            except (RuntimeError, ValueError):
                over = True
        if over:
            self.idx, self.snap, self.history, self.turn = self.idx + 1, None, [], 0

    def run(self, chooser, rng):
        while (d := self.decision()) is not None:
            person, options = d
            self.act(chooser(person, self.history, options, rng))
        return self.result()

    def result(self):
        now = {p['id']: p for p in self.world.call('--list')['people']}
        week = {p['id']: p for p in self.world.call('--days', HORIZON, '--list')['people']}
        end = self.world.call('--observe', self.helper, self.order[0])
        crowd, helper = self.order, self.helper
        return {'people': len(crowd),
                'relieved_now': sum(now[str(p)]['hungry_days'] == 0 for p in crowd),
                'hungry_days_week': sum(week[str(p)]['hungry_days'] for p in crowd),
                'helper_hungry_week': week[str(helper)]['hungry_days'],
                'crowns_spent': self.start[str(helper)]['coins'] - now[str(helper)]['coins'],
                'helper_coins_left': now[str(helper)]['coins'],
                'units_bought': self.bought, 'stock_before': self.observed['stock'],
                'hungry_before': sum(self.start[str(p)]['hungry_days'] for p in crowd) / len(crowd),
                'turns': self.turns, 'stock_share_after': end['stock'] / max(1, end['reserve_target'])}
