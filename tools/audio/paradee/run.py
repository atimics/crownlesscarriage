#!/usr/bin/env python3
"""Prepare and run one pinned Paradee voice training recipe."""

import argparse
from contextlib import contextmanager
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tarfile
import time
from urllib.request import urlopen

HERE = Path(__file__).resolve().parent
RECIPE = HERE / 'recipe.json'
STAGES = ('corpus', 'text', 'decoder', 'gan10', 'gan3', 'export')


def digest(path):
    value = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            value.update(block)
    return value.hexdigest()


def write_json(path, value):
    path = Path(path)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + '\n')
    temporary.replace(path)


def config(voice, device, seed):
    if not re.fullmatch(r'[ab][fm]_[a-z]+', voice):
        raise ValueError('Choose an American or British English Kokoro voice ID')
    if device not in ('cpu', 'mps', 'cuda') or not 0 <= seed <= 2**32 - 1:
        raise ValueError('Choose a supported device and a 32-bit seed')
    recipe = json.loads(RECIPE.read_text())
    return dict(recipe=recipe, voice=voice, language=voice[0], device=device,
                seed=seed, runner_sha256=digest(HERE / 'run.py'),
                runtime_sha256=digest(HERE / 'runtime.py'))


def commands(settings):
    steps = settings['recipe']['stages']
    device = ['--device', settings['device']]
    return {
        'corpus': [['gen_teacher.py', str(settings['recipe']['sentences'])], ['regen_audio.py']],
        'text': [['train_text.py', 's+mlp', '--steps', str(steps['text']), *device]],
        'decoder': [['train_decoder.py', 'A', '--steps', str(steps['decoder']), *device]],
        'gan10': [['train_decoder.py', 'A', '--gan', '--mel-weight', '10', '--init-from',
                   'checkpoints/dec_A/last.pt', '--steps', str(steps['gan10']), '--bs', '8',
                   '--tag', '_gan10', *device]],
        'gan3': [['train_decoder.py', 'A', '--gan', '--mel-weight', '3', '--init-from',
                  'checkpoints/dec_A_gan10/last.pt', '--steps', str(steps['gan3']), '--bs', '8',
                  '--shards', '8', '--tag', '_gan3', *device]],
        'export': [['export_paradee.py']],
    }


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f'Upstream source changed at {old!r}')
    return text.replace(old, new)


def adapt(name, text, settings):
    """Small checked edits; the rest of the published training recipe stays intact."""
    if name == 'scripts/gen_teacher.py':
        text = replace_once(text, 'SHARD = 500; DEV = "mps"',
                            f'SHARD = 500; DEV = {settings["device"]!r}')
        text = replace_once(text, 'r = run(ps);',
                            f'torch.manual_seed({settings["seed"]} + sh + len(rows)); r = run(ps);')
        text = replace_once(text, 'torch.save(rows, path)', 'cc_save(rows, path)')
    if name == 'scripts/train_text.py':
        text = replace_once(text, '@torch.no_grad()\ndef evaluate',
                            '@torch.no_grad()\n@cc_preserve_rng\ndef evaluate')
        text = replace_once(text, 'print(f"resumed at step {step}", flush=True)',
                            'cc_restore_rng(ck); print(f"resumed at step {step}", flush=True)')
        text = replace_once(text,
                            'torch.save({"model": model.state_dict(), "opt": opt.state_dict(), "step": step}, last)',
                            'cc_save({"model": model.state_dict(), "opt": opt.state_dict(), "step": step}, last)')
    if name == 'scripts/train_decoder.py':
        text = replace_once(text, '@torch.no_grad()\ndef evaluate',
                            '@torch.no_grad()\n@cc_preserve_rng\ndef evaluate')
        text = replace_once(text,
                            'if A.gan and "D" in ck: D.load_state_dict(ck["D"]); optD.load_state_dict(ck["optD"])',
                            'if A.gan and "D" in ck: D.load_state_dict(ck["D"]); optD.load_state_dict(ck["optD"])\n    cc_restore_rng(ck)')
        text = replace_once(text, 'torch.save(ck, last)', 'cc_save(ck, last)')
    if name.endswith('.py'):
        compile(text, name, 'exec')
    return text


