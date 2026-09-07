#!/usr/bin/env python3

from __future__ import annotations

import json
import math
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
        # The batched body keeps one primitive per material; the four wheels
        # stay as their own meshes so the client can spin them.
        self.assertEqual(len(after_document["meshes"][0]["primitives"]),
                         len(before_document["materials"]))
        self.assertEqual(after.meshes, 5)
        self.assertEqual(after.primitives, batched_primitives)
        self.assertEqual(after.triangles, before.triangles)
        for actual, expected in zip(after.bounds_min, before.bounds_min):
            self.assertAlmostEqual(actual, expected, delta=0.02)
        for actual, expected in zip(after.bounds_max, before.bounds_max):
            self.assertAlmostEqual(actual, expected, delta=0.02)

    def test_static_batcher_keeps_carriage_wheels_classifiable(self) -> None:
        """The browser build must load wheels the client can still spin.

        ClassifyCarriageWheels in src/client/local3d/asset_loading.inc finds
        wheel meshes by bounds: every mesh that fits a wheel hub's box spins
        with the road. The static batcher used to merge the wheels into the
        body, which baked them into one static mesh and left the browser
        carriage wheels frozen. The batcher now keeps each wheel separate, so
        every wheel mesh must still fit its hub box after batching, and the
        batched body must never be mistaken for a wheel.
        """
        source = (ROOT / "assets" / "exports" / "glb" /
                  "carriage_base_v01.glb")
        output = Path(self.temporary.name) / source.name
        batch_static_glb(source, output)
        document, _ = parse_glb(output)

        # Wheel hubs as authored in asset_loading.inc: front pair tall at
        # x -1.22, rear pair short at x 1.05, hubs y 0.80/0.62, track z 0.94.
        centers = ((-1.22, 0.80, 0.94), (-1.22, 0.80, -0.94),
                   (1.05, 0.62, 0.94), (1.05, 0.62, -0.94))
        radii = (0.81, 0.81, 0.63, 0.63)
        z_tolerance = 0.18

        classified: dict[int, str] = {}
        for node in document["nodes"]:
            mesh = document["meshes"][node["mesh"]]
            corners_min = [math.inf] * 3
            corners_max = [-math.inf] * 3
            for primitive in mesh["primitives"]:
                position = document["accessors"][
                    primitive["attributes"]["POSITION"]]
                for axis in range(3):
                    corners_min[axis] = min(corners_min[axis],
                                           position["min"][axis])
                    corners_max[axis] = max(corners_max[axis],
                                           position["max"][axis])
            for wheel in range(4):
                center = centers[wheel]
                radius = radii[wheel]
                if (corners_min[0] >= center[0] - radius and
                        corners_max[0] <= center[0] + radius and
                        corners_min[1] >= center[1] - radius and
                        corners_max[1] <= center[1] + radius and
                        corners_min[2] >= center[2] - z_tolerance and
                        corners_max[2] <= center[2] + z_tolerance):
                    classified[wheel] = node["name"]
                    break

        self.assertEqual(sorted(classified), [0, 1, 2, 3])
        for wheel, name in classified.items():
            self.assertTrue(name.startswith("GEO_Wheel_"), name)
        self.assertNotIn("GEO_BATCHED", classified.values())

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
