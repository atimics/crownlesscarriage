#!/usr/bin/env python3
"""Regression cases for the issue 809 model identity receipt."""
import copy
import hashlib
import json
import struct
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/language'))
from model_identity import (ARTIFACTS, RECEIPT, build_receipt, check_receipt,
                            parse_header, sha256)


class ReceiptTests(unittest.TestCase):
    def test_receipt_matches_the_tree(self):
        stored = json.loads(RECEIPT.read_text())
        self.assertEqual(check_receipt(ROOT), [])
        self.assertEqual(build_receipt(ROOT), stored)

    def test_receipt_pins_the_reviewed_artifact(self):
        receipt = build_receipt(ROOT)
        self.assertEqual(receipt['artifacts']['model']['sha256'],
                         '7d1c3cd5e46738ba7cd60673e58f05e12535ac46204b07a8449f41a6fcb936c9')
        self.assertEqual(receipt['artifacts']['tokenizer']['sha256'],
                         receipt['model']['tokenizer_sha256'])
        self.assertEqual(receipt['model']['parameters'], 4_945_153)
        self.assertEqual(receipt['model']['meanings'], 61)
        self.assertEqual(receipt['model']['mode'], 'conversation')
        self.assertEqual(receipt['model']['config']['dim'], 192)
        self.assertEqual(receipt['model']['config']['layers'], 8)
        self.assertEqual(receipt['model']['config']['vocab'], 4096)

    def test_official_readme_claim_is_not_the_shipped_artifact(self):
        # Issue 809: the language README described an older checkpoint. The
        # receipt must carry the shipped file, not the PR 37 model.
        readme = (ROOT / 'assets/language/README.md').read_text()
        self.assertIn('7d1c3cd5e46738ba7cd60673e58f05e12535ac46204b07a8449f41a6fcb936c9', readme)
        self.assertNotIn('244a809aba71679efe129495bb981ca8d7fbe0d9012948a5f88731dabcb9c48b', readme)

    def test_drift_is_detected(self):
        stored = json.loads(RECEIPT.read_text())
        for mutate in (
            lambda r: r['artifacts']['model'].__setitem__('sha256', '0' * 64),
            lambda r: r['model'].__setitem__('parameters', r['model']['parameters'] + 1),
            lambda r: r['evaluators'][0].__setitem__('sha256', '0' * 64),
            lambda r: r.__setitem__('version', 99),
        ):
            broken = copy.deepcopy(stored)
            mutate(broken)
            self.assertNotEqual(check_receipt(ROOT, broken), [])

    def test_header_hash_is_recomputed(self):
        raw = (ROOT / ARTIFACTS['model']).read_bytes()
        size, = struct.unpack_from('<I', raw, 8)
        header = json.loads(raw[12:12 + size])
        forged = raw[:12 + size] + b'x' + raw[13 + size:]
        with self.assertRaises(ValueError):
            parse_header(forged)
        self.assertEqual(parse_header(raw)['payload_sha256'], header['payload_sha256'])

    def test_missing_artifact_is_reported(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(FileNotFoundError):
                build_receipt(Path(tmp))


if __name__ == '__main__':
    unittest.main()