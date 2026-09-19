#!/usr/bin/env python3
"""Run two separate participant workers against a native simulation snapshot.

Workers read one JSON request per line and write one JSON response per line.
Each response is one speech turn or an available action. See README.md.
"""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import selectors
import sqlite3
import subprocess
import time

PROTOCOL = 'crownless-participant-v1'
INSTRUCTION = (
    'You are the person named in self. Take one turn as that person. '
    'Use your own state, held accounts, and observed memories. '
    'Treat received speech as what that speaker said. '
    'Respond to the person speaking to you and pursue your own needs and interests. '
    'Produce exactly one JSON object: {"kind":"speech","text":"your spoken words"} '
    'or {"kind":"action","action":"end_conversation"}. '
    'The speech text contains only your utterance. Available actions are listed in your view.'
)


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=False).encode()).hexdigest()


def validate_snapshot(snapshot):
    if snapshot.get('version') != 1 or len(snapshot.get('participants', [])) != 2:
        raise ValueError('expected version 1 with two participants')
    a, b = snapshot['participants']
    ids = [p['self']['id'] for p in (a, b)]
    if len(set(ids)) != 2 or any(not isinstance(i, str) or not i.isdecimal() or int(i) == 0 for i in ids):
        raise ValueError('expected two distinct string person IDs')
    for own, other in ((a, b), (b, a)):
        if own['listener']['id'] != other['self']['id']:
            raise ValueError('listener identity differs from the paired participant')
        if own['place']['id'] != other['place']['id'] or own['self']['in_transit']:
            raise ValueError('participants must be present in the same place')
        if own['day'] != snapshot['day']:
            raise ValueError('participant day differs from snapshot')
        if own['available_actions'] != ['end_conversation']:
            raise ValueError('unsupported action set')


def validate_turn(value, participant):
    if not isinstance(value, dict):
        raise ValueError('a turn must be one object')
    if value.get('kind') == 'speech' and set(value) == {'kind', 'text'}:
        text = value['text']
        if not isinstance(text, str) or not text.strip() or len(text.encode('utf-8')) > 511:
            raise ValueError('speech must contain 1..511 UTF-8 bytes')
        if any(ord(c) < 32 or ord(c) == 127 for c in text):
            raise ValueError('speech must be one line')
        return value
    if value.get('kind') == 'action' and set(value) == {'kind', 'action'}:
        if value['action'] in participant['available_actions']:
            return value
    raise ValueError('expected one speech turn or an available action')


class Worker:
    def __init__(self, command, stderr_path):
        if not isinstance(command, list) or not command or any(not isinstance(x, str) for x in command):
            raise ValueError('worker command must be a JSON array of arguments')
        self.log = stderr_path.open('wb')
        try:
            self.process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                            stderr=self.log)
        except BaseException:
            self.log.close()
            raise
        self.selector = selectors.DefaultSelector()
        self.selector.register(self.process.stdout, selectors.EVENT_READ)
        self.buffer = b''
        self.raw = b''

    def turn(self, request, timeout):
        self.raw = b''
        self.process.stdin.write((json.dumps(request, ensure_ascii=False) + '\n').encode())
        self.process.stdin.flush()
        deadline = time.monotonic() + timeout
        while b'\n' not in self.buffer:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not self.selector.select(remaining):
                self.raw = self.buffer
                raise TimeoutError('participant response timed out')
            data = os.read(self.process.stdout.fileno(), 4096)
            if not data:
                self.raw = self.buffer
                raise RuntimeError('participant worker closed its output')
            self.buffer += data
            self.raw = self.buffer
            if len(self.buffer) > 65536:
                raise ValueError('participant response exceeds 64 KiB')
        line, self.buffer = self.buffer.split(b'\n', 1)
        self.raw = line
        if self.buffer.strip():
            raise ValueError('participant emitted several responses for one turn')
        return json.loads(line)

    def close(self):
        self.selector.close()
        if self.process.poll() is None:
            self.process.terminate()
            try:
                self.process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        self.process.stdin.close()
        self.process.stdout.close()
        self.log.close()


class Memory:
    """Store only events each person observed, scoped to a named world."""
    def __init__(self, path):
        self.db = sqlite3.connect(path)
        self.db.execute('''CREATE TABLE IF NOT EXISTS observation (
            world TEXT, person TEXT, episode TEXT, turn INTEGER, day INTEGER,
            payload TEXT, PRIMARY KEY(world, person, episode, turn))''')
        self.db.commit()

    def read(self, world, person, day):
        latest = self.db.execute('SELECT MAX(day) FROM observation WHERE world=? AND person=?',
                                 (world, person)).fetchone()[0]
        if latest is not None and latest > day:
            raise ValueError('memory belongs to a later simulation day')
        rows = self.db.execute('''SELECT payload FROM (
            SELECT rowid, payload FROM observation WHERE world=? AND person=?
            ORDER BY rowid DESC LIMIT 16) ORDER BY rowid''', (world, person))
        return [json.loads(r[0]) for r in rows]

    def record(self, world, people, episode, turn, day, event):
        with self.db:
            for person in people:
                self.db.execute('INSERT INTO observation VALUES (?,?,?,?,?,?)',
                                (world, person, episode, turn, day, json.dumps(event)))

    def close(self):
        self.db.close()


