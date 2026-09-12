"""Local DSP checks; run with tools/audio/requirements-effects.txt installed."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
import wave

import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/audio'))
from goblin_voice import process, render
from goblin_bass import carrier, vocode


class GoblinVoiceTests(unittest.TestCase):
    def test_pitch_layers_and_duration(self):
        rate = 24000
        dry = 0.4 * np.sin(2 * np.pi * 240 * np.arange(rate) / rate)
        wet = process(dry, rate)
        self.assertEqual(len(wet), len(dry))
        self.assertTrue(np.isfinite(wet).all())
        self.assertLessEqual(np.max(np.abs(wet)), 0.82)
        self.assertEqual(wet[0], 0)
        self.assertEqual(wet[-1], 0)
        spectrum = np.abs(np.fft.rfft(wet))
        self.assertTrue(252 <= np.argmax(spectrum) <= 257)
        self.assertGreater(np.max(spectrum[177:183]), np.max(spectrum) * 0.03)
        np.testing.assert_array_equal(wet, process(dry, rate))
        self.assertGreater(np.max(np.abs(process(dry, rate, 'priest') - wet)), 0.01)

    def test_bass_vocoder(self):
        rate = 24000
        dry = np.zeros(rate)
        self.assertFalse(np.any(vocode(dry, rate)))
        dry[6000:18000] = 0.4 * np.sin(2 * np.pi * 240 * np.arange(12000) / rate)
        wet = vocode(dry, rate)
        self.assertEqual(len(wet), len(dry))
        self.assertTrue(np.isfinite(wet).all())
        self.assertGreater(np.max(np.abs(wet)), 0.01)
        self.assertFalse(np.any(wet[:6000]))
        np.testing.assert_array_equal(carrier(rate, rate), carrier(rate, rate))

    def test_bass_speed_and_bounds(self):
        rate = 24000
        dry = 0.4 * np.sin(2 * np.pi * 240 * np.arange(rate) / rate)
        fast = process(dry, rate, 'bass')
        normal = process(dry, rate, 'bass', speed=1)
        self.assertEqual(len(fast), rate // 2)
        self.assertEqual(len(normal), rate)
        self.assertLessEqual(np.max(np.abs(fast)), 0.78)
        self.assertEqual(fast[0], 0)
        self.assertEqual(fast[-1], 0)
        np.testing.assert_array_equal(fast, process(dry, rate, 'bass'))
        self.assertFalse(np.any(process(np.zeros(rate), rate, 'bass')))
        for speed in (0, float('nan'), 4):
            with self.assertRaises(ValueError):
                process(dry, rate, 'bass', speed=speed)
        for samples in ([], [float('nan')] * 2400, [2.] * 2400, np.zeros((2400, 2))):
            with self.assertRaises(ValueError):
                vocode(samples, rate)

    def test_silence_and_invalid_input(self):
        self.assertFalse(np.any(process(np.zeros(2400), 24000)))
        for samples in ([], [float('nan')] * 2400, [2.] * 2400, np.zeros((2400, 2))):
            with self.assertRaises(ValueError):
                process(samples, 24000)

    def test_render_receipt_and_master(self):
        with tempfile.TemporaryDirectory() as folder:
            master = Path(folder) / 'dry.wav'
            output = Path(folder) / 'wet.wav'
            with wave.open(str(master), 'wb') as audio:
                audio.setparams((1, 2, 24000, 0, 'NONE', 'not compressed'))
                audio.writeframes((8000 * np.sin(np.arange(4800) * 0.1)).astype('<i2').tobytes())
            original = master.read_bytes()
            receipt = render(master, output)
            self.assertEqual(master.read_bytes(), original)
            with wave.open(str(output)) as audio:
                self.assertEqual(audio.getnframes(), 4800)
                self.assertEqual(audio.getframerate(), 24000)
            self.assertEqual(json.loads(output.with_suffix('.json').read_text()), receipt)
            bass = render(master, output, 'bass')
            self.assertEqual(bass['speech_speed'], 2.0)
            self.assertEqual(bass['style'], 'hrakhor-bass-v2')
            self.assertEqual(bass['frames'], 2400)
            self.assertIn('bass_renderer_sha256', bass)
            self.assertEqual(master.read_bytes(), original)
            with self.assertRaises(ValueError):
                render(master, master)


if __name__ == '__main__':
    unittest.main()
