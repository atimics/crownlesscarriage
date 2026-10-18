"""Run the pinned trainers with local teacher files and saved random state."""

import functools
import importlib.metadata
import json
import os
from pathlib import Path
import platform
import random
import runpy
import sys
import time


def rng_state():
    import numpy as np
    import torch
    numpy_state = np.random.get_state()
    state = dict(python=random.getstate(), numpy=[numpy_state[0], numpy_state[1].tolist(),
                 *numpy_state[2:]], torch=torch.get_rng_state())
    if torch.cuda.is_available():
        state['cuda'] = torch.cuda.get_rng_state_all()
    if torch.backends.mps.is_available():
        state['mps'] = torch.mps.get_rng_state()
    return state


def restore_rng(state):
    import numpy as np
    import torch
    if 'cc_rng' not in state:
        raise ValueError('Use a checkpoint made by this training runner')
    value = state['cc_rng']
    random.setstate(value['python'])
    name, values, *rest = value['numpy']
    np.random.set_state((name, np.asarray(values, dtype=np.uint32), *rest))
    torch.set_rng_state(value['torch'].cpu())
    if 'cuda' in value:
        torch.cuda.set_rng_state_all([item.cpu() for item in value['cuda']])
    if 'mps' in value:
        torch.mps.set_rng_state(value['mps'].cpu())


def preserve_rng(function):
    @functools.wraps(function)
    def wrapped(*args, **kwargs):
        state = rng_state()
        try:
            return function(*args, **kwargs)
        finally:
            restore_rng({'cc_rng': state})
    return wrapped


def save(value, destination):
    import torch
    if isinstance(value, dict) and 'model' in value and 'opt' in value:
        value = dict(value, cc_rng=rng_state())
    destination = Path(destination)
    temporary = destination.with_suffix(destination.suffix + '.tmp')
    torch.save(value, temporary)
    temporary.replace(destination)


def setup(root, settings):
    import numpy as np
    import torch
    import kokoro
    from kokoro.model import KModel
    device = settings['device']
    if device == 'cuda' and not torch.cuda.is_available():
        raise ValueError('Choose a machine with CUDA for this run')
    if device == 'mps' and not torch.backends.mps.is_available():
        raise ValueError('Choose an Apple GPU machine for this run')
    torch.set_num_threads(settings['recipe']['threads'])
    random.seed(settings['seed'])
    np.random.seed(settings['seed'])
    torch.manual_seed(settings['seed'])
    torch.use_deterministic_algorithms(True)
    torch.backends.cudnn.benchmark = False
    torch.backends.cudnn.allow_tf32 = False
    torch.backends.cuda.matmul.allow_tf32 = False
    packages = {item.metadata['Name']: item.version for item in importlib.metadata.distributions()}
    identity = dict(python=platform.python_version(), system=platform.system(),
                    kernel=platform.release(), machine=platform.machine(),
                    processor=platform.processor(), torch=torch.__version__, packages=packages,
                    cuda=torch.version.cuda, cudnn=torch.backends.cudnn.version(),
                    device=device, deterministic_algorithms=True)
    if device == 'cuda':
        identity['gpu'] = torch.cuda.get_device_name()
    if identity['python'] != settings['recipe']['python']:
        raise ValueError('Use the Python version pinned in the recipe')
    from run import write_json
    path = root / 'environment.json'
    if path.exists() and json.loads(path.read_text()) != identity:
        raise ValueError('The training environment changed; choose a fresh run folder')
    write_json(path, identity)
    original = kokoro.KPipeline

    class LocalPipeline(original):
        def __init__(self, *args, **kwargs):
            kwargs['lang_code'] = settings['language']
            kwargs['model'] = KModel(repo_id=settings['recipe']['teacher_repo'],
                                    config=str(root / 'teacher/config.json'),
                                    model=str(root / 'teacher/kokoro-v1_0.pth'))
            kwargs['model'] = kwargs['model'].to(kwargs.get('device', 'cpu')).eval()
            super().__init__(*args, **kwargs)

        def load_voice(self, voice, delimiter=','):
            return super().load_voice(str(root / 'teacher/voices' / (settings['voice'] + '.pt')))

    kokoro.KPipeline = LocalPipeline


def validate_export(root, settings):
    """Check the exported graphs on the held-out corpus and record CPU speed."""
    import numpy as np
    import onnx
    import onnxruntime as ort
    import soundfile as sf
    import torch
    from run import digest, write_json
    folder = root / 'training/models/paradee'
    teacher_config = json.loads((root / 'teacher/config.json').read_text())
    write_json(folder / 'config.json', dict(teacher_config, sample_rate=24000,
               voice=settings['voice'], language=settings['language']))
    shards = sorted((root / 'training/data/teacher').glob('shard_*.pt'))
    held = torch.load(shards[0], map_location='cpu')[:settings['recipe']['held_out']]
    if len(held) != settings['recipe']['held_out']:
        raise ValueError('The corpus needs all 200 held-out records')
    result = dict(voice=settings['voice'], held_out=len(held), sample_rate=24000,
                  timing_basis='ONNX inference on stored phonemes; one CPU thread', models={})
    for name in ('paradee.onnx', 'paradee_int8.onnx'):
        path = folder / name
        onnx.checker.check_model(str(path))
        options = ort.SessionOptions()
        options.intra_op_num_threads = 1
        options.inter_op_num_threads = 1
        session = ort.InferenceSession(str(path), options, providers=['CPUExecutionProvider'])
        samples = held if name == 'paradee_int8.onnx' else held[:4]
        elapsed = audio_seconds = 0
        for index, row in enumerate(samples):
            feed = dict(input_ids=row['ids'][None].numpy(), speed=np.ones(1, np.float32))
            start = time.perf_counter()
            audio = session.run(None, feed)[0]
            elapsed += time.perf_counter() - start
            if audio.ndim != 2 or audio.shape[0] != 1 or not np.isfinite(audio).all():
                raise ValueError('The exported model returned invalid audio')
            duration = audio.shape[1] / 24000
            if not 0.15 <= duration <= 25 or np.abs(audio).max() < 0.001:
                raise ValueError('The exported audio needs a valid duration and audible speech')
            audio_seconds += duration
            if index < 8:
                sf.write(folder / f'held_{index + 1}_{path.stem}.wav', audio[0], 24000)
        result['models'][name] = dict(sha256=digest(path), bytes=path.stat().st_size,
                                     sentences=len(samples), audio_seconds=audio_seconds,
                                     compute_seconds=elapsed, realtime_factor=audio_seconds / elapsed)
    write_json(folder / 'validation.json', result)


def main():
    root = Path(sys.argv[1]).resolve()
    script = sys.argv[2]
    settings = json.loads((root / 'run.json').read_text())
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    sys.path.insert(0, str(root / 'training/scripts'))
    setup(root, settings)
    sys.argv = [str(root / 'training/scripts' / script), *sys.argv[3:]]
    runpy.run_path(sys.argv[0], run_name='__main__', init_globals={
        'cc_save': save, 'cc_restore_rng': restore_rng, 'cc_preserve_rng': preserve_rng})
    if script == 'export_paradee.py':
        validate_export(root, settings)


if __name__ == '__main__':
    main()
