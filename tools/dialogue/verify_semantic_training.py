"""Check training output, typed v3 rows, and native probe parity."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

KNOWN_FORMATS = {
    'crownless-semantic-ids-v1': ('--semantic-prefix', False),
    'crownless-policy-v2': ('--policy-prefix', True),
    'crownless-meaning-v3': ('--policy-prefix', True),
}


def policy_gate(test, version_two=False):
    records = test.get('records', [])
    count = len(records)
    exact = sum(row.get('exact') is True for row in records)
    if not count or test.get('count') != count or test.get('exact') != exact:
        return False
    if any(not (row.get('valid') and row.get('eos')) for row in records):
        return False
    return exact * 100 >= count * 99 if version_two else exact == count


def _sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _read_rows(path):
    return [json.loads(line) for line in Path(path).read_text().splitlines() if line.strip()]


def _check_v3_rows(run, test, manifest):
    """Rebuild every v3 prefix and teacher choice from the typed input."""
    dataset_path = run / 'test.jsonl'
    dataset_meta = manifest.get('datasets', {}).get('test', {})
    rows = _read_rows(dataset_path)
    errors = []
    if dataset_meta.get('rows') != len(rows):
        errors.append('test dataset row count differs from manifest')
    if dataset_meta.get('sha256') != _sha(dataset_path):
        errors.append('test dataset hash differs from manifest')
    if len(rows) != len(test.get('records', [])):
        errors.append('v3 evaluation does not cover the complete test dataset')
    sys.path.insert(0, str(Path(__file__).parent))
    import meaning
    source = Path(meaning.__file__)
    expected_hash = _sha(source)
    source_hashes = [value for path, value in manifest.get('sources', {}).items()
                     if Path(path).name == 'meaning.py']
    if source_hashes != [expected_hash]:
        errors.append('meaning.py checksum differs from the training manifest')
    for number, (row, record) in enumerate(zip(rows, test.get('records', []))):
        try:
            request = row['input']
            person, heard = request['person'], request['heard']
            choices = meaning.candidates(person, heard)
            expected_index = meaning.preferred(person, heard, choices)
            expected_prefix = meaning.encode_input(person, heard, choices)
            expected_reference = {'choiceindex': expected_index}
            if row.get('prompt', {}).get('format') != meaning.FORMAT:
                errors.append(f'row {number}: prompt format differs from meaning format')
            if row.get('prompt', {}).get('tokens') != expected_prefix:
                errors.append(f'row {number}: prompt is not rebuilt from the typed input')
            if row.get('teacher_index') != expected_index or row.get('teacher_intent') != choices[expected_index]['intent']:
                errors.append(f'row {number}: teacher choice differs from current meaning rules')
            if json.loads(row.get('target_text', '{}')) != expected_reference:
                errors.append(f'row {number}: target reference differs from teacher choice')
            target = [1024 + expected_index]
            if (row.get('choice_count') != len(choices) or row.get('tokens') != expected_prefix + target or
                    row.get('labels') != [-100] * (len(expected_prefix) - 1) + target + [0]):
                errors.append(f'row {number}: training labels are detached from the typed choice')
            if record.get('prefix_ids') != expected_prefix or record.get('reference') != expected_reference:
                errors.append(f'row {number}: evaluation record is detached from its dataset row')
            output = record.get('ids')
            start = expected_prefix.index(1580) + 1
            stop = expected_prefix.index(1281, start)
            allowed = set(expected_prefix[start:stop])
            valid = isinstance(output, list) and len(output) == 1 and output[0] in allowed
            exact = valid and output[0] == 1024 + expected_index
            if record.get('valid') is not valid or record.get('exact') is not exact:
                errors.append(f'row {number}: evaluation validity flags are false')
            if not record.get('eos'):
                errors.append(f'row {number}: v3 choice output has no EOS marker')
        except (KeyError, IndexError, TypeError, ValueError, json.JSONDecodeError) as error:
            errors.append(f'row {number}: meaning reconstruction failed: {error}')
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--timeout', type=float, default=30.0)
    args = parser.parse_args()
    test = json.loads((args.run / 'test-evaluation.json').read_text())
    manifest = json.loads((args.run / 'manifest.json').read_text())
    format_name = manifest.get('format')
    errors = []
    if format_name not in KNOWN_FORMATS:
        errors.append(f'unknown training format {format_name!r}')
        prefix_flag, tolerant = '--semantic-prefix', False
    else:
        prefix_flag, tolerant = KNOWN_FORMATS[format_name]
    if manifest.get('status') != 'complete':
        errors.append(f"training manifest status is {manifest.get('status')!r}")
    exported = _sha(args.run / 'last.ccv2')
    if manifest.get('export_sha256') != exported:
        errors.append('export hash does not match manifest')
    records = test.get('records', [])
    if test.get('count') != len(records):
        errors.append('evaluation count does not match records')
    if manifest.get('datasets', {}).get('test', {}).get('rows') != len(records):
        errors.append('evaluation must cover the complete test dataset')
    if not policy_gate(test, tolerant):
        errors.append(f"policy quality gate failed: {test.get('exact')}/{test.get('count')}")
    if format_name == 'crownless-meaning-v3':
        errors.extend(_check_v3_rows(args.run, test, manifest))
    if errors:
        raise SystemExit('; '.join(errors))
    checked = 0
    failures = []
    for number, row in enumerate(records):
        prefix = ','.join(str(value) for value in row['prefix_ids'])
        try:
            result = subprocess.run(
                [str(args.probe), str(args.run / 'last.ccv2'), prefix_flag, prefix, '--generate'],
                capture_output=True, text=True, timeout=args.timeout, check=False,
            )
        except subprocess.TimeoutExpired as error:
            failures.append({'row': number, 'error': 'probe timeout',
                             'stdout': repr(error.stdout), 'stderr': repr(error.stderr)})
            continue
        output = result.stdout.strip()
        try:
            native = [int(value) for value in output.split()] if output else []
        except ValueError:
            native = None
        if result.returncode != 0 or native != row['ids']:
            failures.append({'row': number, 'returncode': result.returncode,
                             'python': row['ids'], 'native': native,
                             'stdout': result.stdout, 'stderr': result.stderr})
        checked += 1
    parity = {'checked': checked, 'failures': failures}
    (args.run / 'native-parity.json').write_text(json.dumps(parity, indent=2) + '\n')
    if failures:
        raise SystemExit(f"native parity gate failed: {len(failures)} of {len(records)}")
    print(json.dumps({'exact': test['exact'], 'test': test['count'], 'native_checked': checked}))


if __name__ == '__main__':
    main()
