#!/usr/bin/env python3
"""Check the art crop against the complete scene and a rendered fixture."""
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest

from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "art_check", ROOT / "tools/art/run_art_check.py")
ART = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = ART
SPEC.loader.exec_module(ART)


class ArtViewportTests(unittest.TestCase):
    def test_scene_edges_reach_the_crop(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "scene.png"
            frame = Image.new("RGB", (1280, 760), (255, 0, 255))
            draw = ImageDraw.Draw(frame)
            draw.rectangle((10, 54, 1269, 693), fill=(255, 0, 0),
                           outline=(0, 255, 0), width=8)
            frame.save(path)
            self.assert_complete_scene(ART.crop_world(path), (0, 255, 0))

    def test_right_side_damage_is_measured(self):
        with tempfile.TemporaryDirectory() as directory:
            before = Path(directory) / "before.png"
            after = Path(directory) / "after.png"
            frame = Image.new("RGB", (1280, 760), "black")
            frame.save(before)
            ImageDraw.Draw(frame).rectangle((950, 54, 1269, 693), fill="white")
            frame.save(after)
            difference = ImageChops.difference(ART.crop_world(before),
                                              ART.crop_world(after))
            self.assertIsNotNone(difference.getbbox())

    def assert_complete_scene(self, image, edge_color):
        self.assertEqual(image.size, (630, 320))
        for point in ((0, 0), (629, 0), (0, 319), (629, 319)):
            self.assertEqual(image.getpixel(point), edge_color)
        self.assertEqual(image.getpixel((315, 160)), (255, 0, 0))
        self.assertNotIn((255, 0, 255), ART.image_pixels(image))

    def test_rendered_viewport(self):
        if VIEWPORT_IMAGE is None:
            self.skipTest("pass --viewport-image to check the rendered client layout")
        # raylib GREEN is (0, 228, 48).
        self.assert_complete_scene(ART.crop_world(VIEWPORT_IMAGE), (0, 228, 48))


VIEWPORT_IMAGE = None
if __name__ == "__main__":
    if "--viewport-image" in sys.argv:
        index = sys.argv.index("--viewport-image")
        VIEWPORT_IMAGE = Path(sys.argv[index + 1])
        del sys.argv[index:index + 2]
    unittest.main()
