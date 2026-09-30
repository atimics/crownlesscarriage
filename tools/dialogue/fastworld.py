"""In-process food-relief worlds (libcrowdsim), with the interface of world_dialogue.World.

Each call is a function call instead of a process launch, and `clone` copies the
world in memory. Set CROWDSIM_LIB to the shared library, or it is searched for in
`build*/` next to the repository.
"""
import ctypes
import json
import os
from pathlib import Path

BUFFER = 1 << 20


def _find():
    named = os.environ.get('CROWDSIM_LIB')
    if named:
        return Path(named)
    root = Path(__file__).resolve().parents[2]
    for base in (root, root.parent):
        for pattern in ('build*/libcrowdsim.*', 'crownless-ctrain-build/libcrowdsim.*', 'out/build/*/libcrowdsim.*'):
            hits = sorted(base.glob(pattern))
            if hits:
                return hits[0]
    return None


_lib = None


def library():
    global _lib
    if _lib is None:
        path = _find()
        if path is None or not path.exists():
            raise FileNotFoundError('libcrowdsim not found; build target crowdsim or set CROWDSIM_LIB')
        lib = ctypes.CDLL(str(path))
        c_u64, c_p, c_i = ctypes.c_uint64, ctypes.c_void_p, ctypes.c_int
        lib.cs_load.restype = c_p
        lib.cs_load.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
        lib.cs_clone.restype = c_p
        lib.cs_clone.argtypes = [c_p]
        lib.cs_free.argtypes = [c_p]
        lib.cs_hash.restype = c_u64
        lib.cs_hash.argtypes = [c_p]
        lib.cs_advance.argtypes = [c_p, c_i]
        for name, args in (('cs_list', []), ('cs_observe', [c_u64, c_u64]), ('cs_view', [c_u64, c_u64]),
                           ('cs_accept', [c_u64, c_u64]), ('cs_execute', [c_u64, c_u64])):
            fn = getattr(lib, name)
            fn.restype = c_i
            fn.argtypes = [c_p] + args + [ctypes.c_char_p, ctypes.c_size_t]
        lib.cs_promise.restype = c_i
        lib.cs_promise.argtypes = [c_p, c_u64, c_u64, c_i, c_i, ctypes.c_char_p, ctypes.c_size_t]
        _lib = lib
    return _lib


class FastWorld:
    def __init__(self, handle):
        self.handle = handle
        self.lib = library()
        self.buffer = ctypes.create_string_buffer(BUFFER)

    @classmethod
    def load(cls, path):
        lib = library()
        error = ctypes.create_string_buffer(256)
        handle = lib.cs_load(str(path).encode(), error, len(error))
        if not handle:
            raise RuntimeError(error.value.decode() or 'could not load world')
        return cls(handle)

    def clone(self):
        return FastWorld(self.lib.cs_clone(self.handle))

    def close(self):
        if self.handle:
            self.lib.cs_free(self.handle)
            self.handle = None

    def __del__(self):
        try:
            self.close()
        except Exception:
            pass

    def hash(self):
        return self.lib.cs_hash(self.handle)

    def advance(self, days):
        self.lib.cs_advance(self.handle, int(days))

    def _json(self, fn, *args):
        n = fn(self.handle, *args, self.buffer, BUFFER)
        if n < 0:
            raise RuntimeError('native world command failed')
        return json.loads(self.buffer.raw[:n])

    def call(self, *args, mutate=False):
        """The few probe commands the crowd code uses, on this world."""
        args = list(map(str, args))
        if args[:1] == ['--observe']:
            return self._json(self.lib.cs_observe, int(args[1]), int(args[2]))
        if args == ['--list']:
            return self._json(self.lib.cs_list)
        if args[:1] == ['--days'] and args[2:] == ['--list'] and mutate:   # let the days pass in this world
            self.advance(int(args[1]))
            return self._json(self.lib.cs_list)
        if args[:1] == ['--days'] and args[2:] == ['--list']:   # look ahead without changing this world
            other = self.clone()
            other.advance(int(args[1]))
            try:
                return other._json(other.lib.cs_list)
            finally:
                other.close()
        raise ValueError('unsupported world command: ' + ' '.join(args))

    def snapshot(self, first, second):
        return self._json(self.lib.cs_view, int(first), int(second))

    def execute(self, terms):
        if terms['condition'] != 'now':
            raise ValueError('wait until the stated condition is available')
        observed = self.call('--observe', terms['payer_id'], terms['beneficiary_id'])
        if terms['place_id'] != observed['place_id']:
            raise ValueError('agreement place changed')
        promised = self._json(self.lib.cs_promise, int(terms['payer_id']), int(terms['beneficiary_id']),
                              int(terms['quantity']), int(terms['unit_price']))
        accepted = self._json(self.lib.cs_accept, int(promised['agreement_id']), int(terms['beneficiary_id']))
        result = self._json(self.lib.cs_execute, int(promised['agreement_id']), int(terms['payer_id']))
        return {'promise': promised, 'accepted': accepted, 'result': result}
