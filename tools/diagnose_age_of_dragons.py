#!/usr/bin/env python3
"""Explain failed validation predicates using a temporary diagnostic build."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def instrument(source):
    checks = [
        ('if (settlement->population < 0 || !valid_ruin ||',
         'SetError(error, error_capacity,\n                         "Settlement service state is invalid.");'),
        ('if (CcIdKind(situation->id) != CC_ENTITY_SITUATION ||',
         'SetError(error, error_capacity, "Situation data is invalid.");'),
    ]
    for start_text, error_text in checks:
        start = source.index(start_text)
        end = source.index(error_text, start)
        condition = source[start + 4:source.rindex(') {', start, end)]
        reports = []
        for expression in condition.split('||'):
            expression = ' '.join(expression.split())
            reports.append(f'if ({expression}) (void)fprintf(stderr, "%s\\n", {json.dumps(expression)});')
        if 'settlement' in start_text:
            reports.append(r'''(void)fprintf(stderr, "town=%s population=%d security=%d prosperity=%d services=%u project=%d project_days=%d\n", settlement->name, settlement->population, settlement->security, settlement->prosperity, settlement->service_mask, (int)settlement->service_project, settlement->service_project_days);''')
        else:
            reports.append(r'''(void)fprintf(stderr, "situation_kind=%d status=%d target_kind=%d target_id=%llu\n", (int)situation->kind, (int)situation->status, (int)target_kind, (unsigned long long)situation->target_id);''')
        source = source[:end] + '\n'.join(reports) + '\n' + source[end:]
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = args.source_root.resolve()
    manifest = json.loads(args.manifest.read_text())
    original = (root / 'src/sim/cc_sim.c').read_text()
    failures = [r for r in manifest['runs'] if not r['passed']]
    report = dict(source=manifest['source'], original_source_sha256=hashlib.sha256(original.encode()).hexdigest(),
                  method='The temporary build prints the true clauses of two validation checks. Simulation updates are unchanged.', runs=[])
    with tempfile.TemporaryDirectory(prefix='age-diagnostic-') as tmp:
        temporary = Path(tmp)
        debug_source = temporary / 'cc_sim.c'
        debug_source.write_text(instrument(original))
        binary = temporary / 'metrics'
        subprocess.run(['cc', '-O3', '-std=c17', '-I'+str(root/'src'), '-I'+str(root/'src/sim'),
                        str(debug_source), str(root/'tools/sim_metrics.c'),
                        str(args.build.resolve()/'libcrownless_sim.a'), '-lm', '-o', str(binary)], check=True)
        report['diagnostic_binary_sha256'] = hashlib.sha256(binary.read_bytes()).hexdigest()
        for run in failures:
            result = subprocess.run([str(binary), '--seed', str(run['seed']), '--years', str(manifest['years']),
                                     '--final-only'], capture_output=True, text=True, check=False)
            original_error = run['error'].split('\n')[0]
            reproduced = result.returncode != 0 and original_error in result.stderr
            report['runs'].append(dict(seed=run['seed'], reproduced=reproduced,
                                       returncode=result.returncode, diagnostic=result.stderr.strip()))
            print(f"seed {run['seed']}: {result.stderr.strip()}", flush=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    return int(any(not r['reproduced'] for r in report['runs']))


if __name__ == '__main__':
    raise SystemExit(main())