@contextmanager
def run_lock(root):
    """Only one process may change this voice's run folder."""
    import fcntl
    root.mkdir(parents=True, exist_ok=True)
    with (root / '.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as error:
            raise ValueError('This voice run is active in another process') from error
        yield


def download(url, destination, expected=None):
    destination = Path(destination)
    if destination.exists() and expected and digest(destination) == expected:
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + '.download')
    try:
        with urlopen(url, timeout=60) as source, temporary.open('wb') as target:
            shutil.copyfileobj(source, target)
        if expected and digest(temporary) != expected:
            raise ValueError(f'Download checksum differs for {destination.name}')
        temporary.replace(destination)
    finally:
        temporary.unlink(missing_ok=True)


def prepare(root, settings, archive=None):
    receipt = root / 'run.json'
    if receipt.exists():
        if json.loads(receipt.read_text()) != settings:
            raise ValueError('Run settings changed; choose a new run folder')
        verify_sources(root, settings)
        return
    training = root / 'training'
    if training.exists():
        raise ValueError('Use a fresh run folder after an incomplete preparation')
    if archive is None:
        archive = root / 'source.tar.gz'
        download('https://codeload.github.com/sahilmahendrakar/paradee/tar.gz/' +
                 settings['recipe']['upstream_commit'], archive)
    expected = settings['recipe']['source_sha256']
    files = {}
    notices = {}
    with tarfile.open(archive, 'r:gz') as bundle:
        for member in bundle:
            relative = member.name.partition('/')[2]
            if relative in settings['recipe']['notice_sha256'] and member.isfile():
                data = bundle.extractfile(member).read()
                if hashlib.sha256(data).hexdigest() != settings['recipe']['notice_sha256'][relative]:
                    raise ValueError(f'Notice checksum differs for {relative}')
                notices[relative] = data
            parts = member.name.split('/', 2)
            if len(parts) != 3 or parts[1] != 'training' or not member.isfile():
                continue
            name = parts[2]
            if name not in expected or member.size > 8 * 1024 * 1024:
                raise ValueError(f'Unexpected source file: {name}')
            data = bundle.extractfile(member).read()
            if name in files or hashlib.sha256(data).hexdigest() != expected[name]:
                raise ValueError(f'Source checksum differs for {name}')
            files[name] = data
    if files.keys() != expected.keys():
        raise ValueError('The archive has an incomplete training source tree')
    if notices.keys() != settings['recipe']['notice_sha256'].keys():
        raise ValueError('The archive needs the upstream license and README')
    for name, data in files.items():
        path = training / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(adapt(name, data.decode('utf-8'), settings))
    for name, data in notices.items():
        path = root / 'notices' / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    write_json(root / 'sources.json', {name: digest(training / name) for name in expected})
    write_json(receipt, settings)


def verify_sources(root, settings):
    verify_hashes(root / 'notices', settings['recipe']['notice_sha256'])
    hashes = json.loads((root / 'sources.json').read_text())
    if hashes.keys() != settings['recipe']['source_sha256'].keys():
        raise ValueError('Prepared source list changed')
    for name, expected in hashes.items():
        if digest(root / 'training' / name) != expected:
            raise ValueError(f'Prepared source changed: {name}')


def python_path(root):
    return root / 'training/.venv/bin/python'


def install(root, settings):
    version = subprocess.check_output(['uv', '--version'], text=True).split()[1]
    if version != settings['recipe']['uv']:
        raise ValueError(f'Use uv {settings["recipe"]["uv"]} for this recipe')
    subprocess.run(['uv', 'sync', '--frozen', '--python', settings['recipe']['python']],
                   cwd=root / 'training', check=True)
    wheel = root / 'downloads/en_core_web_sm-3.8.0-py3-none-any.whl'
    pin = settings['recipe']['pronunciation_wheel']
    download(pin['url'], wheel, pin['sha256'])
    subprocess.run(['uv', 'pip', 'install', '--python', str(python_path(root)),
                    '--no-deps', str(wheel)], check=True)
    teacher = root / 'teacher'
    for name in ('config.json', 'kokoro-v1_0.pth', f'voices/{settings["voice"]}.pt'):
        path = teacher / name
        download('https://huggingface.co/' + settings['recipe']['teacher_repo'] + '/resolve/' +
                 settings['recipe']['teacher_revision'] + '/' + name, path)
    write_json(root / 'teacher.json', {str(path.relative_to(teacher)): digest(path)
                                     for path in sorted(teacher.rglob('*')) if path.is_file()})


