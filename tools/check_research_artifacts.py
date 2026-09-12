#!/usr/bin/env python3
"""Check new Git blobs under research directories against a PR size budget."""
import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import subprocess
import sys

MIB = 1024 * 1024
ROOTS = ('docs/experiments/', 'docs/reviews/')


@dataclass(frozen=True)
class Blob:
    path: str
    oid: str
    size: int


def read_tree(repo, revision):
    tree = subprocess.check_output(
        ['git', '-C', str(repo), 'rev-parse', '--verify', '--end-of-options', revision + '^{tree}'],
        text=True).strip()
    output = subprocess.check_output(['git', '-C', str(repo), 'ls-tree', '-r', '-l', '-z', tree])
    entries = []
    for row in output.split(b'\0'):
        if not row:
            continue
        metadata, path = row.split(b'\t', 1)
        _, kind, oid, size = metadata.split()
        if kind == b'blob':
            entries.append(Blob(path.decode('utf-8', 'surrogateescape'), oid.decode(), int(size)))
    return entries


def audit(before, after, max_file_bytes=MIB, max_added_bytes=2 * MIB):
    existing = {item.oid for item in before}
    added = {}
    for item in sorted(after, key=lambda item: item.path):
        if item.path.startswith(ROOTS) and item.oid not in existing:
            added.setdefault(item.oid, []).append(item)
    files = [{'paths': [item.path for item in items], 'oid': oid, 'bytes': items[0].size}
             for oid, items in added.items()]
    total = sum(item['bytes'] for item in files)
    errors = []
    for item in files:
        if item['bytes'] > max_file_bytes:
            errors.append(f"{item['paths'][0]}: {item['bytes']} bytes exceeds the {max_file_bytes}-byte file budget")
    if total > max_added_bytes:
        errors.append(f'{total} new research bytes exceeds the {max_added_bytes}-byte PR budget')
    return {'new_blob_count': len(files), 'new_bytes': total, 'file_limit': max_file_bytes,
            'added_limit': max_added_bytes, 'files': files, 'errors': errors}


def positive(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError('Use a positive byte count.')
    return number


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base', required=True, help='Base Git revision or tree')
    parser.add_argument('--head', default='HEAD', help='Proposed Git revision or tree')
    parser.add_argument('--repo', type=Path, default=Path('.'))
    parser.add_argument('--max-file-bytes', type=positive, default=MIB)
    parser.add_argument('--max-added-bytes', type=positive, default=2 * MIB)
    parser.add_argument('--json', action='store_true')
    args = parser.parse_args()
    try:
        report = audit(read_tree(args.repo, args.base), read_tree(args.repo, args.head),
                       args.max_file_bytes, args.max_added_bytes)
    except subprocess.CalledProcessError:
        parser.error('Fetch the requested base and head revisions, then run the check again.')
    if args.json:
        print(json.dumps(report, indent=2))
    else:
        print(f"Research artifacts: {report['new_blob_count']} new blobs, {report['new_bytes']} bytes")
        for error in report['errors']:
            print(error, file=sys.stderr)
    return 1 if report['errors'] else 0


if __name__ == '__main__':
    sys.exit(main())
