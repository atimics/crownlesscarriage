#!/usr/bin/env python3
"""Check voice isolation, input pins, costs, and checkpoint continuation."""

import importlib.util
import json
import math
import os
from pathlib import Path
import random
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
FOLDER = ROOT / 'tools/audio/paradee'
sys.path.insert(0, str(FOLDER))
import run as pipeline
import runtime


class TrainingTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.settings = pipeline.config('af_heart', 'cpu', 123)

    def tearDown(self):
        self.temporary.cleanup()

    def test_plan_has_no_download_or_output(self):
        process = subprocess.run([sys.executable, str(FOLDER / 'run.py'), 'plan', '--run',
                                  str(self.root / 'voice')], check=True, capture_output=True, text=True)
        plan = json.loads(process.stdout)
        self.assertEqual(list(plan['commands']), list(pipeline.STAGES))
        self.assertEqual(sum(plan['settings']['recipe']['stages'].values()), 68000)
        self.assertFalse((self.root / 'voice').exists())

    def test_voice_language_and_paths(self):
        self.assertEqual(pipeline.config('bf_emma', 'cuda', 42)['language'], 'b')
        for voice in ('../af_heart', 'af_heart;touch x', 'jf_alpha', '', 'af_Heart'):
            with self.assertRaises(ValueError):
                pipeline.config(voice, 'cpu', 42)
        with self.assertRaises(ValueError):
            pipeline.config('af_heart', 'cpu', -1)

    def test_cost_is_an_explicit_scenario(self):
        result = pipeline.estimate(24, 0.89, 30, 0.07)
        self.assertEqual(result['compute_usd'], 21.36)
        self.assertEqual(result['storage_usd_per_month'], 2.1)
        self.assertIn('Assumed', result['basis'])
        for bad in (-1, math.nan, math.inf):
            with self.assertRaises(ValueError):
                pipeline.estimate(bad, 0.89, 30, 0.07)

    def test_lock_stops_a_second_writer(self):
        with pipeline.run_lock(self.root):
            with self.assertRaises(ValueError):
                with pipeline.run_lock(self.root):
                    self.fail('A second process acquired the voice folder')

    def test_changed_output_fails_resume(self):
        artifact = self.root / 'last.pt'
        artifact.write_bytes(b'checkpoint')
        hashes = {'last.pt': pipeline.digest(artifact)}
        pipeline.verify_hashes(self.root, hashes)
        artifact.write_bytes(b'changed')
        with self.assertRaises(ValueError):
            pipeline.verify_hashes(self.root, hashes)

    def test_patch_checks_upstream_shape(self):
        with self.assertRaises(ValueError):
            pipeline.adapt('scripts/train_text.py', 'unexpected source', self.settings)

    def test_dependency_tool_version_is_pinned(self):
        with patch.object(subprocess, 'check_output', return_value='uv 0.1.0'), \
             patch.object(subprocess, 'run') as command:
            with self.assertRaises(ValueError):
                pipeline.install(self.root, self.settings)
            command.assert_not_called()

    @unittest.skipUnless(os.environ.get('PARADEE_SOURCE_ARCHIVE'), 'Set PARADEE_SOURCE_ARCHIVE for pinned source checks')
    def test_real_upstream_preparation_and_drift(self):
        archive = Path(os.environ['PARADEE_SOURCE_ARCHIVE'])
        pipeline.prepare(self.root, self.settings, archive)
        pipeline.verify_sources(self.root, self.settings)
        pipeline.prepare(self.root, self.settings, archive)
        source = self.root / 'training/scripts/train_text.py'
        self.assertIn('cc_restore_rng(ck)', source.read_text())
        self.assertIn('cc_preserve_rng', source.read_text())
        decoder = (self.root / 'training/scripts/train_decoder.py').read_text()
        self.assertIn('cc_restore_rng(ck)', decoder)
        self.assertIn('cc_save(ck, last)', decoder)
        british = pipeline.config('bf_emma', 'cpu', 123)
        with self.assertRaises(ValueError):
            pipeline.prepare(self.root, british, archive)
        source.write_text(source.read_text() + '\n# edit\n')
        with self.assertRaises(ValueError):
            pipeline.verify_sources(self.root, self.settings)

    @unittest.skipUnless(os.environ.get('PARADEE_SOURCE_ARCHIVE'), 'Set PARADEE_SOURCE_ARCHIVE for pinned source checks')
    def test_corrupt_source_is_rejected(self):
        settings = json.loads(json.dumps(self.settings))
        settings['recipe']['source_sha256']['scripts/train_text.py'] = '0' * 64
        with self.assertRaises(ValueError):
            pipeline.prepare(self.root, settings, Path(os.environ['PARADEE_SOURCE_ARCHIVE']))
        self.assertFalse((self.root / 'run.json').exists())


@unittest.skipUnless(importlib.util.find_spec('torch') and importlib.util.find_spec('numpy'),
                     'Torch and NumPy enable the real checkpoint continuation check')
class RandomStateTests(unittest.TestCase):
    def test_checkpoint_continues_the_same_cpu_training(self):
        import numpy as np
        import torch
        torch.set_num_threads(1)
        torch.use_deterministic_algorithms(True)

        def new():
            model = torch.nn.Sequential(torch.nn.Linear(3, 4), torch.nn.Dropout(0.2), torch.nn.Linear(4, 1))
            return model, torch.optim.AdamW(model.parameters(), lr=0.01)

        def seed():
            random.seed(31)
            np.random.seed(31)
            torch.manual_seed(31)

        @runtime.preserve_rng
        def evaluate():
            random.random()
            np.random.rand()
            torch.rand(3)

        def step(model, optimizer):
            x = torch.randn(4, 3) * random.random() + float(np.random.rand())
            optimizer.zero_grad()
            model(x).square().mean().backward()
            optimizer.step()
            evaluate()

        seed()
        full, optimizer = new()
        for _ in range(10):
            step(full, optimizer)
        seed()
        partial, optimizer = new()
        for _ in range(5):
            step(partial, optimizer)
        with tempfile.TemporaryDirectory() as folder:
            checkpoint = Path(folder) / 'last.pt'
            runtime.save(dict(model=partial.state_dict(), opt=optimizer.state_dict(), step=5), checkpoint)
            resumed, optimizer = new()
            saved = torch.load(checkpoint, map_location='cpu', weights_only=True)
            resumed.load_state_dict(saved['model'])
            optimizer.load_state_dict(saved['opt'])
            runtime.restore_rng(saved)
            for _ in range(5):
                step(resumed, optimizer)
            for left, right in zip(full.parameters(), resumed.parameters()):
                self.assertTrue(torch.equal(left, right))
            self.assertFalse(checkpoint.with_suffix('.pt.tmp').exists())


if __name__ == '__main__':
    unittest.main()
