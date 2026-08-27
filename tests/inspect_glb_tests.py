#!/usr/bin/env python3
"""Focused corruption tests for the shipped GLB validator."""

from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "blender"))

from inspect_glb import CHUNK_BIN, CHUNK_JSON, GLB_MAGIC, GlbError, parse_glb


def write_glb(path: Path, document: dict, binary: bytes) -> None:
    encoded = json.dumps(document, separators=(",", ":")).encode("utf-8")
    encoded += b" " * (-len(encoded) % 4)
    binary += b"\0" * (-len(binary) % 4)
    chunks = (
        struct.pack("<II", len(encoded), CHUNK_JSON) + encoded +
        struct.pack("<II", len(binary), CHUNK_BIN) + binary
    )
    path.write_bytes(
        struct.pack("<4sII", GLB_MAGIC, 2, 12 + len(chunks)) + chunks)


def minimal_document(buffer_length: int = 12,
                     accessor_count: int = 1) -> dict:
    return {
        "asset": {"version": "2.0"},
        "buffers": [{"byteLength": buffer_length}],
        "bufferViews": [{"buffer": 0, "byteOffset": 0,
                         "byteLength": buffer_length}],
        "accessors": [{"bufferView": 0, "componentType": 5126,
                       "count": accessor_count, "type": "VEC3"}],
        "nodes": [],
    }


class InspectGlbTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.path = Path(self.temporary.name) / "fixture.glb"

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_valid_accessor_range_is_accepted(self) -> None:
        write_glb(self.path, minimal_document(), b"\0" * 12)
        document, binary = parse_glb(self.path)
        self.assertEqual(document["accessors"][0]["count"], 1)
        self.assertEqual(len(binary), 12)

    def test_declared_buffer_larger_than_bin_is_rejected(self) -> None:
        write_glb(self.path, minimal_document(), b"\0" * 8)
        with self.assertRaisesRegex(GlbError, "declares 12 bytes"):
            parse_glb(self.path)

    def test_accessor_overrun_is_rejected(self) -> None:
        write_glb(self.path, minimal_document(accessor_count=2), b"\0" * 12)
        with self.assertRaisesRegex(GlbError, "beyond bufferView"):
            parse_glb(self.path)


if __name__ == "__main__":
    unittest.main()
