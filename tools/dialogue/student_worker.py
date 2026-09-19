#!/usr/bin/env python3
"""Run an experimental participant checkpoint as one persistent person."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from paired_participants import validate_turn
from participant_training import NativeTokenizer, build_prompt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe', type=Path, required=True)
    parser.add_argument('--model', type=Path, required=True,
                        help='Checkpoint trained for crownless-person-v1 input and JSON turns')
    args = parser.parse_args()
    tokenizer = NativeTokenizer(args.probe)
    actor = None
    for line in sys.stdin:
        request = json.loads(line)
        current = request['participant']['self']['id']
        if actor is not None and actor != current:
            raise ValueError('student session changed person')
        actor = current
        prompt = build_prompt(request, tokenizer)
        # The outer paired runner retains stderr and failed output receipts.
        print(json.dumps({'speaker_id': actor, 'prompt': prompt}), file=sys.stderr, flush=True)
        try:
            result = subprocess.run([str(tokenizer.probe), str(args.model.resolve()),
                '--participant-prefix', prompt['text'], '--generate'],
                capture_output=True, timeout=45)
        except subprocess.TimeoutExpired as error:
            print(json.dumps({'timeout': True, 'stdout_hex': (error.stdout or b'').hex(),
                              'stderr_hex': (error.stderr or b'').hex()}),
                  file=sys.stderr, flush=True)
            raise
        print(json.dumps({'returncode': result.returncode, 'stdout_hex': result.stdout.hex(),
                          'stderr_hex': result.stderr.hex()}), file=sys.stderr, flush=True)
        if result.returncode:
            raise RuntimeError('participant generation failed')
        output = validate_turn(json.loads(result.stdout.decode('utf-8')), request['participant'])
        print(json.dumps(output, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()
