"""Exercise capture receipts without launching a graphics process."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import zlib

MODULE_PATH = Path(__file__).resolve().parents[1] / 'tools' / 'capture_four_towns.py'
SPEC = importlib.util.spec_from_file_location('four_town_capture', MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
CAPTURE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CAPTURE)


def png(width: int = 320, height: int = 180) -> bytes:
    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack('>I', len(data)) + kind + data +
                struct.pack('>I', zlib.crc32(kind + data)))
    header = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    pixels = (b'\0' + b'\0' * width * 3) * height
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) +
            chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b''))


class FourTownCaptureTests(unittest.TestCase):
    def test_sixteen_distinct_named_positions(self) -> None:
        self.assertEqual(len(CAPTURE.SCENES), 16)
        self.assertEqual(len({(s[0], s[2]) for s in CAPTURE.SCENES}), 16)
        self.assertEqual(len({(s[0], s[3], s[4]) for s in CAPTURE.SCENES}), 16)
        self.assertEqual({s[0] for s in CAPTURE.SCENES}, {0, 1, 2, 3})

    def test_absolute_output_becomes_relative_screenshot_argument(self) -> None:
        # The real screenshot API prefixes cwd, even for an absolute argument.
        with tempfile.TemporaryDirectory(prefix='town capture ') as directory:
            output = Path(directory).resolve()
            commands = []

            def render(command, **kwargs):
                del kwargs
                commands.append(command)
                self.assertFalse(Path(command[5]).is_absolute())
                (Path.cwd() / command[5]).write_bytes(png())

            with patch.object(CAPTURE.subprocess, 'run', side_effect=render):
                receipt = CAPTURE.capture(Path('/unused/client'), output,
                                          CAPTURE.SCENES[0], 'peaceful')
            self.assertEqual(len(commands), 1)
            self.assertEqual((receipt['width'], receipt['height']), (320, 180))
            self.assertEqual(len(receipt['sha256']), 64)
            self.assertTrue((output / receipt['file']).is_file())

    def test_failed_process_cannot_reuse_stale_image(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            image = output / 'thornford-river-crossing-peaceful.png'
            image.write_bytes(png())
            with patch.object(CAPTURE.subprocess, 'run',
                              side_effect=subprocess.CalledProcessError(1, 'client')):
                with self.assertRaises(subprocess.CalledProcessError):
                    CAPTURE.capture(Path('/unused/client'), output,
                                    CAPTURE.SCENES[0], 'peaceful')
            self.assertFalse(image.exists())

    def test_rejects_non_image_and_tiny_capture(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            image = Path(directory) / 'capture.png'
            for data in (b'not an image', png(16, 16)):
                image.write_bytes(data)
                with self.assertRaises(ValueError):
                    CAPTURE.png_receipt(image)


if __name__ == '__main__':
    unittest.main()
