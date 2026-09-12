#!/usr/bin/env python3
"""Signal checks for offline speech finishing."""
import sys
from pathlib import Path
import unittest
import numpy as np
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/audio'))
from voice_finish import finish, active_rms, true_peak


class FinishTests(unittest.TestCase):
    rate = 24000

    def test_silence_and_invalid_input(self):
        y, _ = finish(np.zeros(2400), self.rate)
        self.assertTrue(np.all(y == 0))
        for x in ([], [np.nan], [[0, 0]], [1.1]):
            with self.assertRaises(ValueError):
                finish(x, self.rate)

    def test_peaks_edges_length_and_repeatability(self):
        rng = np.random.default_rng(7)
        x = np.clip(rng.normal(0, .28, self.rate), -.95, .95)
        y, _ = finish(x, self.rate)
        self.assertEqual(len(x), len(y))
        self.assertTrue(np.isfinite(y).all())
        self.assertLessEqual(true_peak(y), 10 ** (-2 / 20) + 1e-8)
        self.assertEqual(y[0], 0)
        self.assertEqual(y[-1], 0)
        np.testing.assert_array_equal(y, finish(x, self.rate)[0])

    def test_compression_reduces_level_gap(self):
        t = np.arange(self.rate * 2) / self.rate
        tone = np.sin(2 * np.pi * 220 * t)
        x = np.concatenate([tone * .08, tone * .65])
        y, _ = finish(x, self.rate)
        low = slice(self.rate, self.rate * 2 - 1000)
        high = slice(self.rate * 3, self.rate * 4 - 1000)
        ratio = lambda z: active_rms(z[high], self.rate) / active_rms(z[low], self.rate)
        self.assertLess(ratio(y), ratio(x) * .8)

    def test_sharp_band_is_softened(self):
        t = np.arange(self.rate) / self.rate
        x = .15 * np.sin(2 * np.pi * 220 * t) + .15 * np.sin(2 * np.pi * 6000 * t)
        y, report = finish(x, self.rate)
        energy = lambda z, hz: abs(np.sum(z * np.exp(-2j * np.pi * hz * t)))
        self.assertLess(energy(y, 6000) / energy(y, 220), .85)
        self.assertLessEqual(report['max_deess_db'], 4)

    def test_rumble_reduction(self):
        t = np.arange(self.rate) / self.rate
        x = .2 * np.sin(2 * np.pi * 20 * t) + .2 * np.sin(2 * np.pi * 220 * t)
        y, _ = finish(x, self.rate)
        energy = lambda z, hz: abs(np.sum(z * np.exp(-2j * np.pi * hz * t)))
        self.assertLess(energy(y, 20) / energy(y, 220), .2)


if __name__ == '__main__':
    unittest.main()