def outputs(root, stage):
    training = root / 'training'
    patterns = {
        'corpus': ('data/teacher/shard_*.pt',),
        'text': ('checkpoints/text_s+mlp/last.pt',),
        'decoder': ('checkpoints/dec_A/last.pt',),
        'gan10': ('checkpoints/dec_A_gan10/last.pt',),
        'gan3': ('checkpoints/dec_A_gan3/last.pt',),
        'export': ('models/paradee/paradee.onnx', 'models/paradee/paradee_int8.onnx',
                   'models/paradee/config.json', 'models/paradee/validation.json'),
    }
    files = []
    for pattern in patterns[stage]:
        matches = sorted(training.glob(pattern))
        if not matches:
            raise ValueError(f'Stage {stage} needs its output {pattern}')
        files.extend(matches)
    return {str(path.relative_to(root)): digest(path) for path in files}


def verify_hashes(root, hashes):
    for name, expected in hashes.items():
        path = root / name
        if not path.is_file() or digest(path) != expected:
            raise ValueError(f'Run artifact changed: {name}')


def execute(root, settings, until):
    if not python_path(root).is_file():
        raise ValueError('Run install first to prepare the training environment')
    verify_hashes(root / 'teacher', json.loads((root / 'teacher.json').read_text()))
    state_path = root / 'state.json'
    state = json.loads(state_path.read_text()) if state_path.exists() else {}
    env = dict(os.environ, PYTHONHASHSEED=str(settings['seed']), CUBLAS_WORKSPACE_CONFIG=':4096:8',
               PYTORCH_ENABLE_MPS_FALLBACK='1', HF_HUB_OFFLINE='1', TRANSFORMERS_OFFLINE='1')
    for stage in STAGES:
        if stage in state:
            verify_hashes(root, state[stage]['outputs'])
        else:
            start = time.monotonic()
            for index, command in enumerate(commands(settings)[stage]):
                log_path = root / f'{stage}-{index}.log'
                argv = [str(python_path(root)), str(HERE / 'runtime.py'), str(root), *command]
                print('Running ' + stage + ': ' + ' '.join(command), flush=True)
                with log_path.open('ab') as log:
                    subprocess.run(argv, cwd=root / 'training', env=env, stdout=log,
                                   stderr=subprocess.STDOUT, check=True)
            state[stage] = dict(seconds=time.monotonic() - start, outputs=outputs(root, stage))
            write_json(state_path, state)
        if stage == until:
            break
    print(json.dumps(state, indent=2))


def estimate(hours, rate, storage_gb, storage_rate):
    if any(not math.isfinite(n) or n < 0 for n in (hours, rate, storage_gb, storage_rate)):
        raise ValueError('Cost inputs must be finite and positive or zero')
    return dict(currency='USD', assumed_billable_hours=hours, hourly_rate=rate,
                compute_usd=round(hours * rate, 2),
                storage_usd_per_month=round(storage_gb * storage_rate, 2),
                basis='Assumed hours; use measured full-run time to update this figure')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('plan', 'prepare', 'install', 'run', 'cost'))
    parser.add_argument('--voice', default='af_heart')
    parser.add_argument('--device', choices=('cpu', 'mps', 'cuda'), default='cpu')
    parser.add_argument('--seed', type=int, default=20261018)
    parser.add_argument('--run', type=Path, default=Path('out/paradee/af_heart'))
    parser.add_argument('--archive', type=Path, help='Use a previously downloaded source archive')
    parser.add_argument('--until', choices=STAGES, default='export')
    parser.add_argument('--hours', type=float, default=24)
    parser.add_argument('--hourly-rate', type=float, default=0.89)
    parser.add_argument('--storage-gb', type=float, default=30)
    parser.add_argument('--storage-rate', type=float, default=0.07)
    args = parser.parse_args()
    try:
        settings = config(args.voice, args.device, args.seed)
        if args.action == 'cost':
            print(json.dumps(estimate(args.hours, args.hourly_rate, args.storage_gb,
                                      args.storage_rate), indent=2))
            return
        if args.action == 'plan':
            print(json.dumps(dict(settings=settings, commands=commands(settings)), indent=2))
            return
        root = args.run.resolve()
        with run_lock(root):
            if args.action == 'prepare':
                prepare(root, settings, args.archive)
            else:
                if json.loads((root / 'run.json').read_text()) != settings:
                    raise ValueError('Use the same voice, device, seed, and runner as prepare')
                verify_sources(root, settings)
                if args.action == 'install':
                    install(root, settings)
                else:
                    execute(root, settings, args.until)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