def run(snapshot, commands, output, turns=8, timeout=60, memory_path=None, world=None, episode=None):
    validate_snapshot(snapshot)
    if not 1 <= turns <= 32 or timeout <= 0:
        raise ValueError('use 1..32 turns and a positive timeout')
    if len(commands) != 2:
        raise ValueError('provide two worker commands')
    if memory_path is not None and (not world or not episode):
        raise ValueError('persistent memory requires explicit world and episode IDs')
    output.mkdir(parents=True, exist_ok=False)
    receipt = {'protocol': PROTOCOL, 'snapshot_sha256': digest(snapshot),
               'world_seed': snapshot['world_seed'], 'state_hash': snapshot['state_hash'],
               'day': snapshot['day'], 'commands': commands, 'requested_turns': turns,
               'world': world, 'episode': episode, 'status': 'running', 'completed_turns': 0,
               'runner_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
               'worker_files': {arg: hashlib.sha256(Path(arg).read_bytes()).hexdigest()
                                for command in commands for arg in command if Path(arg).is_file()}}
    (output / 'snapshot.json').write_text(json.dumps(snapshot, indent=2) + '\n')
    workers = []
    memory = None
    records = []
    try:
        memory = Memory(memory_path) if memory_path else None
        people = copy.deepcopy(snapshot['participants'])
        ids = [p['self']['id'] for p in people]
        memories = [memory.read(world, i, snapshot['day']) if memory else [] for i in ids]
        # Each process receives only its own view. The pair snapshot remains with the runner.
        workers = []
        for i, command in enumerate(commands):
            workers.append(Worker(command, output / f'worker-{i}.stderr'))
        observed = []
        for turn in range(turns):
            index = turn % 2
            view = copy.deepcopy(people[index])
            request = {'protocol': PROTOCOL, 'instruction': INSTRUCTION, 'participant': view,
                       'remembered_observations': memories[index], 'observed_turns': copy.deepcopy(observed)}
            record = {'turn': turn, 'speaker_id': ids[index], 'input': request,
                      'input_sha256': digest(request), 'status': 'attempted'}
            records.append(record)
            start = time.monotonic()
            try:
                reply = workers[index].turn(request, timeout)
                record['output'] = reply
                validate_turn(reply, view)
                event = {'speaker_id': ids[index], 'day': snapshot['day'], **reply}
                if memory:
                    memory.record(world, ids, episode, turn, snapshot['day'], event)
                observed.append(event)
                record['status'] = 'accepted'
                receipt['completed_turns'] += 1
            except Exception as error:
                record['status'] = 'failed'
                record['error'] = f'{type(error).__name__}: {error}'
                record['raw_output'] = workers[index].raw.decode('utf-8', errors='replace')
                raise
            finally:
                record['elapsed_seconds'] = time.monotonic() - start
                (output / f'turn-{turn:03}.json').write_text(json.dumps(record, indent=2) + '\n')
            if reply['kind'] == 'action':
                receipt['status'] = 'ended_by_participant'
                break
        else:
            receipt['status'] = 'turn_limit'
        return receipt
    except Exception as error:
        receipt['status'] = 'failed'
        receipt['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        for worker in workers:
            worker.close()
        if memory:
            memory.close()
        # Training rows keep each actor's input and exactly their own accepted output.
        rows = [{'episode': episode, 'review_status': 'pending', 'speaker_id': r['speaker_id'], 'input': r['input'],
                 'output': r['output']} for r in records if r['status'] == 'accepted']
        (output / 'turns.jsonl').write_text(''.join(json.dumps(r) + '\n' for r in records))
        (output / 'training-candidates.jsonl').write_text(''.join(json.dumps(r) + '\n' for r in rows))
        receipt['files'] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                            for p in output.iterdir() if p.is_file()}
        (output / 'receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--snapshot', type=Path, required=True)
    parser.add_argument('--first-worker', required=True, help='JSON argv array')
    parser.add_argument('--second-worker', required=True, help='JSON argv array')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--turns', type=int, default=8)
    parser.add_argument('--timeout', type=float, default=60)
    parser.add_argument('--memory-db', type=Path)
    parser.add_argument('--world')
    parser.add_argument('--episode')
    args = parser.parse_args()
    result = run(json.loads(args.snapshot.read_text()),
                 [json.loads(args.first_worker), json.loads(args.second_worker)],
                 args.output, args.turns, args.timeout, args.memory_db, args.world, args.episode)
    print(json.dumps(result))


if __name__ == '__main__':
    main()
