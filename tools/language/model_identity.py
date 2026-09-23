#!/usr/bin/env python3
"""Publish and check the reviewed identity of the shipped conversation core.

Issue 809 asks for the exact model, tokenizer, grammar, runtime and evaluator
identities before any held-out comparison runs. This emits one receipt from the
actual artifacts and verifies later runs still use them. A changed file or a
changed model header fails the check; a stale README then cannot pass as the
reviewed artifact. See EVALUATION.md.

    python3 tools/language/model_identity.py --write   # freeze the receipt
    python3 tools/language/model_identity.py --check    # verify, default
    python3 tools/language/model_identity.py --print     # show, no write
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RECEIPT = ROOT / 'assets/language/model_identity_receipt.json'
VERSION = 1

# Artifacts whose identity bounds a comparison. Paths are relative to the repo
# root so the receipt travels with the tree.
ARTIFACTS = {
    'model': 'assets/language/core.ccv2',
    'tokenizer': 'assets/language/tokenizer.json',
    'grammar_source': 'tools/data/core_account_rules.json',
    'native_reference': 'tests/data/core-model-reference.json',
}
RUNTIME = [
    'src/story/cc_core_model.c',
    'src/story/cc_core_model.h',
    'src/story/cc_core_model_tables.inc',
]
COMPILERS = [
    'tools/language/compile_model.py',
    'tools/compile_core_accounts.py',
]
EVALUATORS = [
    'tools/language/measure_field_membership.py',
    'tools/language/measure_memory_membership.py',
    'tools/language/field_calibration.json',
    'tools/language/model_identity.py',
    'tests/language_membership_tests.py',
    'tests/model_identity_tests.py',
    'tools/language/EVALUATION.md',
]


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def parse_header(raw):
    """Return the .ccv2 header without trusting any of its declared hashes."""
    if raw[:4] != b'CCOR':
        raise ValueError('not a CCOR model container')
    size, = struct.unpack_from('<I', raw, 8)
    header = json.loads(raw[12:12 + size])
    payload = raw[12 + size:]
    if hashlib.sha256(payload).hexdigest() != header['payload_sha256']:
        raise ValueError('payload hash does not match the header')
    parameters = 0
    for tensor in header['tensors']:
        count = 1
        for extent in tensor['shape']:
            count *= extent
        parameters += count
    return {
        'container_version': header['version'],
        'mode': header['mode'],
        'parameters': parameters,
        'tensors': len(header['tensors']),
        'meanings': len(header['meaning_ids']),
        'event_kinds': len(header.get('kind_ids', {})),
        'config': header['config'],
        'payload_sha256': header['payload_sha256'],
        'rules_sha256': header['rules_sha256'],
        'tokenizer_sha256': header['tokenizer_sha256'],
    }


def _files(root, relatives):
    return [{'path': rel, 'sha256': sha256(root / rel)} for rel in relatives]


def build_receipt(root=ROOT):
    for paths in (ARTIFACTS.values(), RUNTIME, COMPILERS, EVALUATORS):
        for rel in paths:
            if not (root / rel).is_file():
                raise FileNotFoundError(rel)
    model_path = root / ARTIFACTS['model']
    raw = model_path.read_bytes()
    receipt = {
        'version': VERSION,
        'issue': 809,
        'artifacts': {name: {'path': rel, 'sha256': sha256(root / rel)}
                      for name, rel in ARTIFACTS.items()},
        'model_bytes': len(raw),
        'model': parse_header(raw),
        'runtime': _files(root, RUNTIME),
        'compilers': _files(root, COMPILERS),
        'evaluators': _files(root, EVALUATORS),
    }
    reference = json.loads((root / ARTIFACTS['native_reference']).read_text())
    receipt['native_reference'] = {
        'model_sha256': reference['model_sha256'],
        'sentences': len(reference['cases']),
        'tokenizer_cases': len(reference['tokenizer']),
        'vocabulary': len(reference['vocabulary']),
        'zero_sources': reference['zero_sources'],
    }
    return receipt


def check_receipt(root=ROOT, receipt=None):
    """Return a list of human-readable mismatches; empty means exact."""
    receipt = receipt or json.loads(RECEIPT.read_text())
    if receipt.get('version') != VERSION:
        return [f"receipt version {receipt.get('version')} != {VERSION}"]
    current = build_receipt(root)
    if current == receipt:
        return []
    mismatches = []
    for key in sorted(set(current) | set(receipt)):
        if current.get(key) != receipt.get(key):
            mismatches.append(f'{key}: stored={receipt.get(key)!r} current={current.get(key)!r}')
    return mismatches


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--write', action='store_true', help='Freeze the receipt')
    p.add_argument('--check', action='store_true', help='Verify the receipt (default)')
    p.add_argument('--print', dest='show', action='store_true', help='Print, do not write')
    args = p.parse_args()
    if args.write:
        RECEIPT.write_text(json.dumps(build_receipt(), indent=2, sort_keys=True) + '\n')
        print(f'wrote {RECEIPT.relative_to(ROOT)}')
        return 0
    if args.show:
        print(json.dumps(build_receipt(), indent=2, sort_keys=True))
        return 0
    mismatches = check_receipt()
    if mismatches:
        print('model identity receipt does not match the tree:', file=sys.stderr)
        for line in mismatches:
            print('  ' + line, file=sys.stderr)
        return 1
    receipt = build_receipt()
    print(f"model {receipt['artifacts']['model']['sha256'][:12]} "
          f"{receipt['model']['parameters']} parameters "
          f"{receipt['model']['meanings']} meanings "
          f"({receipt['native_reference']['sentences']} native reference sentences)")
    return 0


if __name__ == '__main__':
    sys.exit(main())