#!/usr/bin/env python3

from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "blender"))

from batch_static_glb import batch_static_glb
from inspect_glb import (
    CHUNK_BIN,
    CHUNK_JSON,
    GLB_MAGIC,
    GlbError,
    collect_stats,
    parse_glb,
)


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

    def test_static_batcher_preserves_carriage_geometry(self) -> None:
        source = (ROOT / "assets" / "exports" / "glb" /
                  "carriage_base_v01.glb")
        output = Path(self.temporary.name) / source.name
        before_document, _ = parse_glb(source)
        before = collect_stats(source, before_document)

        original_primitives, batched_primitives = batch_static_glb(
            source, output)
        after_document, _ = parse_glb(output)
        after = collect_stats(output, after_document)

        self.assertGreater(original_primitives, batched_primitives)
        self.assertEqual(batched_primitives, len(before_document["materials"]))
        self.assertEqual(after.meshes, 1)
        self.assertEqual(after.primitives, batched_primitives)
        self.assertEqual(after.triangles, before.triangles)
        for actual, expected in zip(after.bounds_min, before.bounds_min):
            self.assertAlmostEqual(actual, expected, delta=0.02)
        for actual, expected in zip(after.bounds_max, before.bounds_max):
            self.assertAlmostEqual(actual, expected, delta=0.02)

    def test_static_batcher_keeps_painted_market_channels(self) -> None:
        source = (ROOT / "assets" / "exports" / "glb" /
                  "environment_market_granary_v01.glb")
        output = Path(self.temporary.name) / source.name
        original_primitives, batched_primitives = batch_static_glb(
            source, output)
        document, _ = parse_glb(output)

        self.assertGreater(original_primitives, batched_primitives)
        self.assertEqual(batched_primitives, len(document["materials"]))
        primitives = document["meshes"][0]["primitives"]
        self.assertTrue(primitives)
        for primitive in primitives:
            self.assertIn("COLOR_0", primitive["attributes"])
            self.assertIn("COLOR_1", primitive["attributes"])


if __name__ == "__main__":
    unittest.main()
