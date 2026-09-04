"""The shared carriage uses the same C simulation as the game client."""
import ctypes as c
import json
from pathlib import Path


class Engine:
    def __init__(self, library):
        self.library_path = Path(library).resolve()
        self.lib = c.CDLL(str(self.library_path))
        pointer, size, text = c.c_void_p, c.c_size_t, c.c_char_p
        self.lib.CcCoopCreate.argtypes = [c.c_uint32]
        self.lib.CcCoopCreate.restype = pointer
        self.lib.CcCoopDestroy.argtypes = [pointer]
        self.lib.CcCoopDestroy.restype = None
        self.lib.CcCoopFree.argtypes = [pointer]
        self.lib.CcCoopFree.restype = None
        signatures = {
            "CcCoopApply": [pointer, text, c.c_uint64, c.c_int32, c.c_int32, text, size],
            "CcCoopAdvance": [pointer, c.c_int32, text, size],
            "CcCoopAdvanceAway": [pointer, c.c_int32, text, size],
            "CcCoopSnapshot": [pointer, text, size],
            "CcCoopEncode": [pointer, c.POINTER(pointer), c.POINTER(size), text, size],
            "CcCoopDecode": [pointer, text, size, text, size],
        }
        for name, args in signatures.items():
            fn = getattr(self.lib, name)
            fn.argtypes, fn.restype = args, c.c_bool

    def open(self, seed=42, saved=None):
        return Campaign(self, seed, saved)


class Campaign:
    def __init__(self, engine, seed, saved):
        self.lib = engine.lib
        self.handle = self.lib.CcCoopCreate(seed)
        self.error = c.create_string_buffer(512)
        if not self.handle:
            raise RuntimeError("The campaign could not allocate memory.")
        if saved is not None and not self.lib.CcCoopDecode(
            self.handle, saved, len(saved), self.error, len(self.error)
        ):
            self.close()
            raise RuntimeError("Campaign recovery failed: " + self.error.value.decode("utf-8"))

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.close()

    def close(self):
        if self.handle:
            self.lib.CcCoopDestroy(self.handle)
            self.handle = None

    def snapshot(self):
        output = c.create_string_buffer(131072)
        if not self.lib.CcCoopSnapshot(self.handle, output, len(output)):
            raise RuntimeError("The campaign view exceeded its limit.")
        return json.loads(output.value)

    def save(self):
        data, length = c.c_void_p(), c.c_size_t()
        if not self.lib.CcCoopEncode(self.handle, c.byref(data), c.byref(length), self.error, len(self.error)):
            raise RuntimeError("Campaign save failed: " + self.error.value.decode("utf-8"))
        try:
            return c.string_at(data, length.value)
        finally:
            self.lib.CcCoopFree(data)

    def apply(self, action, target=0, good=0, amount=0):
        self.error.value = b""
        ok = self.lib.CcCoopApply(self.handle, action.encode("ascii"), int(target), good, amount,
                                 self.error, len(self.error))
        return bool(ok), self.error.value.decode("utf-8")

    def advance(self, ticks):
        if not self.lib.CcCoopAdvance(self.handle, ticks, self.error, len(self.error)):
            raise RuntimeError("Campaign clock failed: " + self.error.value.decode("utf-8"))

    def advance_away(self, days):
        if not self.lib.CcCoopAdvanceAway(self.handle, days, self.error, len(self.error)):
            raise RuntimeError("Away clock failed: " + self.error.value.decode("utf-8"))
